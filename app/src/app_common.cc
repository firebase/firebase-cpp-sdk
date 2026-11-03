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

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>  // Used to detect STL variant.
#include <vector>

#include "app/src/assert.h"
#include "app/src/callback.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"

namespace firebase {

#ifdef FIREBASE_LINUX_BUILD_CONFIG_STRING
void CheckCompilerString(const char* input) {
  FIREBASE_ASSERT_MESSAGE(
      input != nullptr &&
          strcmp(FIREBASE_LINUX_BUILD_CONFIG_STRING, input) == 0,
      "The compiler or stdlib library Firebase was compiled with does not "
      "match what is being used to compile this application."
      " [Lib: '%s' != Bin: '%s']",
      FIREBASE_LINUX_BUILD_CONFIG_STRING, input ? input : "(null)");
}
#endif  // FIREBASE_LINUX_BUILD_CONFIG_STRING

// Default app name.
const char* const kDefaultAppName = "__FIRAPP_DEFAULT";

namespace app_common {

// clang-format=off
// Detect operating system and architecture.
#if FIREBASE_PLATFORM_WINDOWS

const char* const kOperatingSystem = "windows";
#if defined(_DLL) && _DLL == 1
const char* const kCppRuntimeOrStl = "MD";
#else
const char* const kCppRuntimeOrStl = "MT";
#endif  // defined(_DLL) && _DLL == 1

#if ((defined(_M_X64) && _M_X64 == 100) || \
     (defined(_M_AMD64) && _M_AMD64 == 100))
const char* const kCpuArchitecture = "x86_64";
#elif defined(_M_IX86) && _M_IX86 == 600
const char* const kCpuArchitecture = "x86";
#elif defined(_M_ARM64) && _M_ARM64 == 1
const char* const kCpuArchitecture = "arm64";
#elif defined(_M_ARM) && _M_ARM == 7
const char* const kCpuArchitecture = "arm32";
#else
#error Unknown Windows architecture.
#endif  // Architecture

#elif defined(__APPLE__)
const char* const kCppRuntimeOrStl = "libcpp";
#if FIREBASE_PLATFORM_IOS
const char* const kOperatingSystem = "ios";
#elif FIREBASE_PLATFORM_TVOS
const char* const kOperatingSystem = "tvos";
#elif FIREBASE_PLATFORM_OSX
const char* const kOperatingSystem = "darwin";
#else
#error Unknown Apple operating system.
#endif  // OS

#if __i386__
const char* const kCpuArchitecture = "x86";
#elif __amd64__
const char* const kCpuArchitecture = "x86_64";
#elif __aarch64__
const char* const kCpuArchitecture = "arm64";
#elif __arm__
const char* const kCpuArchitecture = "arm32";
#else
#error Unknown Apple architecture.
#endif  // Architecture

#elif FIREBASE_PLATFORM_ANDROID
const char* const kOperatingSystem = "android";

#if __i386__
const char* const kCpuArchitecture = "x86";
#elif __amd64__
const char* const kCpuArchitecture = "x86_64";
#elif __aarch64__
const char* const kCpuArchitecture = "arm64";
#elif __ARM_EABI__ || __arm__
const char* const kCpuArchitecture = "armeabi-v7a";
#elif __mips__ || __mips
#if __mips__ == 32 || __mips == 32
const char* const kCpuArchitecture = "mips";
#elif __mips__ == 64 || __mips == 64
const char* const kCpuArchitecture = "mips64";
#else
#error Unknown MIPS version. __mips__
#endif  // __mips__
#else
#error Unknown Android architecture.
#endif  // Architecture

#if defined(_STLPORT_VERSION)
const char* const kCppRuntimeOrStl = "stlport";
#elif defined(_LIBCPP_VERSION) || defined(_LIBCPP_STD_VER)
const char* const kCppRuntimeOrStl = "libcpp";
#elif defined(__GLIBCXX__) || defined(_GLIBCXX_UTILITY)
const char* const kCppRuntimeOrStl = "gnustl";
#else
#error Unknown Android STL.
#endif  // STL

#elif FIREBASE_PLATFORM_LINUX
const char* const kOperatingSystem = "linux";

#if defined(_LIBCPP_VERSION) || defined(_LIBCPP_STD_VER)
const char* const kCppRuntimeOrStl = "libcpp";
#elif defined(__GLIBCXX__) || defined(_GLIBCXX_UTILITY)
const char* const kCppRuntimeOrStl = "gnustl";
#else
#error Unknown Linux STL.
#endif  // STL

#if __amd64__ || __x86_64__
const char* const kCpuArchitecture = "x86_64";
#elif __i386__
const char* const kCpuArchitecture = "x86";
#elif __aarch64__
const char* const kCpuArchitecture = "arm64";
#elif __arm__
const char* const kCpuArchitecture = "arm32";
#else
#error Unknown Linux architecture.
#endif  // Architecture
#else
#error Unknown operating system.
#endif  // Operating system

#if FIREBASE_GITHUB_ACTION_BUILD
const char* const kBuildSource = "github_action_built";
#else
const char* const kBuildSource = "custom_built";
#endif
// clang-format=on

const char* const kApiClientHeader = "x-firebase-client";
const char* const kXFirebaseGmpIdHeader = "X-Firebase-GMPID";

SystemLogger g_system_logger;  // NOLINT

// Private cross platform data associated with an app.
struct AppData {
  // TODO(b/140528778): Remove kLogLevelVerbose here and add [GS]etLogLevel
  // member functions to app.
  AppData()
      : app(nullptr),
        logger(&g_system_logger, kLogLevelVerbose),
        function_registry(std::make_unique<internal::FunctionRegistry>()) {}

