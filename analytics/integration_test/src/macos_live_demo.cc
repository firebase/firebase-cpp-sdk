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

// Standalone macOS desktop verification app for Firebase Analytics +
// Firebase Installations backed by the embedded Firebase Apple SDK.
//
// Builds as `macos_live_demo.app` when configuring `analytics/integration_test`
// with `-DFIREBASE_ANALYTICS_TEST_EMBED_APPLE_SDK=ON`. Enables Analytics
// DebugView mode, retrieves the real Firebase Installation ID (FID),
// Installation Auth Token, Analytics App Instance ID, and Session ID, logs
// verification events, and pumps the macOS CFRunLoop so GoogleAppMeasurement
// flushes the events to the Firebase Console -> Analytics -> DebugView.

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <libgen.h>
#include <unistd.h>
#endif  // defined(__APPLE__)

#include "firebase/analytics.h"
#include "firebase/analytics/event_names.h"
#include "firebase/analytics/parameter_names.h"
#include "firebase/analytics/user_property_names.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/installations.h"
#include "firebase/log.h"

namespace {

void PumpMainLoopMilliseconds(int milliseconds) {
#if defined(__APPLE__)
  double seconds = static_cast<double>(milliseconds) / 1000.0;
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
#endif
}

template <typename T>
bool WaitForFuture(const firebase::Future<T>& future, const char* label,
                   int timeout_ms = 15000) {
  int elapsed = 0;
  constexpr int kStepMs = 50;
  while (future.status() == firebase::kFutureStatusPending &&
         elapsed < timeout_ms) {
    PumpMainLoopMilliseconds(kStepMs);
    elapsed += kStepMs;
  }
  if (future.status() != firebase::kFutureStatusComplete) {
    std::fprintf(stderr, "[ERROR] %s did not complete within %d ms (status=%d)\n",
                 label, timeout_ms, static_cast<int>(future.status()));
    return false;
  }
  if (future.error() != 0) {
    std::fprintf(stderr, "[ERROR] %s failed with error=%d: %s\n", label,
                 future.error(),
                 future.error_message() ? future.error_message() : "");
    return false;
  }
  return true;
}

#if defined(__APPLE__)
std::string GetMainBundleIdentifier() {
  CFBundleRef main_bundle = CFBundleGetMainBundle();
  if (!main_bundle) return "<no main bundle>";
  CFStringRef bundle_id = CFBundleGetIdentifier(main_bundle);
  if (!bundle_id) return "<missing CFBundleIdentifier>";
  char buffer[512] = {0};
  if (CFStringGetCString(bundle_id, buffer, sizeof(buffer),
                         kCFStringEncodingUTF8)) {
    return std::string(buffer);
  }
  return "<unreadable CFBundleIdentifier>";
}

// Ensures the working directory contains google-services-desktop.json if it was
// copied next to the executable inside the .app bundle.
void EnsureConfigDirectory(const char* argv0) {
  if (access("google-services-desktop.json", R_OK) == 0 ||
      access("google-services.json", R_OK) == 0) {
    return;
  }
  if (argv0 == nullptr) return;
  std::vector<char> path_copy(argv0, argv0 + std::strlen(argv0) + 1);
  const char* exe_dir = dirname(path_copy.data());
  if (exe_dir != nullptr) {
    std::string candidate =
        std::string(exe_dir) + "/google-services-desktop.json";
    if (access(candidate.c_str(), R_OK) == 0) {
      (void)chdir(exe_dir);
    }
  }
}
#endif  // defined(__APPLE__)

}  // namespace

int main(int argc, char* argv[]) {
  int wait_seconds = 15;
  if (const char* env_wait = std::getenv("FIREBASE_DEMO_WAIT_SECONDS")) {
    int parsed = std::atoi(env_wait);
    if (parsed > 0) wait_seconds = parsed;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--wait-seconds") == 0 && i + 1 < argc) {
      wait_seconds = std::atoi(argv[++i]);
    }
  }

#if defined(__APPLE__)
  EnsureConfigDirectory(argc > 0 ? argv[0] : nullptr);
#endif

  firebase::SetLogLevel(firebase::kLogLevelDebug);

  std::printf("============================================================\n");
  std::printf(" Firebase macOS Desktop Live Verification (Analytics + FID)\n");
  std::printf("============================================================\n");
#if defined(__APPLE__)
  std::printf("  Host Bundle ID (NSBundle): %s\n",
              GetMainBundleIdentifier().c_str());
