// Copyright 2026 Google LLC
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

#ifndef FIREBASE_INSTALLATIONS_SRC_DESKTOP_INSTALLATIONS_DESKTOP_APPLE_H_
#define FIREBASE_INSTALLATIONS_SRC_DESKTOP_INSTALLATIONS_DESKTOP_APPLE_H_

#include <memory>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace installations {
namespace internal {
namespace apple {

// Controls whether the macOS desktop build forwards Installations calls to the
// Firebase Apple SDK (FirebaseInstallations / FirebaseCore).
enum BackendMode {
  // Default: use the Apple SDK if its Objective-C classes are linked into the
  // host process and the App is configured with an Apple App ID, otherwise fall
  // back to the desktop stubs.
  kBackendModeAuto = 0,
  // Never use the Apple SDK; always use the desktop stubs.
  kBackendModeDisabled,
  // Use the Apple SDK if linked; log a warning if it is missing.
  kBackendModeEnabled,
};

// Environment variable that overrides the backend mode at process startup.
// Recognized values (case-insensitive):
//   "0", "false", "off", "no", "disabled", "disable" -> kBackendModeDisabled
//   "1", "true",  "on",  "yes", "enabled", "enable"  -> kBackendModeEnabled
//   "debug"                                          -> kBackendModeEnabled
//   anything else / unset                            -> kBackendModeAuto
// If unset, FIREBASE_ANALYTICS_APPLE_SDK is also checked as a fallback so a
// single environment variable can control the embedded Apple SDK.
constexpr const char kBackendModeEnvironmentVariable[] =
    "FIREBASE_INSTALLATIONS_APPLE_SDK";
constexpr const char kFallbackBackendModeEnvironmentVariable[] =
    "FIREBASE_ANALYTICS_APPLE_SDK";

// Exposed for unit testing.
BackendMode ParseBackendMode(const char* value);
bool IsAppleGoogleAppId(const char* app_id);

// Programmatic override for tests; takes precedence over the environment
// variable when not kBackendModeAuto. Call before Installations::GetInstance().
void SetBackendMode(BackendMode mode);

// Reference-counted per-instance state shared with asynchronous Objective-C
// completion blocks so deleting an Installations instance while Futures are
// pending cannot cause a use-after-free on ReferenceCountedFutureImpl.
struct InstanceState;

// Attempts to initialize the Firebase Apple SDK (FIRApp + FIRInstallations) for
// `app`. Returns a valid active state if the Apple SDK classes are linked into
// the current process and `app` has a valid Apple configuration; otherwise
// returns nullptr so the caller falls back to the desktop stubs.
std::shared_ptr<InstanceState> CreateInstance(
    const App& app, ReferenceCountedFutureImpl* future_impl);

// Returns true while `state` is backed by a live FIRInstallations instance.
bool IsInstanceActive(const std::shared_ptr<InstanceState>& state);

// Detaches `state` from its ReferenceCountedFutureImpl so any in-flight
// completion handlers become safe no-ops. Must be called before `future_impl`
// is destroyed.
void CleanupInstance(const std::shared_ptr<InstanceState>& state);

void LogHeartbeat(const App& app);

void GetId(const std::shared_ptr<InstanceState>& state,
           SafeFutureHandle<std::string> handle);
void GetToken(const std::shared_ptr<InstanceState>& state, bool force_refresh,
              SafeFutureHandle<std::string> handle);
void Delete(const std::shared_ptr<InstanceState>& state,
            SafeFutureHandle<void> handle);

}  // namespace apple
}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_DESKTOP_INSTALLATIONS_DESKTOP_APPLE_H_
