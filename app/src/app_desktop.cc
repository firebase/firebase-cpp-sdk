/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/src/app_desktop.h"

#include <string.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <windows.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {
DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {

static Mutex* GetDefaultConfigPathMutex() {
  static Mutex* mutex = new Mutex();
  return mutex;
}

static std::string& GetDefaultConfigPath() {
  static std::string* default_config_path = new std::string();
  return *default_config_path;
}

static std::string GetDefaultConfigPathCopy() {
  MutexLock lock(*GetDefaultConfigPathMutex());
  return GetDefaultConfigPath();
}

// Attempts to load a config file from the path specified, and use it to
// populate the AppOptions pointer.  Returns true on success, false otherwise.
bool LoadAppOptionsFromJsonConfigFile(const char* path, AppOptions* options) {
  if (!path) return false;
  // Size is arbitrary, just making sure that there is a sane limit.
  static const std::streamoff kMaxBuffersize = 1024 * 500;

#if FIREBASE_PLATFORM_WINDOWS
  // Convert the path from UTF-8 to UTF-16 before opening on Windows.
  std::wstring path_utf16;
  if (path[0] != '\0') {
    int wide_len =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    if (wide_len <= 0) {
      LogError("Can't convert path '%s' to UTF-16.", path);
      return false;
    }
    path_utf16.resize(static_cast<size_t>(wide_len));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                            &path_utf16[0], wide_len) <= 0) {
      LogError("Can't convert path '%s' to UTF-16.", path);
      return false;
    }
    path_utf16.pop_back();
  }
  std::ifstream infile(path_utf16.c_str(), std::ifstream::binary);
#else
  std::ifstream infile(path, std::ifstream::binary);
#endif  // FIREBASE_PLATFORM_WINDOWS
  if (!infile) return false;

  infile.seekg(0, infile.end);
  std::streamoff file_length = infile.tellg();
  // Make sure our seeks/tells succeeded and the file is a sane size:
  if (file_length < 0 || file_length > kMaxBuffersize) return false;
  infile.seekg(0, infile.beg);
  if (infile.fail()) return false;

  std::string buffer(static_cast<size_t>(file_length), '\0');
  if (file_length > 0) {
    infile.read(&buffer[0], file_length);
    if (infile.fail()) return false;
  }

  AppOptions temp_options;
  AppOptions* target_options = options ? options : &temp_options;
  return AppOptions::LoadFromJsonConfig(buffer.c_str(), target_options) !=
         nullptr;
}

}  // namespace internal

// Searches internal::g_default_config_path for filenames matching
// kDefaultGoogleServicesNames attempting to load the app options from each
// config file in turn.
AppOptions* AppOptions::LoadDefault(AppOptions* options) {
  static const char* kDefaultGoogleServicesNames[] = {
      "google-services-desktop.json", "google-services.json"};
  std::string default_config_path = internal::GetDefaultConfigPathCopy();
  std::string config_files;
  size_t number_of_config_filenames =
      FIREBASE_ARRAYSIZE(kDefaultGoogleServicesNames);
  for (size_t i = 0; i < number_of_config_filenames; i++) {
    std::string full_path =
        default_config_path + kDefaultGoogleServicesNames[i];
    AppOptions temp_options = options ? *options : AppOptions();
    if (internal::LoadAppOptionsFromJsonConfigFile(full_path.c_str(),
                                                   &temp_options)) {
      if (!options) {
        options = new AppOptions(temp_options);
      } else {
        *options = temp_options;
      }
      return options;
    }
    config_files += full_path;
    if (i < number_of_config_filenames - 1) config_files += ", ";
  }
  LogError(
      "Unable to load Firebase app options ([%s] are missing or "
      "malformed)",
      config_files.c_str());
  return nullptr;
}

namespace {

// A HeartbeatController together with the DateProvider and Logger it holds
// references to, so they share one lifetime.
struct HeartbeatControllerBundle {
  explicit HeartbeatControllerBundle(const std::string& app_id)
      : controller(app_id, logger, date_provider) {}

  heartbeat::DateProviderImpl date_provider;
  SystemLogger system_logger;
  Logger logger{&system_logger, kLogLevelVerbose};
  heartbeat::HeartbeatController controller;  // Must be declared last.
};

std::shared_ptr<heartbeat::HeartbeatController> CreateHeartbeatController(
    const std::string& app_id) {
  auto bundle = std::make_shared<HeartbeatControllerBundle>(app_id);
  // Aliasing constructor: the returned pointer keeps the whole bundle alive.
  return std::shared_ptr<heartbeat::HeartbeatController>(bundle,
                                                         &bundle->controller);
}

}  // namespace

