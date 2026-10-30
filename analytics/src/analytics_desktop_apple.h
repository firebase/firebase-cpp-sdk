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

// macOS desktop backend for Analytics that forwards to the Firebase Apple SDK
// (FirebaseAnalytics / GoogleAppMeasurement) when it is embedded in the
// process.
//
// The backend binds to the Apple SDK at *runtime* (NSClassFromString), so
// firebase_analytics has no link-time dependency on it:
//  * Binaries that embed the Apple SDK (e.g. the Unity FirebaseCppApp bundle,
//    or any app that calls firebase_cpp_link_apple_analytics_sdk() from
//    cmake/firebase_apple_desktop_sdk.cmake) get real Analytics.
//  * Binaries that don't keep the existing desktop stub behavior.
//
// Backend selection (evaluated in Initialize()):
//  1. SetBackendMode(), if called with anything other than kBackendModeAuto.
//  2. The FIREBASE_ANALYTICS_APPLE_SDK environment variable:
//       "disabled" / "0" / "off" / "false"  -> stubs
//       "enabled"  / "1" / "on"  / "true"   -> Apple SDK
//       "debug"                              -> Apple SDK + DebugView
//  3. Auto: enabled, including inside the Unity Editor. Debug mode is left to
//     the app (SetDesktopDebugMode() / "debug"). Editor caveats, which the
//     SDK cannot fix: GoogleAppMeasurement reports the main bundle's (the
//     Editor's) bundle ID and app version, and keeps its state in the Editor's
//     NSUserDefaults / Application Support, shared by every Unity project on
//     the machine.
//  "disabled" / "enabled" / "debug" set explicitly also overwrite the
//  persisted debug flag; auto leaves it alone.

#ifndef FIREBASE_ANALYTICS_SRC_ANALYTICS_DESKTOP_APPLE_H_
#define FIREBASE_ANALYTICS_SRC_ANALYTICS_DESKTOP_APPLE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "analytics/src/include/firebase/analytics.h"
#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace analytics {
namespace internal {
namespace apple {

enum BackendMode {
  kBackendModeAuto = 0,
  kBackendModeDisabled,
  kBackendModeEnabled,
  kBackendModeDebug,
};

// Name of the environment variable used to select the backend.
extern const char kBackendModeEnvironmentVariable[];

// Overrides the backend selection. Must be called before
// firebase::analytics::Initialize() to take effect.
void SetBackendMode(BackendMode mode);

// Parses a FIREBASE_ANALYTICS_APPLE_SDK value. Returns kBackendModeAuto for
// null / empty / unrecognized values. Exposed for testing.
BackendMode ParseBackendMode(const char* value);

// Returns true if `app_id` can be used with the Firebase Apple SDK, i.e. it is
// an Apple-platform Google App ID ("1:<sender>:ios:<hash>"). Exposed for
// testing.
bool IsAppleGoogleAppId(const char* app_id);

// Attempts to activate the Apple SDK backend for `app`. Returns true if the
// backend is active, false if callers should fall back to the stubs.
// `collection_enabled` / `collection_enabled_set` carry a
// SetAnalyticsCollectionEnabled() call made before Initialize().
bool Initialize(const App& app, bool collection_enabled,
                bool collection_enabled_set);

// Deactivates the backend. The underlying FIRApp is intentionally left
// configured: the Apple SDK does not support being torn down and
// re-configured within a process (relevant for Unity Editor play-mode
// cycles).
void Terminate();

// Whether the Apple SDK backend is currently active.
bool IsActive();

void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters);
void SetUserProperty(const char* name, const char* value);
void SetUserId(const char* user_id);
void SetAnalyticsCollectionEnabled(bool enabled);
void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings);
void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters);
void SetSessionTimeoutDuration(int64_t milliseconds);
void ResetAnalyticsData();
void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address);
void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number);
void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    const std::vector<unsigned char>& hashed_email_address);
void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    const std::vector<unsigned char>& hashed_phone_number);

// Persists the Apple SDK debug-mode flag. The Apple SDK reads it when it
// starts, so a change applies to the next process launch (use the "debug"
// backend mode to enable it for the current launch). No-op if the Apple SDK
// is not embedded.
void SetDebugMode(bool enabled);

// Completes `handle` with the app instance ID once it is available.
void GetAnalyticsInstanceId(ReferenceCountedFutureImpl* api,
                            const SafeFutureHandle<std::string>& handle);

// Completes `handle` with the current session ID.
void GetSessionId(ReferenceCountedFutureImpl* api,
                  const SafeFutureHandle<int64_t>& handle);

}  // namespace apple
}  // namespace internal
}  // namespace analytics
}  // namespace firebase

#endif  // FIREBASE_ANALYTICS_SRC_ANALYTICS_DESKTOP_APPLE_H_
