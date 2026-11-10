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

#include <cerrno>
#include <codecvt>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/response.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/scheduler.h"
#include "app/src/uuid.h"
#include "app/src/variant_util.h"
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

#if FIREBASE_PLATFORM_WINDOWS
// Windows paths can contain characters outside the ANSI code page, so the
// environment is read and files are opened through the UTF-16 APIs. A failed
// conversion yields an empty string instead of throwing.
using Utf8Utf16Converter =
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>;
#endif  // FIREBASE_PLATFORM_WINDOWS

std::string GetNonEmptyEnv(const char* name) {
#if FIREBASE_PLATFORM_WINDOWS
  Utf8Utf16Converter converter("", L"");
  const wchar_t* val = _wgetenv(converter.from_bytes(name).c_str());
  return val != nullptr ? converter.to_bytes(val) : "";
#else
  const char* val = std::getenv(name);
  return val != nullptr ? std::string(val) : "";
#endif  // FIREBASE_PLATFORM_WINDOWS
}

std::vector<std::string> ResolveConfigstorePaths() {
  std::vector<std::string> paths;
  // Per XDG Base Directory Specification, ignore relative paths in
  // XDG_CONFIG_HOME.
  std::string xdg = GetNonEmptyEnv("XDG_CONFIG_HOME");
  if (!xdg.empty() &&
      (xdg[0] == '/' || xdg[0] == '\\' || (xdg.size() >= 3 && xdg[1] == ':'))) {
    paths.push_back(xdg + "/configstore/firebase-appcheck.json");
  }
#if FIREBASE_PLATFORM_WINDOWS
  std::string profile = GetNonEmptyEnv("USERPROFILE");
  if (!profile.empty()) {
    paths.push_back(profile + "/.config/configstore/firebase-appcheck.json");
  }
#endif  // FIREBASE_PLATFORM_WINDOWS
  std::string home = GetNonEmptyEnv("HOME");
  if (!home.empty()) {
    paths.push_back(home + "/.config/configstore/firebase-appcheck.json");
  }
  return paths;
}

}  // namespace

std::string ResolveDebugTokenFromConfig(const std::string& config_path,
                                        const std::string& app_id) {
  if (config_path.empty() || app_id.empty()) return "";
  errno = 0;
#if FIREBASE_PLATFORM_WINDOWS
  std::ifstream infile(Utf8Utf16Converter("", L"").from_bytes(config_path),
                       std::ios::binary);
#else
  std::ifstream infile(config_path.c_str(), std::ios::binary);
#endif  // FIREBASE_PLATFORM_WINDOWS
  if (!infile.is_open()) {
    // ENOENT (file not created yet) stays silent so apps not using the CLI
    // flow do not log errors; other I/O failures are surfaced.
    if (errno != 0 && errno != ENOENT) {
      firebase::LogWarning("[FirebaseAppCheck] Failed to open %s (errno=%d).",
                           config_path.c_str(), errno);
    }
    return "";
  }
  std::string json_str((std::istreambuf_iterator<char>(infile)),
                       std::istreambuf_iterator<char>());
  size_t start = json_str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";

  Variant root = firebase::util::JsonToVariant(json_str.c_str());
  if (!root.is_map()) {
    firebase::LogWarning("[FirebaseAppCheck] Failed to parse JSON in %s.",
                         config_path.c_str());
    return "";
  }
  auto tokens_it = root.map().find(Variant("tokens"));
  if (tokens_it == root.map().end() || !tokens_it->second.is_map()) {
    firebase::LogWarning("[FirebaseAppCheck] Missing 'tokens' object in %s.",
                         config_path.c_str());
    return "";
  }
  auto app_it = tokens_it->second.map().find(Variant(app_id));
  if (app_it == tokens_it->second.map().end() || !app_it->second.is_string() ||
      app_it->second.string_value()[0] == '\0') {
    firebase::LogInfo(
        "[FirebaseAppCheck] %s exists, but contains no debug token for app ID "
        "'%s'.",
        config_path.c_str(), app_id.c_str());
    return "";
  }
  return app_it->second.string_value();
}

