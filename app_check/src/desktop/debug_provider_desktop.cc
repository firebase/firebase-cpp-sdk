// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "app_check/src/desktop/debug_provider_desktop.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "app/rest/response.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/log.h"
#include "app/src/scheduler.h"
#include "app/src/uuid.h"
#include "app_check/src/desktop/debug_token_request.h"
#include "app_check/src/desktop/token_response.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase::app_check::internal {
namespace {
std::string GenerateUuidString() {
  firebase::internal::Uuid uuid;
  uuid.Generate();
  char s[37];
  snprintf(
      s, sizeof(s),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid.data[0], uuid.data[1], uuid.data[2], uuid.data[3], uuid.data[4],
      uuid.data[5], uuid.data[6], uuid.data[7], uuid.data[8], uuid.data[9],
      uuid.data[10], uuid.data[11], uuid.data[12], uuid.data[13], uuid.data[14],
      uuid.data[15]);
  return s;
}

int64_t CurrentTimeMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

struct ScopedCompletionCallback {
  std::function<void(AppCheckToken, int, const std::string&)> callback;
  std::atomic<bool> invoked{false};

  explicit ScopedCompletionCallback(
      std::function<void(AppCheckToken, int, const std::string&)> cb)
      : callback(std::move(cb)) {}

  void Invoke(const AppCheckToken& token, int error_code,
              const std::string& error_message) {
    if (!invoked.exchange(true) && callback) {
      callback(token, error_code, error_message);
    }
  }
  ~ScopedCompletionCallback() {
    Invoke(AppCheckToken{}, kAppCheckErrorUnknown, "Provider destroyed");
  }
};
}  // namespace

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  struct CleanupState {
    std::mutex mutex;
    DebugAppCheckProviderFactoryInternal* factory{nullptr};
    App* app{nullptr};
    flatbuffers::unique_ptr<firebase::rest::Controller> controller;
  };

  DebugAppCheckProvider(App* app, DebugAppCheckProviderFactoryInternal* factory,
                        const std::string& token)
      : app_(app),
        factory_(factory),
        cleanup_state_(std::make_shared<CleanupState>()),
        alive_(std::make_shared<std::atomic<bool>>(true)),
        debug_token_(token) {
    cleanup_state_->factory = factory;
    cleanup_state_->app = app;
    firebase::rest::util::Initialize();
    firebase::rest::InitTransportCurl();
  }

  ~DebugAppCheckProvider() override {
    DetachCleanupState();
    alive_->store(false);
    {
      std::lock_guard<std::mutex> lock(cleanup_state_->mutex);
      if (cleanup_state_->controller) cleanup_state_->controller->Cancel();
    }
    scheduler_.CancelAllAndShutdownWorkerThread();
    firebase::rest::CleanupTransportCurl();
    firebase::rest::util::Terminate();
  }

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override {
    GetTokenInternal(false, std::move(completion_callback));
  }
  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) override {
    GetTokenInternal(true, std::move(completion_callback));
  }

  void SetDebugToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(token_mutex_);
    debug_token_ = token;
  }

  const std::shared_ptr<CleanupState>& cleanup_state() const {
    return cleanup_state_;
  }
  void DetachCleanupState() {
    if (!cleanup_state_) return;
    std::lock_guard<std::mutex> lock(cleanup_state_->mutex);
    cleanup_state_->factory = nullptr;
    cleanup_state_->app = nullptr;
  }

 private:
  void GetTokenInternal(
      bool limited_use,
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback);

  App* app_{nullptr};
  DebugAppCheckProviderFactoryInternal* factory_{nullptr};
  std::shared_ptr<CleanupState> cleanup_state_;
  std::shared_ptr<std::atomic<bool>> alive_;
  std::mutex token_mutex_;
  bool logged_token_{false};
  std::string debug_token_;
  scheduler::Scheduler scheduler_;
};

void GetTokenAsync(
    std::shared_ptr<std::atomic<bool>> alive,
    std::shared_ptr<DebugTokenRequest> request,
    std::shared_ptr<DebugAppCheckProvider::CleanupState> cleanup_state,
    std::function<void(AppCheckToken, int, const std::string&)> cb) {
  if (!alive || !alive->load()) {
    if (cb) cb(AppCheckToken{}, kAppCheckErrorUnknown, "Provider destroyed");
    return;
  }
  TokenResponse response;
  firebase::rest::CreateTransport()->Perform(*request, &response,
                                             &cleanup_state->controller);
  if (!alive->load()) {
    if (cb) cb(AppCheckToken{}, kAppCheckErrorUnknown, "Provider destroyed");
    return;
  }

  if (response.status() == firebase::rest::util::HttpSuccess) {
    AppCheckToken token;
    token.token = response.token();
    errno = 0;
    char* end_ptr = nullptr;
    const std::string& ttl_str = response.ttl();
    long long parsed_ttl = std::strtoll(ttl_str.c_str(), &end_ptr, 10);
    int64_t now_millis = CurrentTimeMillis();
    int64_t max_extra =
        (std::numeric_limits<int64_t>::max() - now_millis) / 1000;
    int64_t extra = (errno == 0 && end_ptr != ttl_str.c_str() && parsed_ttl > 0)
                        ? std::min<int64_t>(parsed_ttl, max_extra)
                        : 0;
    token.expire_time_millis = now_millis + extra * 1000;
    if (cb) cb(token, kAppCheckErrorNone, "");
  } else if (cb) {
    std::string msg = "The server responded with an error.\nHTTP status code: " +
                      std::to_string(response.status()) + " \nResponse body: " +
                      (response.GetBody() ? response.GetBody() : "") + "\n";
    int code = response.status() == 429 ? kAppCheckErrorTooManyRequests
               : response.status() == 0 ? kAppCheckErrorServerUnreachable
                                        : kAppCheckErrorUnknown;
    cb(AppCheckToken{}, code, msg);
  }
}

