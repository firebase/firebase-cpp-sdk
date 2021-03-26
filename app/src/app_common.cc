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

#include <map>
#include <string>
#include <utility>  // Used to detect STL variant.
#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/assert.h"
#include "app/src/callback.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/mutex.h"
#include "app/src/util.h"

// strtok_r is strtok_s on Windows.
#if FIREBASE_PLATFORM_WINDOWS
#define strtok_r strtok_s
#endif  // FIREBASE_PLATFORM_WINDOWS

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

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

// Default app name.
const char* const kDefaultAppName = "__FIRAPP_DEFAULT";

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

#if defined(_STLPORT_VERSION)
const char* kCppRuntimeOrStl = "stlport";
#elif defined(_GLIBCXX_UTILITY)
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
#else
#error Unknown Linux architecture.
#endif  // Architecture
#else
#error Unknown operating system.
#endif  // Operating system
// clang-format=on

const char* kApiClientHeader = "x-goog-api-client";

SystemLogger g_system_logger;  // NOLINT

// Private cross platform data associated with an app.
struct AppData {
  // TODO(b/140528778): Remove kLogLevelVerbose here and add [GS]etLogLevel
  // member functions to app.
  AppData() : logger(&g_system_logger, kLogLevelVerbose) {}

  // App associated with this data.
  App* app;
  // Notifies subscribers when the app is about to be destroyed.
  CleanupNotifier cleanup_notifier;
  // A per-app logger.
  Logger logger;
};

// Tracks library registrations.
class LibraryRegistry {
 private:
  LibraryRegistry() {}

 public:
  // Register a library, returns true if the library version changed.
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
    user_agent_.clear();
    for (auto it = library_to_version_.begin(); it != library_to_version_.end();
         ++it) {
      user_agent_ += it->first + "/" + it->second + " ";
    }
    // Removing the trailing space.
    if (!user_agent_.empty()) {
      user_agent_ = user_agent_.substr(0, user_agent_.length() - 1);
    }
  }

  // Get the cached user agent string.
  const char* GetUserAgent() const { return user_agent_.c_str(); }

 public:
  // Create the library registry singleton and get the instance.
  static LibraryRegistry* Initialize() {
    if (!library_registry_) library_registry_ = new LibraryRegistry();
    return library_registry_;
  }

  // Destroy the library registry singleton if there is one.
  static void Terminate() {
    if (library_registry_) {
      delete library_registry_;
      library_registry_ = nullptr;
    }
  }

 private:
  std::map<std::string, std::string> library_to_version_;
  std::string user_agent_;

  static LibraryRegistry* library_registry_;
};

// Guards g_apps and g_default_app.
static Mutex g_app_mutex;  // NOLINT
static std::map<std::string, UniquePtr<AppData>>* g_apps;
static App* g_default_app = nullptr;
LibraryRegistry* LibraryRegistry::library_registry_ = nullptr;

App* AddApp(App* app, std::map<std::string, InitResult>* results) {
  bool created_first_app = false;
  assert(app);
  App* existing_app = FindAppByName(app->name());
  FIREBASE_ASSERT_RETURN(nullptr, !existing_app);
  MutexLock lock(g_app_mutex);
  if (IsDefaultAppName(app->name())) {
    assert(!g_default_app);
    g_default_app = app;
  }
  UniquePtr<AppData> app_data = MakeUnique<AppData>();
  app_data->app = app;
  app_data->cleanup_notifier.RegisterOwner(app);
  if (!g_apps) {
    g_apps = new std::map<std::string, UniquePtr<AppData>>();
    created_first_app = true;
  }
  (*g_apps)[std::string(app->name())] = app_data;
  // Create a cleanup notifier for the app.
  {
    const AppOptions& app_options = app->options();
    LogDebug(
        "Added app name=%s: options, api_key=%s, app_id=%s, database_url=%s, "
        "messaging_sender_id=%s, storage_bucket=%s, project_id=%s (0x%08x)",
        app->name(), app_options.api_key(), app_options.app_id(),
        app_options.database_url(), app_options.messaging_sender_id(),
        app_options.storage_bucket(), app_options.project_id(),
        static_cast<int>(reinterpret_cast<intptr_t>(app)));
  }
  LibraryRegistry::Initialize();
  if (created_first_app) {
    // This calls the platform specific method to propagate the registration to
    // any SDKs in use by this library.
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX,
                         FIREBASE_VERSION_NUMBER_STRING);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-os",
                         kOperatingSystem);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-arch",
                         kCpuArchitecture);
    App::RegisterLibrary(FIREBASE_CPP_USER_AGENT_PREFIX "-stl",
                         kCppRuntimeOrStl);
  }
  callback::Initialize();
  AppCallback::NotifyAllAppCreated(app, results);
  return app;
}

