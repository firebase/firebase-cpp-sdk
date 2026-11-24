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

#ifndef FIREBASE_APP_SRC_APP_COMMON_H_
#define FIREBASE_APP_SRC_APP_COMMON_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/src/include/firebase/app.h"

namespace firebase {

extern const char* const kDefaultAppName;

namespace internal {
class AppState;
}  // namespace internal

namespace app_common {

// Prefix applied to components of Firebase user agent strings.
#define FIREBASE_USER_AGENT_PREFIX "fire-"
// Prefix applied to Firebase C++ SDK components of user agent strings.
#define FIREBASE_CPP_USER_AGENT_PREFIX FIREBASE_USER_AGENT_PREFIX "cpp"

// Platform information.
extern const char* kOperatingSystem;
extern const char* kCppRuntimeOrStl;
extern const char* kCpuArchitecture;
extern const char* kBuildSource;

// Request headers used by platform monitoring.
extern const char* kApiClientHeader;
extern const char* kXFirebaseGmpIdHeader;

// Determine whether the specified app name refers to the default app.
bool IsDefaultAppName(std::string_view name);

// Opt-in global weak App registry helpers (backing App::Register(),
// App::Unregister(), App::GetInstance(), App::GetApps()).
bool RegisterApp(const std::shared_ptr<internal::AppState>& state);
void UnregisterApp(const std::shared_ptr<internal::AppState>& state);
std::shared_ptr<internal::AppState> FindRegisteredApp(std::string_view name);
std::vector<std::shared_ptr<internal::AppState>> GetRegisteredApps();
void ClearRegisteredAppsForTesting();

// Register a library which uses this SDK.
void RegisterLibrary(const char* library, const char* version);

// Register a set of libraries from a user agent string.
void RegisterLibrariesFromUserAgent(const char* user_agent);

// Register all user-agents related to C++ SDK usages.
void RegisterSdkUsage(void* platform_resource);

// Get the user agent string for all registered libraries.
const char* GetUserAgent();

// Get the version of a registered library.
std::string GetLibraryVersion(const char* library);

// Get the outer-most SDK above the C++ SDK.
void GetOuterMostSdkAndVersion(std::string* sdk, std::string* version);

// Clears registered libraries (for unit tests).
void ResetLibraryRegistryForTesting();

}  // namespace app_common
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_COMMON_H_