void DebugAppCheckProvider::GetTokenInternal(
    bool limited_use,
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  if (!app_) {
    if (completion_callback) {
      completion_callback(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                          "App is null.");
    }
    return;
  }

  std::string message_to_log;
  std::string debug_token_copy;
  {
    std::lock_guard<std::mutex> lock(token_mutex_);
    if (debug_token_.empty()) {
      const char* env_token = std::getenv("APP_CHECK_DEBUG_TOKEN");
      debug_token_ = (env_token && env_token[0] != '\0') ? env_token
                                                         : GenerateUuidString();
    }
    debug_token_copy = debug_token_;
    if (!logged_token_ && !debug_token_.empty()) {
      logged_token_ = true;
      std::string app_id =
          app_->options().app_id() ? app_->options().app_id() : "";
      std::string project_id =
          app_->options().project_id() ? app_->options().project_id() : "";
      message_to_log =
          "\nWARNING: Firebase App Check debug token: " + debug_token_ +
          "\n\nTo use this token for app debugging, register it with your "
          "project in the Firebase Console:\n"
          "https://console.firebase.google.com/project/" +
          project_id + "/appcheck/apps?selectedAppId=" + app_id +
          "\nOr using the Firebase CLI:\nfirebase appcheck:debugtokens:create " +
          debug_token_ + " --app " + app_id + "\n";
    }
  }
  if (!message_to_log.empty()) firebase::LogWarning("%s", message_to_log.c_str());

  auto request = std::make_shared<DebugTokenRequest>(app_);
  request->SetDebugToken(std::move(debug_token_copy));
  request->SetLimitedUse(limited_use);

  auto scoped_cb = std::make_shared<ScopedCompletionCallback>(
      std::move(completion_callback));
  std::function<void(AppCheckToken, int, const std::string&)> wrapped =
      [scoped_cb](AppCheckToken token, int err, const std::string& msg) {
        scoped_cb->Invoke(token, err, msg);
      };
  scheduler_.Schedule(callback::NewCallback(
      GetTokenAsync, alive_, request, cleanup_state_, std::move(wrapped)));
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal() =
    default;

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  std::map<App*, DebugAppCheckProvider*> providers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, provider] : provider_map_) {
      if (provider) provider->DetachCleanupState();
    }
    providers.swap(provider_map_);
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<DebugAppCheckProvider> provider(raw_provider);
    if (auto* notifier = CleanupNotifier::FindByOwner(app);
        notifier && provider) {
      notifier->UnregisterObject(provider.get());
    }
  }
}

void DebugAppCheckProviderFactoryInternal::RemoveProvider(App* app) {
  if (!app) return;
  std::unique_ptr<DebugAppCheckProvider> to_delete;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = provider_map_.find(app); it != provider_map_.end()) {
      if (it->second) it->second->DetachCleanupState();
      to_delete.reset(it->second);
      provider_map_.erase(it);
    }
  }
  if (to_delete) {
    if (auto* notifier = CleanupNotifier::FindByOwner(app)) {
      notifier->UnregisterObject(to_delete.get());
    }
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app) return nullptr;
  std::lock_guard<std::mutex> lock(mutex_);
  if (auto it = provider_map_.find(app); it != provider_map_.end()) {
    return it->second;
  }
  auto* raw_provider = new DebugAppCheckProvider(app, this, debug_token_);
  provider_map_[app] = raw_provider;
  if (auto* notifier = CleanupNotifier::FindByOwner(app)) {
    notifier->RegisterObject(raw_provider, [](void* object) {
      auto state = static_cast<DebugAppCheckProvider*>(object)->cleanup_state();
      DebugAppCheckProviderFactoryInternal* factory = nullptr;
      App* owner_app = nullptr;
      {
        std::lock_guard<std::mutex> state_lock(state->mutex);
        factory = std::exchange(state->factory, nullptr);
        owner_app = std::exchange(state->app, nullptr);
      }
      if (factory && owner_app) factory->RemoveProvider(owner_app);
    });
  }
  return raw_provider;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(const char* token) {
  std::lock_guard<std::mutex> lock(mutex_);
  debug_token_ = token ? token : "";
  for (auto& [_, provider] : provider_map_) {
    if (provider) provider->SetDebugToken(debug_token_);
  }
}

}  // namespace firebase::app_check::internal