namespace {

// Resolves the debug token once at provider construction following the
// priority order:
//   1. Explicit SetDebugToken() value
//   2. Environment variable (FIREBASE_APPCHECK_DEBUG_TOKEN, AppCheckDebugToken,
//      APP_CHECK_DEBUG_TOKEN)
//   3. Local configstore (~/.config/configstore/firebase-appcheck.json)
//   4. Auto-generated UUID
std::string ResolveDebugToken(App* app, const std::string& explicit_token) {
#if defined(NDEBUG)
  firebase::LogWarning(
      "[FirebaseAppCheck] DebugAppCheckProviderFactory is in use in a "
      "Release build (NDEBUG defined). Debug tokens are for development "
      "only; prefer a production provider where one is available.");
#endif  // defined(NDEBUG)

  // 1. Explicit token passed in from SetDebugToken() at provider creation.
  if (!explicit_token.empty()) {
    firebase::LogInfo(
        "[FirebaseAppCheck] Debug token loaded from SetDebugToken.");
    return explicit_token;
  }

  // 2. Check environment variables in precedence order.
  for (const char* var : {"FIREBASE_APPCHECK_DEBUG_TOKEN", "AppCheckDebugToken",
                          "APP_CHECK_DEBUG_TOKEN"}) {
    std::string token = GetNonEmptyEnv(var);
    if (!token.empty()) {
      firebase::LogInfo(
          "[FirebaseAppCheck] Debug token loaded from environment variable "
          "%s.",
          var);
      return token;
    }
  }

  // 3. Check local configstore JSON.
  std::string app_id = app->options().app_id();
  if (!app_id.empty()) {
    for (const std::string& config_path : ResolveConfigstorePaths()) {
      std::string token = ResolveDebugTokenFromConfig(config_path, app_id);
      if (!token.empty()) {
        firebase::LogInfo("[FirebaseAppCheck] Debug token loaded from %s.",
                          config_path.c_str());
        return token;
      }
    }
  }

  // 4. Fallback: auto-generate UUID and log local secret warning.
  std::string token = GenerateUuidString();
  std::string project_id = app->options().project_id();
  // Split across multiple LogWarning calls to stay under the 512-byte
  // buffer in app/src/log.cc.
  std::string message_to_log =
      std::string("\nWARNING: Firebase App Check debug token: ") + token +
      "\n\n" +
      "To use this token for app debugging, register it with your "
      "project.\n\n" +
      "You can do so in the Firebase Console: \n" +
      "https://console.firebase.google.com/project/" + project_id +
      "/appcheck/apps?selectedAppId=" + app_id + " \n\n" +
      "Or using the Firebase CLI: \n" +
      "firebase appcheck:debugtokens:create " + token + " --app " + app_id +
      "\n\n";
  firebase::LogWarning("%s", message_to_log.c_str());
  firebase::LogWarning(
      "%s",
      "This debug token will regenerate every time the application is "
      "run.\n"
      "For more persistent methods of setting the debug token, please "
      "review the \n"
      "\"Debug & test providers\" section of the Firebase App Check "
      "documentation:\n"
      "https://firebase.google.com/docs/app-check\n\n"
      "Note: To keep your project secure, please revoke and delete this "
      "token using the \n"
      "Firebase Console or the CLI (`firebase "
      "appcheck:debugtokens:delete`) "
      "when you finish debugging.\n\n");
  firebase::LogWarning(
      "%s",
      "Warning: This debug token is a secret and should not be shared or "
      "uploaded to source code.\n\n"
      "Debug Token Guide: "
      "https://firebase.google.com/docs/app-check/ios/debug-provider\n"
      "Firebase CLI install instructions: "
      "https://firebase.google.com/docs/cli\n");
  return token;
}

}  // namespace

DebugAppCheckProvider::DebugAppCheckProvider(App* app, const std::string& token)
    : app_(app), scheduler_(), debug_token_(ResolveDebugToken(app, token)) {
  firebase::rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

DebugAppCheckProvider::~DebugAppCheckProvider() {
  scheduler_.CancelAllAndShutdownWorkerThread();
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
    if (response.status() == firebase::rest::util::HttpForbidden) {
      firebase::LogWarning(
          "[FirebaseAppCheck] Debug token was rejected by server. Run "
          "'firebase appcheck:debugtokens:create --force' to provision a new "
          "token.");
    }
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