#endif

  // Enable DebugView mode BEFORE initializing Analytics so the Apple SDK sees
  // /google/firebase/debug_mode = YES in NSUserDefaults at startup.
  firebase::analytics::SetDesktopDebugMode(true);

  firebase::App* app = firebase::App::Create();
  if (app == nullptr) {
    std::fprintf(stderr,
                 "[FATAL] Failed to create firebase::App. Ensure "
                 "google-services-desktop.json (converted from your Apple "
                 "GoogleService-Info.plist) is present.\n");
    return 1;
  }

  std::printf("  Configured App ID:         %s\n", app->options().app_id());
  std::printf("  Configured Project ID:     %s\n", app->options().project_id());
  std::printf("  Configured Bundle ID:      %s\n",
              app->options().package_name());
  std::printf("------------------------------------------------------------\n");

  // 1. Initialize Firebase Analytics and Firebase Installations.
  firebase::analytics::Initialize(*app);
  auto* installations =
      firebase::installations::Installations::GetInstance(app);
  if (installations == nullptr) {
    std::fprintf(stderr, "[FATAL] Installations::GetInstance returned null.\n");
    firebase::analytics::Terminate();
    delete app;
    return 1;
  }

  bool desktop_initialized = firebase::analytics::IsDesktopInitialized();
  std::printf("  Analytics Apple Backend:   %s\n",
              desktop_initialized ? "ACTIVE (FIRAnalytics)" : "STUB (Fallback)");
  if (!desktop_initialized) {
    std::fprintf(
        stderr,
        "[WARNING] Analytics Apple SDK backend is not active! Check that:\n"
        "  1. The app was built with -DFIREBASE_ANALYTICS_TEST_EMBED_APPLE_SDK=ON\n"
        "  2. google-services-desktop.json has an Apple App ID (1:...:ios:...)\n");
  }

  // 2. Fetch the real Firebase Installation ID (FID) and Auth Token.
  firebase::Future<std::string> fid_future = installations->GetId();
  if (!WaitForFuture(fid_future, "Installations::GetId")) {
    delete installations;
    firebase::analytics::Terminate();
    delete app;
    return 1;
  }
  std::string fid = fid_future.result() ? *fid_future.result() : "";
  std::printf("  Firebase Installation ID:  %s\n", fid.c_str());

  firebase::Future<std::string> token_future = installations->GetToken(false);
  if (WaitForFuture(token_future, "Installations::GetToken")) {
    std::string token = token_future.result() ? *token_future.result() : "";
    std::printf("  Installation Auth Token:   <%zu chars, non-empty=%s>\n",
                token.size(), token.empty() ? "no" : "yes");
  }

  // 3. Log verification user properties and events.
  firebase::analytics::SetAnalyticsCollectionEnabled(true);
  firebase::analytics::SetUserId("macos_desktop_demo_user");
  firebase::analytics::SetUserProperty("desktop_platform", "macOS");
  firebase::analytics::SetUserProperty(
      firebase::analytics::kUserPropertySignUpMethod, "macOS_Desktop_SDK");

  const firebase::analytics::Parameter kVerificationParams[] = {
      firebase::analytics::Parameter("sdk_layer", "firebase_cpp_sdk"),
      firebase::analytics::Parameter("apple_sdk_bridged", "true"),
      firebase::analytics::Parameter("installation_id", fid.c_str()),
      firebase::analytics::Parameter("verification_timestamp",
                                     static_cast<int64_t>(std::time(nullptr))),
  };
  firebase::analytics::LogEvent(
      "macos_desktop_verification", kVerificationParams,
      sizeof(kVerificationParams) / sizeof(kVerificationParams[0]));

  firebase::analytics::LogEvent(firebase::analytics::kEventLogin,
                                firebase::analytics::kParameterMethod,
                                "macos_desktop_live_demo");

  const firebase::analytics::Parameter kLevelUpParams[] = {
      firebase::analytics::Parameter(firebase::analytics::kParameterLevel,
                                     static_cast<int64_t>(42)),
      firebase::analytics::Parameter(firebase::analytics::kParameterCharacter,
                                     "macos_explorer"),
  };
  firebase::analytics::LogEvent(
      firebase::analytics::kEventLevelUp, kLevelUpParams,
      sizeof(kLevelUpParams) / sizeof(kLevelUpParams[0]));

  // 4. Fetch Analytics App Instance ID and Session ID.
  firebase::Future<std::string> instance_id_future =
      firebase::analytics::GetAnalyticsInstanceId();
  if (WaitForFuture(instance_id_future, "Analytics::GetAnalyticsInstanceId")) {
    std::printf("  Analytics App Instance ID: %s\n",
                instance_id_future.result()
                    ? instance_id_future.result()->c_str()
                    : "");
  }

  firebase::Future<int64_t> session_id_future =
      firebase::analytics::GetSessionId();
  if (WaitForFuture(session_id_future, "Analytics::GetSessionId")) {
    std::printf("  Analytics Session ID:      %" PRId64 "\n",
                session_id_future.result() ? *session_id_future.result() : 0);
  }

  std::printf("------------------------------------------------------------\n");
  std::printf("  Logged events:\n");
  std::printf("    1. macos_desktop_verification (installation_id=%s)\n",
              fid.c_str());
  std::printf("    2. login (method=macos_desktop_live_demo)\n");
  std::printf("    3. level_up (level=42, character=macos_explorer)\n");
  std::printf("------------------------------------------------------------\n");
  std::printf(
      "  Running macOS CFRunLoop for %d seconds so GoogleAppMeasurement\n"
      "  flushes DebugView packets to Firebase Console...\n",
      wait_seconds);

  for (int remaining = wait_seconds; remaining > 0; --remaining) {
    std::printf("    [%2d s remaining]\r", remaining);
    std::fflush(stdout);
    PumpMainLoopMilliseconds(1000);
  }
  std::printf("\n============================================================\n");
  std::printf("  Done! Check Firebase Console -> Analytics -> DebugView.\n");
  std::printf("============================================================\n");

  delete installations;
  firebase::analytics::Terminate();
  delete app;
  return 0;
}
