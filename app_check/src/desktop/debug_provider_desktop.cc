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

namespace firebase {
namespace app_check {
namespace internal {

namespace {
std::string GenerateUuidString() {
  firebase::internal::Uuid uuid;
  uuid.Generate();
  char uuid_str[37];
  snprintf(
      uuid_str, sizeof(uuid_str),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid.data[0], uuid.data[1], uuid.data[2], uuid.data[3], uuid.data[4],
      uuid.data[5], uuid.data[6], uuid.data[7], uuid.data[8], uuid.data[9],
      uuid.data[10], uuid.data[11], uuid.data[12], uuid.data[13], uuid.data[14],
      uuid.data[15]);
  return std::string(uuid_str);
}

int64_t CurrentTimeMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Ensures that a queued token completion callback is invoked with an error if
// the scheduler cancels the work item during provider destruction before
// GetTokenAsync executes.
struct ScopedCompletionCallback {
  std::function<void(AppCheckToken, int, const std::string&)> callback;
  std::atomic<bool> invoked{false};

  explicit ScopedCompletionCallback(
      std::function<void(AppCheckToken, int, const std::string&)> cb)
      : callback(std::move(cb)) {}

  void Invoke(const AppCheckToken& token, int error_code,
              const std::string& error_message) {
    if (invoked.exchange(true)) return;
    if (callback) {
      callback(token, error_code, error_message);
    }
  }

  ~ScopedCompletionCallback() {
    if (!invoked.exchange(true) && callback) {
      callback(AppCheckToken{}, kAppCheckErrorUnknown, "Provider destroyed");
    }
  }
};
}  // namespace

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  struct CleanupState {
    std::mutex mutex;
    DebugAppCheckProviderFactoryInternal* factory{nullptr};
    App* app{nullptr};
  };

  DebugAppCheckProvider(App* app, DebugAppCheckProviderFactoryInternal* factory,
                        const std::string& token);
  ~DebugAppCheckProvider() override;

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override;

  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) ;

  void SetDebugToken(const std::string& token);

  App* app() const { return app_; }
  DebugAppCheckProviderFactoryInternal* factory() const { return factory_; }
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

DebugAppCheckProvider::DebugAppCheckProvider(
    App* app, DebugAppCheckProviderFactoryInternal* factory,
    const std::string& token)
    : app_(app),
      factory_(factory),
      cleanup_state_(std::make_shared<CleanupState>()),
      alive_(std::make_shared<std::atomic<bool>>(true)),
      debug_token_(token),
      scheduler_() {
  cleanup_state_->factory = factory;
  cleanup_state_->app = app;
  firebase::rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

DebugAppCheckProvider::~DebugAppCheckProvider() {
  DetachCleanupState();
  alive_->store(false);
  scheduler_.CancelAllAndShutdownWorkerThread();
  firebase::rest::CleanupTransportCurl();
  firebase::rest::util::Terminate();
}

void DebugAppCheckProvider::SetDebugToken(const std::string& token) {
  std::lock_guard<std::mutex> lock(token_mutex_);
  debug_token_ = token;
}

// Performs the given rest request, and calls the callback based on the
// response.
void GetTokenAsync(std::shared_ptr<std::atomic<bool>> alive,
                   std::shared_ptr<DebugTokenRequest> request,
                   std::function<void(AppCheckToken, int, const std::string&)>
                       completion_callback) {
  if (!alive || !alive->load()) {
    if (completion_callback) {
      completion_callback(AppCheckToken{}, kAppCheckErrorUnknown,
                          "Provider destroyed");
    }
    return;
  }
  TokenResponse response;
  firebase::rest::CreateTransport()->Perform(*request, &response);
  if (!alive->load()) {
    if (completion_callback) {
      completion_callback(AppCheckToken{}, kAppCheckErrorUnknown,
                          "Provider destroyed");
    }
    return;
  }

  if (response.status() == firebase::rest::util::HttpSuccess) {
    // Call the callback with the response token
    AppCheckToken token;
    token.token = response.token();
    // Expected response ttl is in seconds (e.g. "3600s")
    errno = 0;
    char* end_ptr = nullptr;
    const std::string& ttl_str = response.ttl();
    long long parsed_ttl = std::strtoll(ttl_str.c_str(), &end_ptr, 10);
    int64_t now_millis = CurrentTimeMillis();
    int64_t max_extra_seconds =
        (std::numeric_limits<int64_t>::max() - now_millis) / 1000;
    int64_t extra_time = 0;
    if (errno == 0 && end_ptr != ttl_str.c_str() && parsed_ttl > 0) {
      extra_time = (parsed_ttl > max_extra_seconds)
                       ? max_extra_seconds
                       : static_cast<int64_t>(parsed_ttl);
    }
    token.expire_time_millis = now_millis + extra_time * 1000;
    if (completion_callback) {
      completion_callback(token, kAppCheckErrorNone, "");
    }
  } else {
    // Create an error message, and pass it along instead.
    AppCheckToken token;
    std::string error_message =
        "The server responded with an error.\nHTTP status code: " +
        std::to_string(response.status()) + " \nResponse body: " +
        (response.GetBody() ? response.GetBody() : "") + "\n";
    int error_code = kAppCheckErrorUnknown;
    if (response.status() == 429) {
      error_code = kAppCheckErrorTooManyRequests;
    } else if (response.status() == 0) {
      error_code = kAppCheckErrorServerUnreachable;
    }
    if (completion_callback) {
      completion_callback(token, error_code, error_message);
    }
  }
}

void DebugAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  GetTokenInternal(false, std::move(completion_callback));
}

void DebugAppCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  GetTokenInternal(true, std::move(completion_callback));
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
      if (env_token != nullptr && env_token[0] != '\0') {
        debug_token_ = env_token;
      } else {
        debug_token_ = GenerateUuidString();
      }
    }
    debug_token_copy = debug_token_;

