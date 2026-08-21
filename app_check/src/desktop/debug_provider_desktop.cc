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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "app/rest/response.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
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
}  // namespace

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(App* app, const std::string& token);
  ~DebugAppCheckProvider() override;

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override;

  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) override;

 private:
  void GetTokenInternal(
      bool limited_use,
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback);

  App* app_;

  scheduler::Scheduler scheduler_;

  std::string debug_token_;
};

DebugAppCheckProvider::DebugAppCheckProvider(App* app, const std::string& token)
    : app_(app), scheduler_(), debug_token_(token) {
  firebase::rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

DebugAppCheckProvider::~DebugAppCheckProvider() {
  firebase::rest::CleanupTransportCurl();
  firebase::rest::util::Terminate();
}

// Performs the given rest request, and calls the callback based on the
// response.
void GetTokenAsync(std::shared_ptr<DebugTokenRequest> request,
                   std::function<void(AppCheckToken, int, const std::string&)>
                       completion_callback) {
  TokenResponse response;
  firebase::rest::CreateTransport()->Perform(*request, &response);

  if (response.status() == firebase::rest::util::HttpSuccess) {
    // Call the callback with the response token
    AppCheckToken token;
    token.token = std::move(response.token());
    // Expected response is in seconds
    int64_t extra_time = strtol(response.ttl().c_str(), nullptr, 10);
    token.expire_time_millis = (response.fetch_time() + extra_time) * 1000;
    completion_callback(token, kAppCheckErrorNone, "");
  } else {
    // Create an error message, and pass it along instead.
    AppCheckToken token;
    char error_message[1000];
    snprintf(error_message, sizeof(error_message),
             "The server responded with an error.\n"
             "HTTP status code: %d \n"
             "Response body: %s\n",
             response.status(), response.GetBody());
    completion_callback(token, kAppCheckErrorUnknown, error_message);
  }
}

void DebugAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  GetTokenInternal(false, completion_callback);
}

void DebugAppCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  GetTokenInternal(true, completion_callback);
}

void DebugAppCheckProvider::GetTokenInternal(
    bool limited_use,
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  static std::mutex token_mutex;
  static bool logged_token = false;
  std::string message_to_log;

  {
    std::lock_guard<std::mutex> lock(token_mutex);
    if (debug_token_.empty()) {
      if (const char* env_token = std::getenv("APP_CHECK_DEBUG_TOKEN")) {
        debug_token_ = env_token;
      } else {
        debug_token_ = GenerateUuidString();
      }
    }

    if (!logged_token && !debug_token_.empty()) {
      logged_token = true;
      std::string app_id = app_->options().app_id();
      std::string project_id = app_->options().project_id();
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
          "Note: To keep your project secure, please revoke and delete this "
          "token using the \n" +
          "Firebase Console or the CLI (`firebase "
          "appcheck:debugtokens:delete`) "
          "when you finish debugging.\n\n" +
          "Warning: This debug token is a secret and should not be shared or "
          "uploaded to source code.\n\n" +
          "Note: This debug token will regenerate every time the application is run.\n" +
          "For more persistent methods of setting the debug token, please review the \n" +
          "\"Debug & test providers\" section of the Firebase App Check documentation:\n" +
          "https://firebase.google.com/docs/app-check\n\n" +
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
  request->SetDebugToken(debug_token_);
  request->SetLimitedUse(limited_use);

  // Use an async call, since we don't want to block on the server response.
  auto async_call =
      callback::NewCallback(GetTokenAsync, request, completion_callback);
  scheduler_.Schedule(async_call);
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : provider_map_(), debug_token_() {}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  // Clear the map
  for (auto it : provider_map_) {
    delete it.second;
  }
  provider_map_.clear();
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  // Check the map
  std::map<App*, AppCheckProvider*>::iterator it = provider_map_.find(app);
  if (it != provider_map_.end()) {
    return it->second;
  }
  // Create a new provider and cache it
  AppCheckProvider* provider = new DebugAppCheckProvider(app, debug_token_);
  provider_map_[app] = provider;
  return provider;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(
    const std::string& token) {
  debug_token_ = token;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
