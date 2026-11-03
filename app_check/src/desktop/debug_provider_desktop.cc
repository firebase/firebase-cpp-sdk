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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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

std::string JoinPath(const std::string& base, const std::string& relative) {
  if (base.empty()) {
    return "";
  }
  std::string result = base;
  while (!result.empty() && (result.back() == '/' || result.back() == '\\')) {
    result.pop_back();
  }
  result.push_back('/');
  result.append(relative);
  return result;
}

const char* GetNonEmptyEnv(const char* name) {
  const char* val = std::getenv(name);
  if (val != nullptr && val[0] != '\0') {
    return val;
  }
  return nullptr;
}

bool g_has_simulate_release_override = false;
bool g_simulate_release_build = false;

bool IsReleaseBuild() {
  if (g_has_simulate_release_override) {
    return g_simulate_release_build;
  }
#if defined(NDEBUG)
  return true;
#else
  return false;
#endif
}

}  // namespace

void SetSimulateReleaseBuildForTesting(bool simulate_release) {
  g_has_simulate_release_override = true;
  g_simulate_release_build = simulate_release;
}

void ResetSimulateReleaseBuildForTesting() {
  g_has_simulate_release_override = false;
  g_simulate_release_build = false;
}

std::string ResolveConfigstorePath() {
  if (const char* xdg_config_home = GetNonEmptyEnv("XDG_CONFIG_HOME")) {
    return JoinPath(xdg_config_home, "configstore/firebase-appcheck.json");
  }
#if FIREBASE_PLATFORM_WINDOWS
  if (const char* user_profile = GetNonEmptyEnv("USERPROFILE")) {
    return JoinPath(user_profile, ".config/configstore/firebase-appcheck.json");
  }
#endif  // FIREBASE_PLATFORM_WINDOWS
  if (const char* home = GetNonEmptyEnv("HOME")) {
    return JoinPath(home, ".config/configstore/firebase-appcheck.json");
  }
  return "";
}

std::string ResolveDebugTokenFromConfig(const std::string& config_path,
                                        const std::string& app_id) {
  if (config_path.empty() || app_id.empty()) {
    return "";
  }

  static const std::streamoff kMaxConfigFileSize = 1024 * 500;
  errno = 0;
  std::ifstream infile(config_path.c_str(), std::ifstream::binary);
  if (!infile.is_open()) {
    if (errno != 0 && errno != ENOENT) {
      firebase::LogWarning(
          "[FirebaseAppCheck] Failed to open %s (errno=%d).",
          config_path.c_str(), errno);
    }
    return "";
  }

  infile.seekg(0, std::ios::end);
  std::streamoff file_length = infile.tellg();
  if (file_length == 0) {
    return "";
  }
  if (file_length < 0 || file_length > kMaxConfigFileSize) {
    firebase::LogWarning("[FirebaseAppCheck] Failed to read %s.",
                         config_path.c_str());
    return "";
  }
  infile.seekg(0, std::ios::beg);
  if (infile.fail()) {
    firebase::LogWarning("[FirebaseAppCheck] Failed to read %s.",
                         config_path.c_str());
    return "";
  }

  std::string json_str(static_cast<size_t>(file_length), '\0');
  infile.read(&json_str[0], file_length);
  if (infile.fail()) {
    firebase::LogWarning("[FirebaseAppCheck] Failed to read %s.",
                         config_path.c_str());
    return "";
  }

  // Ensure the input is a non-empty JSON object before invoking FlexBuffers
  // parser, as FlexBuffers GetRoot() assumes a non-empty buffer.
  size_t first_non_space = json_str.find_first_not_of(" \t\r\n");
  if (first_non_space == std::string::npos) {
    return "";
  }
  if (json_str[first_non_space] != '{') {
    firebase::LogWarning("[FirebaseAppCheck] Failed to parse JSON in %s.",
                         config_path.c_str());
    return "";
  }

  Variant root = firebase::util::JsonToVariant(json_str.c_str());
  if (!root.is_map()) {
    firebase::LogWarning("[FirebaseAppCheck] Failed to parse JSON in %s.",
                         config_path.c_str());
    return "";
  }

  const std::map<Variant, Variant>& root_map = root.map();
  auto tokens_it = root_map.find(Variant("tokens"));
  if (tokens_it == root_map.end() || !tokens_it->second.is_map()) {
    return "";
  }

  const std::map<Variant, Variant>& tokens_map = tokens_it->second.map();
  auto app_it = tokens_map.find(Variant(app_id));
  if (app_it == tokens_map.end()) {
    if (!tokens_map.empty()) {
      firebase::LogInfo(
          "[FirebaseAppCheck] Config file %s exists, but contains no debug "
          "token for app ID '%s'.",
          config_path.c_str(), app_id.c_str());
    }
    return "";
  }
  if (!app_it->second.is_string()) {
    firebase::LogWarning(
        "[FirebaseAppCheck] Invalid debug token entry for app ID '%s' in %s.",
        app_id.c_str(), config_path.c_str());
    return "";
  }

  const char* token_str = app_it->second.string_value();
  if (token_str == nullptr || token_str[0] == '\0') {
    return "";
  }
  return std::string(token_str);
}

