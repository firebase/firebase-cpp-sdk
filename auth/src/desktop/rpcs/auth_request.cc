// Copyright 2018 Google LLC
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

#include "auth/src/desktop/rpcs/auth_request.h"

#include <cstring>
#include <memory>
#include <string>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/log.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/include/firebase/auth.h"

namespace {
const int kAppCheckTokenTimeoutMs = 10000;
}  // namespace

namespace firebase {
namespace auth {

// Key name for header when sending language code data.
const char* const kHeaderFirebaseLocale = "X-Firebase-Locale";

AuthRequest::AuthRequest(::firebase::App& app, const char* schema,
                         bool deliver_heartbeat)
    : RequestJson(schema),
      app(app),
      deliver_heartbeat_(deliver_heartbeat) {}

void AuthRequest::PrepareRequestHeaders() {
  if (headers_prepared_) {
    return;
  }
  headers_prepared_ = true;

  if (deliver_heartbeat_) {
    std::shared_ptr<heartbeat::HeartbeatController> heartbeat_controller =
        app.GetHeartbeatController();
    if (heartbeat_controller) {
      std::string payload = heartbeat_controller->GetAndResetStoredHeartbeats();
      const char* gmp_app_id = app.options().app_id();
      if (!payload.empty()) {
        add_header(app_common::kApiClientHeader, payload.c_str());
        if (gmp_app_id && *gmp_app_id != '\0') {
          add_header(app_common::kXFirebaseGmpIdHeader, gmp_app_id);
        }
      }
    }
  }

  // Add AppCheck attestation token if available.
  // This is required when AppCheck enforcement is enabled on the project.
  ::firebase::internal::FunctionRegistry* registry = app.function_registry();
  if (registry) {
    Future<std::string> app_check_future;
    bool succeeded =
        registry->CallFunction(::firebase::internal::FnAppCheckGetTokenAsync,
                               &app, nullptr, &app_check_future);
    if (succeeded && app_check_future.status() != kFutureStatusInvalid) {
      const std::string* token =
          app_check_future.Await(kAppCheckTokenTimeoutMs);
      if (token && app_check_future.error() == 0 && !token->empty()) {
        add_header("X-Firebase-AppCheck", token->c_str());
      }
    }
  }
}

std::string AuthRequest::GetUrl(const char* server_url) {
  if (server_url == nullptr) {
    return "";
  }
  Auth* auth_ptr = Auth::FindAuth(&app);
  std::string assigned_emulator_url;
  if (auth_ptr != nullptr && auth_ptr->auth_data_ != nullptr &&
      auth_ptr->auth_data_->auth_impl != nullptr) {
    assigned_emulator_url =
        static_cast<AuthImpl*>(auth_ptr->auth_data_->auth_impl)
            ->GetAssignedEmulatorUrl();
  }

  if (assigned_emulator_url.empty()) {
    std::string url(kHttps);
    url += server_url;
    return url;
  } else {
    std::string url(kHttp);
    url += assigned_emulator_url;
    url += "/";
    url += server_url;
    return url;
  }
}

void AuthRequest::SetUrl(const char* api_host, const char* api_key,
                         const char* server_url) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);
  std::string url = GetUrl(server_url);
  if (api_host) {
    url.append(api_host);
  }
  url.append(UrlEncode(api_key));
  set_url(url.c_str());
}

void AuthRequest::SetIdToken(const char* const id_token) {
  if (!application_data_) return;
  if (id_token && *id_token != '\0') {
    application_data_->idToken = id_token;
    UpdatePostFields();
  } else {
    LogError("No id token given.");
  }
}

void AuthRequest::SetLocaleHeader(const char* const language_code) {
  if (!language_code || *language_code == '\0') return;
  std::string sanitized;
  sanitized.reserve(std::strlen(language_code));
  for (const char* p = language_code; *p != '\0'; ++p) {
    if (*p != '\r' && *p != '\n') {
      sanitized.push_back(*p);
    }
  }
  if (!sanitized.empty()) {
    add_header(kHeaderFirebaseLocale, sanitized.c_str());
  }
}

std::string AuthRequest::UrlEncode(const char* str) {
  if (!str) return "";
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(std::strlen(str));
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(str);
       *p != '\0'; ++p) {
    const unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      encoded.push_back(static_cast<char>(c));
    } else {
      encoded.push_back('%');
      encoded.push_back(kHex[(c >> 4) & 0x0F]);
      encoded.push_back(kHex[c & 0x0F]);
    }
  }
  return encoded;
}

}  // namespace auth
}  // namespace firebase
