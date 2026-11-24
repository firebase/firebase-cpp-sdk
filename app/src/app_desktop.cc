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

#include <codecvt>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

#include "app/src/app_common.h"
#include "app/src/app_state.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"
#include "app/src/semaphore.h"
#include "app/src/util.h"

namespace firebase {
DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {

namespace {

std::string& DefaultConfigPath() {
  static NoDestructor<std::string> path;
  return *path;
}

}  // namespace

void AppState::DestroyPlatformData() {
  delete platform_data_;
  platform_data_ = nullptr;
}

// Attempts to load a config file from the path specified, and use it to
// populate the AppOptions pointer.  Returns true on success, false otherwise.
bool LoadAppOptionsFromJsonConfigFile(const char* path, AppOptions* options) {
  static const int kMaxBuffersize = 1024 * 500;

  bool loaded_options = false;
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring path_utf16;
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>
      utf16_converter;
  try {
    path_utf16 = utf16_converter.from_bytes(path);
  } catch (std::range_error& ex) {
    LogError("Can't convert path '%s' to UTF-16: %s", path, ex.what());
    return false;
  }
  std::ifstream infile(path_utf16.c_str(), std::ifstream::binary);
#else
  std::ifstream infile(path, std::ifstream::binary);
#endif  // FIREBASE_PLATFORM_WINDOWS
  if (infile) {
    infile.seekg(0, infile.end);
    int file_length = static_cast<int>(infile.tellg());
    if (file_length > kMaxBuffersize) return false;
    infile.seekg(0, infile.beg);

    if (file_length == -1) return false;
    if (infile.fail()) return false;

    std::string buffer(static_cast<size_t>(file_length), '\0');
    infile.read(&buffer[0], file_length);
    if (infile.fail()) return false;

    loaded_options =
        AppOptions::LoadFromJsonConfig(buffer.c_str(), options) != nullptr;
  }
  return loaded_options;
}

}  // namespace internal

AppOptions* AppOptions::LoadDefault(AppOptions* options) {
  static const char* kDefaultGoogleServicesNames[] = {
      "google-services-desktop.json", "google-services.json"};
  bool allocated_options;
  if (options) {
    allocated_options = false;
  } else {
    options = new AppOptions();
    allocated_options = true;
  }
  std::string config_files;
  size_t number_of_config_filenames =
      FIREBASE_ARRAYSIZE(kDefaultGoogleServicesNames);
  for (size_t i = 0; i < number_of_config_filenames; i++) {
    std::string full_path =
        internal::DefaultConfigPath() + kDefaultGoogleServicesNames[i];
    if (internal::LoadAppOptionsFromJsonConfigFile(full_path.c_str(),
                                                   options)) {
      return options;
    }
    config_files += full_path;
    if (i < number_of_config_filenames - 1) config_files += ", ";
  }
  if (allocated_options) delete options;
  LogError(
      "Unable to load Firebase app options ([%s] are missing or "
      "malformed)",
      config_files.c_str());
  return nullptr;
}

App App::Create() {
  AppOptions options;
  return AppOptions::LoadDefault(&options) ? Create(options) : App();
}

App App::Create(const AppOptions& options) {
  return Create(options, kDefaultAppName);
}

App App::Create(const AppOptions& options, std::string_view name) {
  std::string app_name(name);
  LogDebug("Creating Firebase App %s for %s", app_name.c_str(),
           kFirebaseVersionString);
  LogDebug("Validating semaphore creation.");
  { firebase::Semaphore sem_test(0); }

  AppOptions options_with_defaults = options;
  if (!options_with_defaults.PopulateRequiredWithDefaults()) {
    return App();
  }

  // Register C++/Unity user-agents.
  app_common::RegisterSdkUsage(nullptr);

  auto state = std::make_shared<internal::AppState>(
      std::move(app_name), std::move(options_with_defaults));

  auto* platform = new internal::AppInternal();
  std::string unique_name =
      std::string(state->options().package_name()) + "." + state->name();
  platform->heartbeat_controller_ =
      std::make_shared<heartbeat::HeartbeatController>(
          unique_name, state->logger(), platform->date_provider_);
  state->set_platform_data(platform);

  App app = state->MakeAppHandle();
  AppCallback::NotifyAllAppCreated(app, &state->mutable_init_results());

#ifndef SWIG
  // Log a heartbeat after creating an App. In the Unity SDK this will happen
  // at a later time, after additional user agents have been registered.
  platform->heartbeat_controller_->LogHeartbeat();
#endif  // SWIG

  return app;
}

App App::CreateRegistered() {
  AppOptions options;
  return AppOptions::LoadDefault(&options) ? CreateRegistered(options) : App();
}

App App::CreateRegistered(const AppOptions& options) {
  return CreateRegistered(options, kDefaultAppName);
}

App App::CreateRegistered(const AppOptions& options, std::string_view name) {
  if (auto existing = GetInstance(name)) {
    LogError("App %.*s already registered, options will not be applied.",
             static_cast<int>(name.size()), name.data());
    return App();
  }
  App app = Create(options, name);
  if (!app || !app.Register()) {
    return App();
  }
  return app;
}

void App::RegisterLibrary(const char* library, const char* version,
                          void* /* platform_resource */) {
  app_common::RegisterLibrary(library, version);
}

void App::SetDefaultConfigPath(const char* path) {
  std::string& config_path = internal::DefaultConfigPath();
  config_path = path ? path : "";

#if defined(WIN32)
  const char kSeparator = '\\';
#else
  const char kSeparator = '/';
#endif

  if (!config_path.empty()) {
    char last_character = config_path.back();
    if (last_character != '\\' && last_character != '/') {
      config_path += kSeparator;
    }
  }
}

void App::LogHeartbeat() const {
  if (state_ && state_->platform_data() &&
      state_->platform_data()->heartbeat_controller_) {
    state_->platform_data()->heartbeat_controller_->LogHeartbeat();
  }
}

std::shared_ptr<heartbeat::HeartbeatController> App::GetHeartbeatController()
    const {
  if (state_ && state_->platform_data() &&
      state_->platform_data()->heartbeat_controller_) {
    // Alias to AppState so the HeartbeatController keeps date_provider_ and
    // logger_ alive even if the caller drops its App handle.
    return std::shared_ptr<heartbeat::HeartbeatController>(
        state_, state_->platform_data()->heartbeat_controller_.get());
  }
  return {};
}

void App::SetDataCollectionDefaultEnabled(bool /* enabled */) {}

bool App::IsDataCollectionDefaultEnabled() const { return true; }

}  // namespace firebase
