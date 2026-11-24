/*
 * Copyright 2016 Google LLC
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

#include "app/src/app_common.h"

#include <assert.h>
#include <string.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"

#if FIREBASE_PLATFORM_WINDOWS
#define strtok_r strtok_s
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {

#ifdef FIREBASE_LINUX_BUILD_CONFIG_STRING
void CheckCompilerString(const char* input) {
  FIREBASE_ASSERT_MESSAGE(
      strcmp(FIREBASE_LINUX_BUILD_CONFIG_STRING, input) == 0,
      "The compiler or stdlib library Firebase was compiled with does not "
      "match what is being used to compile this application."
      " [Lib: '%s' != Bin: '%s']",
      FIREBASE_LINUX_BUILD_CONFIG_STRING, input);
}
#endif  // FIREBASE_LINUX_BUILD_CONFIG_STRING

const char* const kDefaultAppName = "__FIRAPP_DEFAULT";

App::App(std::shared_ptr<internal::AppState> state) : state_(std::move(state)) {
#ifdef FIREBASE_LINUX_BUILD_CONFIG_STRING
  if (state_) {
    CheckCompilerString(FIREBASE_LINUX_BUILD_CONFIG_STRING);
  }
#endif  // FIREBASE_LINUX_BUILD_CONFIG_STRING
}

bool App::Register() const {
  if (!state_) return false;
  return app_common::RegisterApp(state_);
}

void App::Unregister() const {
  if (!state_) return;
  app_common::UnregisterApp(state_);
}

std::optional<App> App::GetInstance(std::string_view name) {
  auto state = app_common::FindRegisteredApp(name);
  if (!state) return std::nullopt;
  return App(std::move(state));
}

std::vector<App> App::GetApps() {
  auto states = app_common::GetRegisteredApps();
  std::vector<App> result;
  result.reserve(states.size());
  for (auto& state : states) {
    result.push_back(App(std::move(state)));
  }
  return result;
}

const char* App::name() const { return state_ ? state_->name().c_str() : ""; }

const AppOptions& App::options() const {
  if (state_) return state_->options();
  static const internal::NoDestructor<AppOptions> kEmptyOptions;
  return *kEmptyOptions;
}

#ifdef INTERNAL_EXPERIMENTAL
const std::map<std::string, InitResult>& App::init_results() const {
  if (state_) return state_->init_results();
  static const internal::NoDestructor<std::map<std::string, InitResult>>
      kEmptyResults;
  return *kEmptyResults;
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }
#endif  // INTERNAL_EXPERIMENTAL

namespace app_common {

// clang-format=off
// Detect operating system and architecture.
#if FIREBASE_PLATFORM_WINDOWS

const char* kOperatingSystem = "windows";
#if defined(_DLL) && _DLL == 1
const char* kCppRuntimeOrStl = "MD";
#else
const char* kCppRuntimeOrStl = "MT";
#endif  // defined(_MT) && _MT == 1

#if ((defined(_M_X64) && _M_X64 == 100) || \
     (defined(_M_AMD64) && _M_AMD64 == 100))
const char* kCpuArchitecture = "x86_64";
#elif defined(_M_IX86) && _M_IX86 == 600
const char* kCpuArchitecture = "x86";
#elif defined(_M_ARM64) && _M_ARM64 == 1
const char* kCpuArchitecture = "arm64";
#elif defined(_M_ARM) && _M_ARM == 7
const char* kCpuArchitecture = "arm32";
#else
#error Unknown Windows architecture.
#endif  // Architecture

#elif defined(__APPLE__)
const char* kCppRuntimeOrStl = "libcpp";
#if FIREBASE_PLATFORM_IOS
const char* kOperatingSystem = "ios";
#elif FIREBASE_PLATFORM_TVOS
const char* kOperatingSystem = "tvos";
#elif FIREBASE_PLATFORM_OSX
const char* kOperatingSystem = "darwin";
#else
#error Unknown Apple operating system.
#endif  // OS

#if __i386__
const char* kCpuArchitecture = "x86";
#elif __amd64__
const char* kCpuArchitecture = "x86_64";
#elif __aarch64__
const char* kCpuArchitecture = "arm64";
#elif __arm__
const char* kCpuArchitecture = "arm32";
#else
#error Unknown Apple architecture.
#endif  // Architecture

#elif FIREBASE_PLATFORM_ANDROID
const char* kOperatingSystem = "android";

#if __i386__
const char* kCpuArchitecture = "x86";
#elif __amd64__
const char* kCpuArchitecture = "x86_64";
#elif __aarch64__
const char* kCpuArchitecture = "arm64";
#elif __ARM_EABI__ || __arm__
const char* kCpuArchitecture = "armeabi-v7a";
#elif __mips__ || __mips
#if __mips__ == 32 || __mips == 32
const char* kCpuArchitecture = "mips";
#elif __mips__ == 64 || __mips == 64
const char* kCpuArchitecture = "mips64";
#else
#error Unknown MIPS version. __mips__
#endif  // __mips__
#else
#error Unknown Android architecture.
#endif  // Architecture

#if defined(_GLIBCXX_UTILITY)
const char* kCppRuntimeOrStl = "gnustl";
#elif defined(_LIBCPP_STD_VER)
const char* kCppRuntimeOrStl = "libcpp";
#else
#error Unknown Android STL.
#endif  // STL

#elif FIREBASE_PLATFORM_LINUX
const char* kOperatingSystem = "linux";

#if defined(_GLIBCXX_UTILITY)
const char* kCppRuntimeOrStl = "gnustl";
#elif defined(_LIBCPP_STD_VER)
const char* kCppRuntimeOrStl = "libcpp";
#else
#error Unknown Linux STL.
#endif  // STL

#if __amd64__
const char* kCpuArchitecture = "x86_64";
#elif __i386__
const char* kCpuArchitecture = "x86";
#elif __aarch64__
const char* kCpuArchitecture = "arm64";
#else
#error Unknown Linux architecture.
#endif  // Architecture
#else
#error Unknown operating system.
#endif  // Operating system

#if FIREBASE_GITHUB_ACTION_BUILD
const char* kBuildSource = "github_action_built";
#else
const char* kBuildSource = "custom_built";
#endif
// clang-format=on

const char* kApiClientHeader = "x-firebase-client";
const char* kXFirebaseGmpIdHeader = "X-Firebase-GMPID";

namespace {

// Single opt-in process-wide weak registry for App::GetInstance(name) and
// App::GetApps(). Holds only weak_ptr<AppState> so it never owns AppState,
// and expired entries are pruned lazily so ~AppState() never touches this.
struct AppRegistry {
  Mutex mutex;
  std::map<std::string, std::weak_ptr<internal::AppState>, std::less<>> apps;
};

AppRegistry& GetAppRegistry() {
  static internal::NoDestructor<AppRegistry> registry;
  return *registry;
}

class LibraryRegistry {
 public:
  bool RegisterLibrary(const char* library, const char* version) {
    std::string library_string(library);
    std::string version_string(version);
    bool changed = true;
    std::string existing_version = GetLibraryVersion(library_string);
    if (!existing_version.empty()) {
      changed = existing_version != version_string;
      if (changed) {
        LogWarning(
            "Library %s is already registered with version %s. "
            "This will be overridden with version %s.",
            library, existing_version.c_str(), version);
      }
    }
    library_to_version_[library_string] = version_string;
    return changed;
  }

  std::string GetLibraryVersion(const std::string& library) const {
    auto existing = library_to_version_.find(library);
    if (existing != library_to_version_.end()) {
      return existing->second;
    }
    return std::string();
  }

  void UpdateUserAgent() {
    std::string next_user_agent;
    for (const auto& entry : library_to_version_) {
      next_user_agent += entry.first + "/" + entry.second + " ";
    }
    if (!next_user_agent.empty()) {
      next_user_agent.pop_back();
    }
    if (user_agent_snapshots_.empty() ||
        user_agent_snapshots_.back() != next_user_agent) {
      user_agent_snapshots_.push_back(std::move(next_user_agent));
    }
    current_user_agent_.store(user_agent_snapshots_.back().c_str(),
                              std::memory_order_release);
  }

  const char* GetUserAgent() const {
    return current_user_agent_.load(std::memory_order_acquire);
  }

  bool is_common_library_registered() const {
    return is_common_library_registered_;
  }
  void set_common_library_registered(bool value) {
    is_common_library_registered_ = value;
  }

  void Clear() {
    library_to_version_.clear();
    current_user_agent_.store("", std::memory_order_release);
    is_common_library_registered_ = false;
  }

  Mutex& mutex() { return mutex_; }

 private:
  Mutex mutex_;
  std::map<std::string, std::string> library_to_version_;
  // std::deque never invalidates pointers to existing elements on push_back,
  // keeping every previously published GetUserAgent() C-string immutable and
  // race-free for concurrent readers (e.g. background heartbeat threads).
  std::deque<std::string> user_agent_snapshots_;
  std::atomic<const char*> current_user_agent_{""};
  bool is_common_library_registered_ = false;
};

LibraryRegistry& GetLibraryRegistry() {
  static internal::NoDestructor<LibraryRegistry> registry;
  return *registry;
}

}  // namespace

bool IsDefaultAppName(std::string_view name) { return name == kDefaultAppName; }

bool RegisterApp(const std::shared_ptr<internal::AppState>& state) {
  if (!state) return false;
  AppRegistry& registry = GetAppRegistry();
  MutexLock lock(registry.mutex);

  auto it = registry.apps.find(state->name());
  if (it != registry.apps.end()) {
    auto existing = it->second.lock();
    if (existing) {
      if (existing == state) {
        return true;  // Already registered (idempotent).
      }
      LogError(
          "App '%s' is already registered in the global App registry; "
          "ignoring duplicate registration.",
          state->name().c_str());
      return false;
    }
    // Previous entry expired; overwrite lazily.
    it->second = state;
    return true;
  }
  registry.apps.emplace(state->name(), state);
  return true;
}

void UnregisterApp(const std::shared_ptr<internal::AppState>& state) {
  if (!state) return;
  AppRegistry& registry = GetAppRegistry();
  MutexLock lock(registry.mutex);

  auto it = registry.apps.find(state->name());
  if (it == registry.apps.end()) return;

  auto existing = it->second.lock();
  if (!existing || existing == state) {
    registry.apps.erase(it);
  }
}

std::shared_ptr<internal::AppState> FindRegisteredApp(std::string_view name) {
  AppRegistry& registry = GetAppRegistry();
  MutexLock lock(registry.mutex);

  auto it = registry.apps.find(name);
  if (it == registry.apps.end()) return nullptr;

  auto state = it->second.lock();
  if (!state) {
    // Lazy cleanup of expired entry.
    registry.apps.erase(it);
    return nullptr;
  }
  return state;
}

std::vector<std::shared_ptr<internal::AppState>> GetRegisteredApps() {
  AppRegistry& registry = GetAppRegistry();
  MutexLock lock(registry.mutex);

  std::vector<std::shared_ptr<internal::AppState>> result;
  std::shared_ptr<internal::AppState> default_app;
  for (auto it = registry.apps.begin(); it != registry.apps.end();) {
    auto state = it->second.lock();
    if (!state) {
      it = registry.apps.erase(it);
      continue;
    }
    if (IsDefaultAppName(it->first)) {
      default_app = std::move(state);
    } else {
      result.push_back(std::move(state));
    }
    ++it;
  }
  if (default_app) {
    result.push_back(std::move(default_app));
  }
  return result;
}

void ClearRegisteredAppsForTesting() {
  AppRegistry& registry = GetAppRegistry();
  MutexLock lock(registry.mutex);
  registry.apps.clear();
}

void RegisterLibrary(const char* library, const char* version) {
  LibraryRegistry& registry = GetLibraryRegistry();
  MutexLock lock(registry.mutex());
  if (registry.RegisterLibrary(library, version)) {
    registry.UpdateUserAgent();
  }
}

void RegisterLibrariesFromUserAgent(const char* user_agent) {
  LibraryRegistry& registry = GetLibraryRegistry();
  MutexLock lock(registry.mutex());
  std::vector<char> user_agent_vector(user_agent,
                                      user_agent + strlen(user_agent) + 1);
  if (user_agent_vector.empty()) return;
  char* token = &user_agent_vector[0];
  char* next_token = nullptr;
  bool changed = false;
  do {
    static const char kFieldSeparator[] = " ";
    token = strtok_r(token, kFieldSeparator, &next_token);
    if (token) {
      static const char kLibraryVersionSeparator[] = "/";
      char* version;
      char* library = strtok_r(token, kLibraryVersionSeparator, &version);
      if (library && version) {
        changed |= registry.RegisterLibrary(library, version);
      }
    }
    token = next_token;
  } while (token && next_token && next_token[0] != '\0');
  if (changed) registry.UpdateUserAgent();
}

void RegisterSdkUsage(void* platform_resource) {
  LibraryRegistry& registry = GetLibraryRegistry();
  MutexLock lock(registry.mutex());

  if (!registry.is_common_library_registered()) {
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX,
                         FIREBASE_VERSION_NUMBER_STRING, platform_resource);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-os", kOperatingSystem,
                         platform_resource);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-arch",
                         kCpuArchitecture, platform_resource);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-stl",
                         kCppRuntimeOrStl, platform_resource);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-buildsrc",
                         kBuildSource, platform_resource);
    registry.set_common_library_registered(true);
  }
}

const char* GetUserAgent() { return GetLibraryRegistry().GetUserAgent(); }

std::string GetLibraryVersion(const char* library) {
  LibraryRegistry& registry = GetLibraryRegistry();
  MutexLock lock(registry.mutex());
  return registry.GetLibraryVersion(library);
}

void GetOuterMostSdkAndVersion(std::string* sdk, std::string* version) {
  assert(sdk);
  assert(version);
  sdk->clear();
  version->clear();

  LibraryRegistry& registry = GetLibraryRegistry();
  MutexLock lock(registry.mutex());
  static const char* kLibraryVersions[] = {
      FIREBASE_USER_AGENT_PREFIX "unity",
      FIREBASE_USER_AGENT_PREFIX "mono",
      FIREBASE_CPP_USER_AGENT_PREFIX,
  };
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kLibraryVersions); ++i) {
    std::string library(kLibraryVersions[i]);
    std::string library_version = registry.GetLibraryVersion(library);
    if (!library_version.empty()) {
      *sdk = library;
      *version = library_version;
      break;
    }
  }
}

void ResetLibraryRegistryForTesting() {
  LibraryRegistry& registry = GetLibraryRegistry();
  MutexLock lock(registry.mutex());
  registry.Clear();
}

}  // namespace app_common
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