void App::Initialize() { internal_ = new internal::AppInternal(); }

App::~App() {
  if (!name_.empty()) {
    app_common::RemoveApp(this);
  }
  delete internal_;
  internal_ = nullptr;
}

// On desktop, if you create an app with no arguments, it will try to
// load any data it can find from the google-services-desktop.json
// file, or the google-services.json file, in that order.
App* App::Create() {
  AppOptions options;
  return AppOptions::LoadDefault(&options) ? Create(options) : nullptr;
}

App* App::Create(const AppOptions& options) {  // NOLINT
  return Create(options, kDefaultAppName);
}

App* App::Create(const AppOptions& options, const char* name) {  // NOLINT
  if (!name || name[0] == '\0') {
    LogError("App::Create: App name cannot be null or empty.");
    return nullptr;
  }
  App* app = GetInstance(name);
  if (app) {
    LogError("App %s already created, options will not be applied.", name);
    return app;
  }
  LogDebug("Creating Firebase App %s for %s", name, kFirebaseVersionString);

  AppOptions options_with_defaults = options;
  if (options_with_defaults.PopulateRequiredWithDefaults()) {
    // Register C++/Unity user-agents
    app_common::RegisterSdkUsage(nullptr);

    app = new App();
    app->name_ = name;
    app->options_ = options_with_defaults;
    // Fully initialize the App before AddApp() publishes it to other threads
    // and to AppCallback::NotifyAllAppCreated().
    const char* package_name = app->options_.package_name();
    app->internal_->heartbeat_controller_ = CreateHeartbeatController(
        (package_name && package_name[0] != '\0')
            ? (std::string(package_name) + "." + app->name_)
            : app->name_);
    App* added_app = app_common::AddApp(app, &app->init_results_);
    if (added_app != app) {
      app->name_.clear();
      delete app;
      return added_app ? added_app : GetInstance(name);
    }
#ifndef SWIG
    // Log a heartbeat after creating an App. In the Unity SDK this will happen
    // at a later time, after additional user agents have been registered.
    app->LogHeartbeat();
#endif  // SWIG
  }
  return app;
}

App* App::GetInstance() {  // NOLINT
  return app_common::GetDefaultApp();
}

App* App::GetInstance(const char* name) {  // NOLINT
  return app_common::FindAppByName(name);
}

std::vector<App*> App::GetApps() { return app_common::GetAllApps(); }

#ifdef INTERNAL_EXPERIMENTAL
internal::FunctionRegistry* App::function_registry() {
  return internal_ ? &internal_->function_registry : nullptr;
}
#endif  // INTERNAL_EXPERIMENTAL

void App::RegisterLibrary(const char* library, const char* version,
                          void* /* platform_resource */) {
  app_common::RegisterLibrary(library, version);
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }

void App::SetDefaultConfigPath(const char* path) {
  MutexLock lock(*internal::GetDefaultConfigPathMutex());
  std::string& default_config_path = internal::GetDefaultConfigPath();
  default_config_path = path ? path : "";

#if FIREBASE_PLATFORM_WINDOWS
  const char kSeparator = '\\';
#else
  const char kSeparator = '/';
#endif

  if (!default_config_path.empty()) {
    char last_character = default_config_path.back();
    if (last_character != '\\' && last_character != '/') {
      default_config_path += kSeparator;
    }
  }
}

void App::LogHeartbeat() const {
  if (internal_ != nullptr && internal_->heartbeat_controller_) {
    internal_->heartbeat_controller_->LogHeartbeat();
  }
}

std::shared_ptr<heartbeat::HeartbeatController> App::GetHeartbeatController()
    const {
  if (internal_ != nullptr) {
    return internal_->heartbeat_controller_;
  }
  return std::shared_ptr<heartbeat::HeartbeatController>();
}

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  if (internal_ != nullptr) {
    internal_->data_collection_default_enabled_.store(enabled);
  }
}

bool App::IsDataCollectionDefaultEnabled() const {
  if (internal_ != nullptr) {
    return internal_->data_collection_default_enabled_.load();
  }
  return true;
}

}  // namespace firebase