  // App associated with this data.
  App* app;
  // Notifies subscribers when the app is about to be destroyed.
  CleanupNotifier cleanup_notifier;
  // A per-app logger.
  Logger logger;
  // Per-app function registry.
  std::unique_ptr<internal::FunctionRegistry> function_registry;
};

// Tracks library registrations.
class LibraryRegistry {
 private:
  LibraryRegistry()
      : current_user_agent_(""), is_common_library_registered(false) {}

 public:
  // Register a library, returns true if the library version changed.
  bool RegisterLibrary(const char* library, const char* version) {
    if (!library || !version || library[0] == '\0' || version[0] == '\0') {
      return false;
    }
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

  // Get the version of a previously registered library.
  std::string GetLibraryVersion(const std::string& library) const {
    auto existing = library_to_version_.find(library);
    if (existing != library_to_version_.end()) {
      return existing->second;
    }
    return std::string();
  }

  // Regenerate the user agent string.
  void UpdateUserAgent() {
    std::string user_agent;
    for (auto it = library_to_version_.begin(); it != library_to_version_.end();
         ++it) {
      if (!user_agent.empty()) {
        user_agent.push_back(' ');
      }
      user_agent.append(it->first);
      user_agent.push_back('/');
      user_agent.append(it->second);
    }
    auto inserted = interned_user_agents_.insert(std::move(user_agent)).first;
    current_user_agent_ = inserted->c_str();
  }

  // Get the cached user agent string. Interned strings remain valid for the
  // process lifetime, preventing use-after-free on concurrent calls.
  const char* GetUserAgent() const { return current_user_agent_; }

 public:
  // Create the library registry singleton and get the instance.
  static LibraryRegistry* Initialize() {
    if (!library_registry_) library_registry_ = new LibraryRegistry();
    return library_registry_;
  }

  // Reset the library registry active state while keeping interned user-agent
  // strings alive so previously returned GetUserAgent() pointers never dangle.
  static void Terminate() {
    if (library_registry_) {
      library_registry_->library_to_version_.clear();
      library_registry_->current_user_agent_ = "";
      library_registry_->is_common_library_registered = false;
    }
  }

  static bool IsCommonLibraryRegistered() {
    if (library_registry_) {
      return library_registry_->is_common_library_registered;
    }
    return false;
  }

  static void SetCommonLibraryRegistered() {
    if (library_registry_) {
      library_registry_->is_common_library_registered = true;
    }
  }

 private:
  std::map<std::string, std::string> library_to_version_;
  std::set<std::string> interned_user_agents_;
  const char* current_user_agent_;
  bool is_common_library_registered;

  static LibraryRegistry* library_registry_;
};

// Guards g_apps and g_default_app.
static Mutex* g_app_mutex = new Mutex();
static std::map<std::string, std::unique_ptr<AppData>, std::less<>>* g_apps =
    nullptr;
static std::map<const App*, std::unique_ptr<internal::FunctionRegistry>>*
    g_fallback_registries = nullptr;
static App* g_default_app = nullptr;
LibraryRegistry* LibraryRegistry::library_registry_ = nullptr;

App* AddApp(App* app, std::map<std::string, InitResult>* results) {
  FIREBASE_ASSERT_RETURN(nullptr, app != nullptr && app->name() != nullptr);
  const char* app_name = app->name();
  {
    MutexLock lock(*g_app_mutex);
    if (g_apps && g_apps->find(app_name) != g_apps->end()) {
      FIREBASE_ASSERT_RETURN(nullptr, false);
    }
    if (IsDefaultAppName(app_name)) {
      assert(!g_default_app);
      g_default_app = app;
    }
    std::unique_ptr<AppData> app_data = std::make_unique<AppData>();
    app_data->app = app;
    app_data->cleanup_notifier.RegisterOwner(app);
    if (g_fallback_registries) {
      auto fallback_it = g_fallback_registries->find(app);
      if (fallback_it != g_fallback_registries->end()) {
        if (fallback_it->second) {
          app_data->function_registry = std::move(fallback_it->second);
        }
        g_fallback_registries->erase(fallback_it);
        if (g_fallback_registries->empty()) {
          delete g_fallback_registries;
          g_fallback_registries = nullptr;
        }
      }
    }
    if (!g_apps) {
      g_apps =
          new std::map<std::string, std::unique_ptr<AppData>, std::less<>>();
    }
    (*g_apps)[std::string(app_name)] = std::move(app_data);
    {
      const AppOptions& app_options = app->options();
      LogDebug(
          "Added app name=%s: options, api_key=<redacted>, app_id=%s, "
          "database_url=%s, messaging_sender_id=%s, storage_bucket=%s, "
          "project_id=%s (%p)",
          app_name, app_options.app_id(), app_options.database_url(),
          app_options.messaging_sender_id(), app_options.storage_bucket(),
          app_options.project_id(), static_cast<const void*>(app));
    }
    callback::Initialize();
  }
  AppCallback::NotifyAllAppCreated(app, results);
  return app;
}

App* FindAppByName(const char* name) {
  if (!name) return nullptr;
  MutexLock lock(*g_app_mutex);
  if (g_apps) {
    auto it = g_apps->find(name);
    if (it == g_apps->end()) return nullptr;
    return it->second->app;
  }
  return nullptr;
}

App* GetDefaultApp() {
  MutexLock lock(*g_app_mutex);
  return g_default_app;
}

App* GetAnyApp() {
  MutexLock lock(*g_app_mutex);
  if (g_default_app) {
    return g_default_app;
  }
  if (g_apps && !g_apps->empty()) {
    return g_apps->begin()->second->app;
  }
  return nullptr;
}

std::vector<App*> GetAllApps() {
  std::vector<App*> result;
  MutexLock lock(*g_app_mutex);
  if (g_apps) {
    for (auto it = g_apps->begin(); it != g_apps->end(); ++it) {
      if (it->second->app != g_default_app) result.push_back(it->second->app);
    }
    if (g_default_app) result.push_back(g_default_app);
  }
  return result;
}

void RemoveApp(App* app) {
  if (!app) return;
  std::unique_ptr<AppData> removed_app_data;
  bool removed_app = false;
  bool last_app = false;
  {
    MutexLock lock(*g_app_mutex);
    const char* app_name = app->name();
    if (g_apps && app_name) {
      auto it = g_apps->find(app_name);
      // If app initialization failed AddApp() will not be called.
      if (it != g_apps->end() && it->second->app == app) {
        removed_app = true;
        LogDebug("Deleting app %s (%p)", app_name,
                 static_cast<const void*>(app));
        removed_app_data = std::move(it->second);
        g_apps->erase(it);
        if (app == g_default_app) {
          g_default_app = nullptr;
        }
        if (g_apps->empty()) {
          last_app = true;
          delete g_apps;
          g_apps = nullptr;
        }
      }
    }
    if (removed_app_data && removed_app_data->function_registry) {
      if (!g_fallback_registries) {
        g_fallback_registries =
            new std::map<const App*,
                         std::unique_ptr<internal::FunctionRegistry>>();
      }
      (*g_fallback_registries)[app] =
          std::move(removed_app_data->function_registry);
    }
  }
  if (removed_app_data) {
    removed_app_data->cleanup_notifier.CleanupAll();
    AppCallback::NotifyAllAppDestroyed(app);
    removed_app_data.reset();
  }
  {
    MutexLock lock(*g_app_mutex);
    if (g_fallback_registries) {
      g_fallback_registries->erase(app);
      if (g_fallback_registries->empty()) {
        delete g_fallback_registries;
        g_fallback_registries = nullptr;
      }
    }
    if (last_app && (!g_apps || g_apps->empty())) {
      LibraryRegistry::Terminate();
    }
  }
  if (removed_app) {
    callback::Terminate(last_app);
  }
}

void DestroyAllApps() {
  std::vector<App*> apps_to_delete = GetAllApps();
  for (App* app : apps_to_delete) {
    // As each App is deleted, RemoveApp() is called by the destructor which
    // removes the app from the global list of apps (g_apps). When g_apps
    // is empty it is deleted by RemoveApp().
    delete app;
  }
}

// Determine whether the specified app name refers to the default app.
bool IsDefaultAppName(const char* name) {
  return name != nullptr && strcmp(kDefaultAppName, name) == 0;
}

void RegisterLibrary(const char* library, const char* version) {
  if (!library || !version) return;
  MutexLock lock(*g_app_mutex);
  LibraryRegistry* registry = LibraryRegistry::Initialize();
  if (registry->RegisterLibrary(library, version)) {
    registry->UpdateUserAgent();
  }
}

void RegisterLibrariesFromUserAgent(const char* user_agent) {
  if (!user_agent || user_agent[0] == '\0') return;
  MutexLock lock(*g_app_mutex);
  LibraryRegistry* registry = LibraryRegistry::Initialize();
  bool changed = false;
  const char* cursor = user_agent;
  while (*cursor != '\0') {
    while (*cursor == ' ') ++cursor;
    if (*cursor == '\0') break;
    const char* token_end = cursor;
    while (*token_end != '\0' && *token_end != ' ') ++token_end;
    const char* slash = nullptr;
    for (const char* p = cursor; p < token_end; ++p) {
      if (*p == '/') {
        slash = p;
        break;
      }
    }
    if (slash && slash > cursor && (slash + 1) < token_end) {
      std::string library(cursor, static_cast<size_t>(slash - cursor));
      std::string version(slash + 1,
                          static_cast<size_t>(token_end - (slash + 1)));
      if (version.find('/') == std::string::npos) {
        changed |=
            registry->RegisterLibrary(library.c_str(), version.c_str());
      }
    }
    cursor = token_end;
  }
  if (changed) registry->UpdateUserAgent();
}

void RegisterSdkUsage(void* platform_resource) {
  MutexLock lock(*g_app_mutex);

  // Only register libraries when no C++ apps was created before.
  if (!LibraryRegistry::IsCommonLibraryRegistered()) {
    LibraryRegistry::Initialize();
    // This calls the platform specific method to propagate the registration to
    // any SDKs in use by this library.
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
    LibraryRegistry::SetCommonLibraryRegistered();
  }
}

const char* GetUserAgent() {
  MutexLock lock(*g_app_mutex);
  return LibraryRegistry::Initialize()->GetUserAgent();
}

std::string GetLibraryVersion(const char* library) {
  if (!library) return std::string();
  MutexLock lock(*g_app_mutex);
  return LibraryRegistry::Initialize()->GetLibraryVersion(library);
}

void GetOuterMostSdkAndVersion(std::string* sdk, std::string* version) {
  FIREBASE_ASSERT_RETURN_VOID(sdk != nullptr && version != nullptr);
  sdk->clear();
  version->clear();

  MutexLock lock(*g_app_mutex);
  // Set of library versions to query in order of outer wrapper to inner.
  // We're only retrieving the outer-most SDK version here as we can only send
  // one component as part of the user agent string to the storage backend.
  static const char* const kLibraryVersions[] = {
      FIREBASE_USER_AGENT_PREFIX "unity",
      FIREBASE_USER_AGENT_PREFIX "mono",
      FIREBASE_CPP_USER_AGENT_PREFIX,
  };
  LibraryRegistry* registry = LibraryRegistry::Initialize();
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kLibraryVersions); ++i) {
    std::string library(kLibraryVersions[i]);
    std::string library_version = registry->GetLibraryVersion(library);
    if (!library_version.empty()) {
      *sdk = library;
      *version = library_version;
      break;
    }
  }
}

// Find a logger associated with an app by app name.
Logger* FindAppLoggerByName(const char* name) {
  if (!name) return nullptr;
  MutexLock lock(*g_app_mutex);
  if (g_apps) {
    auto it = g_apps->find(name);
    if (it == g_apps->end()) return nullptr;
    return &it->second->logger;
  }
  return nullptr;
}

internal::FunctionRegistry* GetAppFunctionRegistry(App* app) {
  if (!app) return nullptr;
  MutexLock lock(*g_app_mutex);
  const char* app_name = app->name();
  if (g_apps && app_name) {
    auto it = g_apps->find(app_name);
    if (it != g_apps->end() && it->second->app == app &&
        it->second->function_registry) {
      return it->second->function_registry.get();
    }
  }
  if (!g_fallback_registries) {
    g_fallback_registries =
        new std::map<const App*, std::unique_ptr<internal::FunctionRegistry>>();
  }
  auto& reg = (*g_fallback_registries)[app];
  if (!reg) {
    reg = std::make_unique<internal::FunctionRegistry>();
  }
  return reg.get();
}

}  // namespace app_common
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
