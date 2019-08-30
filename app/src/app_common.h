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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_APP_COMMON_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_APP_COMMON_H_

#include <stddef.h>

#include <map>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/logger.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Default app name.
extern const char* const kDefaultAppName;

namespace app_common {

// Prefix applied to components of Firebase user agent strings.
#define FIREBASE_USER_AGENT_PREFIX "fire-"
// Prefix applied to Firebase C++ SDK components of user agent strings.
#define FIREBASE_CPP_USER_AGENT_PREFIX FIREBASE_USER_AGENT_PREFIX "cpp"

// Platform information.
extern const char* kOperatingSystem;
extern const char* kCppRuntimeOrStl;
extern const char* kCpuArchitecture;

// Extended API client header for Google user agent strings.
extern const char* kApiClientHeader;

// Add an app to the set of apps.
App* AddApp(App* app, std::map<std::string, InitResult>* results);

// Find an app in the set of apps.
App* FindAppByName(const char* name);

// Get the default app.
App* GetDefaultApp();

// Get an instantiated App. If there is more than one App, an unspecified App
// will be returned.
App* GetAnyApp();

// Remove an app from the set of apps.
// Call this method before destroying an app.
void RemoveApp(App* app);

// Destroy all apps.
void DestroyAllApps();

// Determine whether the specified app name refers to the default app.
bool IsDefaultAppName(const char* name);

// Register a library which uses this SDK.
// Library registrations are not thread safe as we build the user agent string
// once and return it from the UserAgent() method.
// NOTE: This is an internal method, use App::RegisterLibrary() instead.
void RegisterLibrary(const char* library, const char* version);

// Register a set of libraries from a user agent string.
// This is useful when this SDK wraps other SDKs.
void RegisterLibrariesFromUserAgent(const char* user_agent);

// Get the user agent string for all registered libraries.
// This is not thread safe w.r.t RegisterLibrary().
// NOTE: This is an internal method, use App::UserAgent() instead.
const char* GetUserAgent();

// Get the version of a registered library.
// This returns an empty string if the library isn't registered.
std::string GetLibraryVersion(const char* library);

// Get the outer-most SDK above the C++ SDK.
// sdk can be one of "fire-unity", "fire-mono" or "fire-cpp".
// version is the version of the SDK respective SDK.
void GetOuterMostSdkAndVersion(std::string* sdk, std::string* version);

// Find a logger associated with an app by app name.
Logger* FindAppLoggerByName(const char* name);

}  // namespace app_common
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_APP_COMMON_H_