    if (!logged_token_ && !debug_token_.empty()) {
      logged_token_ = true;
      std::string app_id =
          app_->options().app_id() ? app_->options().app_id() : "";
      std::string project_id =
          app_->options().project_id() ? app_->options().project_id() : "";
      message_to_log =
          std::string("\nWARNING: Firebase App Check debug token: ") +
          debug_token_ + "\n\n" +
          "To use this token for app debugging, register it with your "
          "project.\n\n" +
          "You can do so in the Firebase Console: \n" +
          "https://console.firebase.google.com/project/" + project_id +
          "/appcheck/apps?selectedAppId=" + app_id + " \n\n" +
          "Or using the Firebase CLI: \n" +
          "firebase appcheck:debugtokens:create " + debug_token_ + " --app " +
          app_id + "\n\n" +
          "This debug token will regenerate every time the application is "
          "run.\n" +
          "For more persistent methods of setting the debug token, please "
          "review the \n" +
          "\"Debug & test providers\" section of the Firebase App Check "
          "documentation:\n" +
          "https://firebase.google.com/docs/app-check\n\n" +
          "Note: To keep your project secure, please revoke and delete this "
          "token using the \n" +
          "Firebase Console or the CLI (`firebase "
          "appcheck:debugtokens:delete`) "
          "when you finish debugging.\n\n" +
          "Warning: This debug token is a secret and should not be shared or "
          "uploaded to source code.\n\n" +
          "Debug Token Guide: "
          "https://firebase.google.com/docs/app-check/ios/debug-provider\n" +
          "Firebase CLI install instructions: "
          "https://firebase.google.com/docs/cli\n";
    }
  }

  if (!message_to_log.empty()) {
    firebase::LogWarning("%s", message_to_log.c_str());
  }

  // Exchange debug token with the backend to get a proper attestation token.
  auto request = std::make_shared<DebugTokenRequest>(app_);
  request->SetDebugToken(std::move(debug_token_copy));
  request->SetLimitedUse(limited_use);

  // Use an async call, since we don't want to block on the server response.
  auto scoped_cb =
      std::make_shared<ScopedCompletionCallback>(std::move(completion_callback));
  std::function<void(AppCheckToken, int, const std::string&)> wrapped_callback =
      [scoped_cb](AppCheckToken token, int error_code,
                  const std::string& error_message) {
        scoped_cb->Invoke(token, error_code, error_message);
      };
  auto async_call = callback::NewCallback(GetTokenAsync, alive_, request,
                                          std::move(wrapped_callback));
  scheduler_.Schedule(async_call);
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : provider_map_(), debug_token_() {}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  std::map<App*, DebugAppCheckProvider*> providers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [app, provider] : provider_map_) {
      (void)app;
      if (provider != nullptr) {
        provider->DetachCleanupState();
      }
    }
    providers.swap(provider_map_);
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<DebugAppCheckProvider> provider(raw_provider);
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    if (notifier != nullptr && provider != nullptr) {
      notifier->UnregisterObject(provider.get());
    }
  }
}

void DebugAppCheckProviderFactoryInternal::RemoveProvider(App* app) {
  if (!app) return;
  std::unique_ptr<DebugAppCheckProvider> provider_to_delete;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = provider_map_.find(app);
    if (it != provider_map_.end()) {
      if (it->second != nullptr) {
        it->second->DetachCleanupState();
      }
      provider_to_delete.reset(it->second);
      provider_map_.erase(it);
    }
  }
  if (provider_to_delete != nullptr) {
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    if (notifier != nullptr) {
      notifier->UnregisterObject(provider_to_delete.get());
    }
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app) return nullptr;
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = provider_map_.find(app);
  if (it != provider_map_.end()) {
    return it->second;
  }
  // Create a new provider and cache it.
  auto provider =
      std::make_unique<DebugAppCheckProvider>(app, this, debug_token_);
  DebugAppCheckProvider* raw_provider = provider.release();
  provider_map_[app] = raw_provider;
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  if (notifier != nullptr) {
    std::shared_ptr<DebugAppCheckProvider::CleanupState> cleanup_state =
        raw_provider->cleanup_state();
    notifier->RegisterObject(
        raw_provider, [](void* object) {
          auto* provider = static_cast<DebugAppCheckProvider*>(object);
          std::shared_ptr<DebugAppCheckProvider::CleanupState> cleanup_state = provider->cleanup_state();
          DebugAppCheckProviderFactoryInternal* factory = nullptr;
          App* owner_app = nullptr;
          {
            std::lock_guard<std::mutex> state_lock(cleanup_state->mutex);
            factory = cleanup_state->factory;
            owner_app = cleanup_state->app;
            cleanup_state->factory = nullptr;
            cleanup_state->app = nullptr;
          }
          if (factory != nullptr && owner_app != nullptr) {
            factory->RemoveProvider(owner_app);
          }
        });
  }
  return raw_provider;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(
    const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  debug_token_ = token;
  for (auto& [app, provider] : provider_map_) {
    (void)app;
    if (provider != nullptr) {
      provider->SetDebugToken(token);
    }
  }
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