App* FindAppByName(const char* name) {
  assert(name);
  MutexLock lock(g_app_mutex);
  if (g_apps) {
    auto it = g_apps->find(std::string(name));
    if (it == g_apps->end()) return nullptr;
    return it->second->app;
  }
  return nullptr;
}

App* GetDefaultApp() { return g_default_app; }

App* GetAnyApp() {
  if (g_default_app) {
    return g_default_app;
  }

  MutexLock lock(g_app_mutex);
  if (g_apps && !g_apps->empty()) {
    return g_apps->begin()->second->app;
  }
  return nullptr;
}

void RemoveApp(App* app) {
  assert(app);
  MutexLock lock(g_app_mutex);
  if (g_apps) {
    auto it = g_apps->find(std::string(app->name()));
    bool last_app = false;
    // If app initialization failed AddApp() will not be called.
    if (it != g_apps->end()) {
      LogDebug("Deleting app %s (0x%08x)", app->name(),
               static_cast<int>(reinterpret_cast<intptr_t>(app)));
      // Notify app callbacks before removing the app from the g_apps
      // dictionary, this makes it possible to look up apps by name on
      // destruction.
      it->second->cleanup_notifier.CleanupAll();
      AppCallback::NotifyAllAppDestroyed(app);
      // Remove the app entry from the map, from this point it is no longer
      // accessible to other components.
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
    callback::Terminate(last_app);
    if (last_app) {
      LibraryRegistry::Terminate();
    }
  }
}

void DestroyAllApps() {
  std::vector<App*> apps_to_delete;
  App* const default_app = GetDefaultApp();
  MutexLock lock(g_app_mutex);
  if (g_apps) {
    for (auto it = g_apps->begin(); it != g_apps->end(); ++it) {
      if (it->second->app != default_app)
        apps_to_delete.push_back(it->second->app);
    }
    if (default_app) apps_to_delete.push_back(default_app);
    for (auto it = apps_to_delete.begin(); it != apps_to_delete.end(); ++it) {
      // As each App is deleted, RemoveApp() is called by the destructor which
      // removes the app from the global list of apps (g_apps).  When g_apps
      // is empty it is deleted by RemoveApp().
      delete *it;
    }
  }
}

// Determine whether the specified app name refers to the default app.
bool IsDefaultAppName(const char* name) {
  return strcmp(kDefaultAppName, name) == 0;
}

void RegisterLibrary(const char* library, const char* version) {
  MutexLock lock(g_app_mutex);
  LibraryRegistry* registry = LibraryRegistry::Initialize();
  if (registry->RegisterLibrary(library, version)) {
    registry->UpdateUserAgent();
  }
}

void RegisterLibrariesFromUserAgent(const char* user_agent) {
  MutexLock lock(g_app_mutex);
  LibraryRegistry* registry = LibraryRegistry::Initialize();
  // Copy the string into a vector so that we can safely mutate the string.
  std::vector<char> user_agent_vector(user_agent,
                                      user_agent + strlen(user_agent) + 1);
  if (user_agent_vector.empty()) return;
  char* token = &user_agent_vector[0];
  char* next_token = nullptr;
  bool changed = false;
  do {
    // Split fields in the string.
    static const char kFieldSeparator[] = " ";
    token = strtok_r(token, kFieldSeparator, &next_token);
    if (token) {
      // Split library / version components and register each tuple.
      static const char kLibraryVersionSeparator[] = "/";
      char* version;
      char* library = strtok_r(token, kLibraryVersionSeparator, &version);
      if (library && version) {
        changed |= registry->RegisterLibrary(library, version);
      }
    }
    token = next_token;
  } while (token && next_token && next_token[0] != '\0');
  if (changed) registry->UpdateUserAgent();
}

const char* GetUserAgent() {
  MutexLock lock(g_app_mutex);
  return LibraryRegistry::Initialize()->GetUserAgent();
}

std::string GetLibraryVersion(const char* library) {
  MutexLock lock(g_app_mutex);
  return LibraryRegistry::Initialize()->GetLibraryVersion(library);
}

void GetOuterMostSdkAndVersion(std::string* sdk, std::string* version) {
  assert(sdk);
  assert(version);
  sdk->clear();
  version->clear();

  MutexLock lock(g_app_mutex);
  // Set of library versions to query in order of outer wrapper to inner.
  // We're only retrieving the outer-most SDK version here as we can only send
  // one component as part of the user agent string to the storage backend.
  static const char* kLibraryVersions[] = {
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
  assert(name);
  MutexLock lock(g_app_mutex);
  if (g_apps) {
    auto it = g_apps->find(std::string(name));
    if (it == g_apps->end()) return nullptr;
    return &it->second->logger;
  }
  return nullptr;
}

}  // namespace app_common
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
