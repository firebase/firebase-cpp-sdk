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

#include <map>
#include <string>
#include <utility>

#include "app/rest/response.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/log.h"
#include "app/src/scheduler.h"
#include "app_check/src/desktop/debug_token_request.h"
#include "app_check/src/desktop/token_response.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase {
namespace app_check {
namespace internal {

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(App* app, const std::string& token);
  ~DebugAppCheckProvider() override;

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override;

 private:
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
void GetTokenAsync(SharedPtr<DebugTokenRequest> request,
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
  // Identify the user's debug token
  const char* debug_token_cstr;
  if (!debug_token_.empty()) {
    debug_token_cstr = debug_token_.c_str();
  } else {
    debug_token_cstr = std::getenv("APP_CHECK_DEBUG_TOKEN");
  }

  if (!debug_token_cstr) {
    completion_callback({}, kAppCheckErrorInvalidConfiguration,
                        "Missing debug token");
    return;
  }

  // Exchange debug token with the backend to get a proper attestation token.
  auto request = MakeShared<DebugTokenRequest>(app_);
  request->SetDebugToken(debug_token_cstr);

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