DebugAppCheckProvider::DebugAppCheckProvider(
    App* app, const std::string& token,
    DebugAppCheckProviderFactoryInternal* factory)
    : app_(app),
      factory_(factory),
      scheduler_(),
      token_mutex_(),
      debug_token_(token),
      logged_token_(false) {
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

const std::string& DebugAppCheckProvider::DetermineDebugToken() {
  std::lock_guard<std::mutex> lock(token_mutex_);
  if (!logged_token_ && IsReleaseBuild()) {
    firebase::LogWarning(
        "[FirebaseAppCheck] DebugAppCheckProviderFactory is in use in a "
        "Release build (NDEBUG defined). Debug tokens are for development "
        "only; prefer a production provider where one is available.");
  }

  if (!debug_token_.empty()) {
    if (!logged_token_) {
      logged_token_ = true;
      firebase::LogInfo(
          "[FirebaseAppCheck] Debug token loaded from SetDebugToken.");
    }
    return debug_token_;
  }

  // 1. Check environment variables: FIREBASE_APPCHECK_DEBUG_TOKEN first, then
  // fallback to AppCheckDebugToken and APP_CHECK_DEBUG_TOKEN.
  const char* env_token = GetNonEmptyEnv("FIREBASE_APPCHECK_DEBUG_TOKEN");
  if (!env_token) {
    env_token = GetNonEmptyEnv("AppCheckDebugToken");
  }
  if (!env_token) {
    env_token = GetNonEmptyEnv("APP_CHECK_DEBUG_TOKEN");
  }
  if (env_token) {
    debug_token_ = env_token;
    logged_token_ = true;
    firebase::LogInfo(
        "[FirebaseAppCheck] Debug token loaded from environment variable.");
    return debug_token_;
  }

  // 2. Check local configstore JSON
  // (~/.config/configstore/firebase-appcheck.json or
  // $XDG_CONFIG_HOME/configstore/firebase-appcheck.json).
  // Compiled and consulted in both Debug and Release builds on Desktop, as the
  // Debug provider is the only App Check provider on Desktop.
  if (app_ != nullptr) {
    std::string config_path = ResolveConfigstorePath();
    std::string config_token =
        ResolveDebugTokenFromConfig(config_path, app_->options().app_id());
    if (!config_token.empty()) {
      debug_token_ = config_token;
      logged_token_ = true;
      firebase::LogInfo(
          "[FirebaseAppCheck] Debug token loaded from "
          "~/.config/configstore/firebase-appcheck.json.");
      return debug_token_;
    }
  }

  // 3. Fallback: auto-generate UUID and log local secret warning.
  debug_token_ = GenerateUuidString();
  if (!logged_token_) {
    logged_token_ = true;
    firebase::LogInfo(
        "[FirebaseAppCheck] Debug token loaded from auto-generated UUID.");
    std::string app_id = app_ ? app_->options().app_id() : "";
    std::string project_id = app_ ? app_->options().project_id() : "";
    // Split warning across multiple LogWarning calls so each call stays well
    // under the 511-character buffer limit in app/src/log.cc.
    std::string warning_part1 =
        std::string("\nWARNING: Firebase App Check debug token: ") +
        debug_token_ + "\n\n" +
        "To use this token for app debugging, register it with your "
        "project.\n\n" +
        "You can do so in the Firebase Console: \n" +
        "https://console.firebase.google.com/project/" + project_id +
        "/appcheck/apps?selectedAppId=" + app_id + " \n\n" +
        "Or using the Firebase CLI: \n" +
        "firebase appcheck:debugtokens:create " + debug_token_ + " --app " +
        app_id + "\n";
    firebase::LogWarning("%s", warning_part1.c_str());

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
        "when you finish debugging.\n");

    firebase::LogWarning(
        "%s",
        "Warning: This debug token is a secret and should not be shared or "
        "uploaded to source code.\n\n"
        "Debug Token Guide: "
        "https://firebase.google.com/docs/app-check/ios/debug-provider\n"
        "Firebase CLI install instructions: "
        "https://firebase.google.com/docs/cli\n");
  }
  return debug_token_;
}

void DebugAppCheckProvider::GetTokenInternal(
    bool limited_use,
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  const std::string& resolved_token = DetermineDebugToken();

  // Exchange debug token with the backend to get a proper attestation token.
  auto request = std::make_shared<DebugTokenRequest>(app_);
  request->SetDebugToken(resolved_token);
  request->SetLimitedUse(limited_use);

  // Use an async call, since we don't want to block on the server response.
  auto async_call =
      callback::NewCallback(GetTokenAsync, request, completion_callback);
  scheduler_.Schedule(async_call);
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : mutex_(), provider_map_(), debug_token_() {}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Clear the map and unregister from App CleanupNotifiers.
  for (auto& it : provider_map_) {
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(it.first);
    if (notifier) {
      notifier->UnregisterObject(it.second);
    }
    delete it.second;
  }
  provider_map_.clear();
}

void DebugAppCheckProviderFactoryInternal::RemoveProviderForApp(App* app) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = provider_map_.find(app);
  if (it != provider_map_.end()) {
    delete it->second;
    provider_map_.erase(it);
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  std::lock_guard<std::mutex> lock(mutex_);
  // Check the map
  std::map<App*, AppCheckProvider*>::iterator it = provider_map_.find(app);
  if (it != provider_map_.end()) {
    return it->second;
  }
  // Create a new provider and cache it
  DebugAppCheckProvider* provider =
      new DebugAppCheckProvider(app, debug_token_, this);
  provider_map_[app] = provider;
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  if (notifier) {
    notifier->RegisterObject(provider, [](void* object) {
      DebugAppCheckProvider* debug_provider =
          reinterpret_cast<DebugAppCheckProvider*>(object);
      if (debug_provider->factory()) {
        debug_provider->factory()->RemoveProviderForApp(debug_provider->app());
      }
    });
  }
  return provider;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(
    const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  debug_token_ = token;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
