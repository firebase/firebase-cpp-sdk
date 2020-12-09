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

#include "analytics/src/include/firebase/analytics.h"

#include <sstream>

#include "analytics/src/analytics_common.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/version.h"

namespace firebase {
namespace analytics {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);

static bool g_initialized = false;
static int g_fake_instance_id = 0;

// Initialize the API.
void Initialize(const ::firebase::App& /*app*/) {
  g_initialized = true;
  internal::RegisterTerminateOnDefaultAppDestroy();
  internal::FutureData::Create();
  g_fake_instance_id = 0;
}

namespace internal {

// Determine whether the analytics module is initialized.
bool IsInitialized() { return g_initialized; }

}  // namespace internal

// Terminate the API.
void Terminate() {
  internal::FutureData::Destroy();
  internal::UnregisterTerminateOnDefaultAppDestroy();
  g_initialized = false;
}

// Enable / disable measurement and reporting.
void SetAnalyticsCollectionEnabled(bool /*enabled*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Log an event with one string parameter.
void LogEvent(const char* /*name*/, const char* /*parameter_name*/,
              const char* /*parameter_value*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Log an event with one double parameter.
void LogEvent(const char* /*name*/, const char* /*parameter_name*/,
              const double /*parameter_value*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Log an event with one 64-bit integer parameter.
void LogEvent(const char* /*name*/, const char* /*parameter_name*/,
              const int64_t /*parameter_value*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Log an event with one integer parameter (stored as a 64-bit integer).
void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value));
}

// Log an event with no parameters.
void LogEvent(const char* name) {
  LogEvent(name, static_cast<const Parameter*>(nullptr),
           static_cast<size_t>(0));
}

// Log an event with associated parameters.
void LogEvent(const char* /*name*/, const Parameter* /*parameters*/,
              size_t /*number_of_parameters*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Set a user property to the given value.
void SetUserProperty(const char* /*name*/, const char* /*value*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Sets the user ID property. This feature must be used in accordance with
// <a href="https://www.google.com/policies/privacy">
// Google's Privacy Policy</a>
void SetUserId(const char* /*user_id*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

// Sets the duration of inactivity that terminates the current session.
void SetSessionTimeoutDuration(int64_t /*milliseconds*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

void SetCurrentScreen(const char* /*screen_name*/,
                      const char* /*screen_class*/) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

void ResetAnalyticsData() {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_fake_instance_id++;
}

Future<std::string> GetAnalyticsInstanceId() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto* api = internal::FutureData::Get()->api();
  const auto future_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  std::string instance_id = std::string("FakeAnalyticsInstanceId");
  {
    std::stringstream ss;
    ss << g_fake_instance_id;
    instance_id += ss.str();
  }
  api->CompleteWithResult(future_handle, 0, "", instance_id);
  return Future<std::string>(api, future_handle.get());
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  return static_cast<const Future<std::string>&>(
      internal::FutureData::Get()->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}

}  // namespace analytics
}  // namespace firebase
