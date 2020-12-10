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

#import <Foundation/Foundation.h>

#import "FIRAnalytics.h"

#include "analytics/src/include/firebase/analytics.h"

#include "analytics/src/analytics_common.h"
#include "app/src/include/firebase/version.h"
#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/time.h"
#include "app/src/thread.h"
#include "app/src/util.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace analytics {

// Used to workaround b/143656277 and b/110166640
class AnalyticsDataResetter {
 private:
  enum ResetState {
    kResetStateNone = 0,
    kResetStateRequested,
    kResetStateRetry,
  };
 public:
  // Initialize the class.
  AnalyticsDataResetter() : reset_state_(kResetStateNone), reset_timestamp_(0) {}

  // Reset analytics data.
  void Reset() {
    MutexLock lock(mutex_);
    reset_timestamp_ = firebase::internal::GetTimestampEpoch();
    reset_state_ = kResetStateRequested;
    instance_id_ = util::StringFromNSString([FIRAnalytics appInstanceID]);
    [FIRAnalytics resetAnalyticsData];
  }

  // Get the instance ID, returning a non-empty string if it's valid or an empty string if it's
  // still being reset.
  std::string GetInstanceId() {
    MutexLock lock(mutex_);
    std::string current_instance_id = util::StringFromNSString([FIRAnalytics appInstanceID]);
    uint64_t reset_time_elapsed_milliseconds = GetResetTimeElapsedMilliseconds();
    switch (reset_state_) {
      case kResetStateNone:
        break;
      case kResetStateRequested:
        if (reset_time_elapsed_milliseconds >= kResetRetryIntervalMilliseconds) {
          // Firebase Analytics on iOS can take a while to initialize, in this case we try to reset
          // again if the instance ID hasn't changed for a while.
          reset_state_ = kResetStateRetry;
          reset_timestamp_ = firebase::internal::GetTimestampEpoch();
          [FIRAnalytics resetAnalyticsData];
          return std::string();
        }
        FIREBASE_CASE_FALLTHROUGH;

      case kResetStateRetry:
        if ((current_instance_id.empty() || current_instance_id == instance_id_) &&
            reset_time_elapsed_milliseconds < kResetTimeoutMilliseconds) {
          return std::string();
        }
        break;
    }
    instance_id_ = current_instance_id;
    return current_instance_id;
  }

 private:
  // Get the time elapsed in milliseconds since reset was requested.
  uint64_t GetResetTimeElapsedMilliseconds() const {
    return firebase::internal::GetTimestampEpoch() - reset_timestamp_;
  }

 private:
  Mutex mutex_;
  // Reset attempt.
  ResetState reset_state_;
  // When a reset was last requested.
  uint64_t reset_timestamp_;
  // Instance ID before it was reset.
  std::string instance_id_;

  // Time to wait before trying to reset again.
  static const uint64_t kResetRetryIntervalMilliseconds;
  // Time to wait before giving up on resetting the ID.
  static const uint64_t kResetTimeoutMilliseconds;
};

DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);

const uint64_t AnalyticsDataResetter::kResetRetryIntervalMilliseconds = 1000;
const uint64_t AnalyticsDataResetter::kResetTimeoutMilliseconds = 5000;

static const double kMillisecondsPerSecond = 1000.0;
static Mutex g_mutex;  // NOLINT
static bool g_initialized = false;
static AnalyticsDataResetter *g_resetter = nullptr;

// Initialize the API.
void Initialize(const ::firebase::App& app) {
  MutexLock lock(g_mutex);
  g_initialized = true;
  g_resetter = new AnalyticsDataResetter();
  internal::RegisterTerminateOnDefaultAppDestroy();
  internal::FutureData::Create();
}

namespace internal {

// Determine whether the analytics module is initialized.
bool IsInitialized() { return g_initialized; }

}  // namespace internal

// Terminate the API.
void Terminate() {
  MutexLock lock(g_mutex);
  internal::FutureData::Destroy();
  internal::UnregisterTerminateOnDefaultAppDestroy();
  delete g_resetter;
  g_resetter = nullptr;
  g_initialized = false;
}

// Enable / disable measurement and reporting.
void SetAnalyticsCollectionEnabled(bool enabled) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setAnalyticsCollectionEnabled:enabled];
}

// Due to overloads of LogEvent(), it's possible for users to call variants that require a
// string with a nullptr.  To prevent a crash and instead yield a warning, pass an empty string
// to logEventWithName: methods instead.  See b/30061553 for the background.
NSString* SafeString(const char* event_name) { return @(event_name ? event_name : ""); }

// Log an event with one string parameter.
void LogEvent(const char* name, const char* parameter_name, const char* parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics logEventWithName:@(name)
                      parameters:@{SafeString(parameter_name) : SafeString(parameter_value)}];
}

// Log an event with one double parameter.
void LogEvent(const char* name, const char* parameter_name, const double parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics
      logEventWithName:@(name)
            parameters:@{SafeString(parameter_name) : [NSNumber numberWithDouble:parameter_value]}];
}

// Log an event with one 64-bit integer parameter.
void LogEvent(const char* name, const char* parameter_name, const int64_t parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics logEventWithName:@(name)
                      parameters:@{
                        SafeString(parameter_name) : [NSNumber numberWithLongLong:parameter_value]
                      }];
}

/// Log an event with one integer parameter (stored as a 64-bit integer).
void LogEvent(const char* name, const char* parameter_name, const int parameter_value) {
  LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value));
}

/// Log an event with no parameters.
void LogEvent(const char* name) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics logEventWithName:@(name) parameters:@{}];
}

// Log an event with associated parameters.
void LogEvent(const char* name, const Parameter* parameters, size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSMutableDictionary* parameters_dict =
      [NSMutableDictionary dictionaryWithCapacity:number_of_parameters];
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& parameter = parameters[i];
    NSString* parameter_name = SafeString(parameter.name);
    if (parameter.value.is_int64()) {
      [parameters_dict setObject:[NSNumber numberWithLongLong:parameter.value.int64_value()]
                          forKey:parameter_name];
    } else if (parameter.value.is_double()) {
      [parameters_dict setObject:[NSNumber numberWithDouble:parameter.value.double_value()]
                          forKey:parameter_name];
    } else if (parameter.value.is_string()) {
      [parameters_dict setObject:SafeString(parameter.value.string_value()) forKey:parameter_name];
    } else if (parameter.value.is_bool()) {
      // Just use integer 0 or 1.
      [parameters_dict setObject:[NSNumber numberWithLongLong:parameter.value.bool_value() ? 1 : 0]
                          forKey:parameter_name];
    } else if (parameter.value.is_null()) {
      // Just use integer 0 for null.
      [parameters_dict setObject:[NSNumber numberWithLongLong:0] forKey:parameter_name];
    } else {
      // Vector or Map were passed in.
      LogError(
          "LogEvent(%s): %s is not a valid parameter value type. "
          "Container types are not allowed. No event was logged.",
          parameter.name, Variant::TypeName(parameter.value.type()));
    }
  }
  [FIRAnalytics logEventWithName:@(name) parameters:parameters_dict];
}

// Set a user property to the given value.
void SetUserProperty(const char* name, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setUserPropertyString:(value ? @(value) : nil) forName:@(name)];
}

// Sets the user ID property. This feature must be used in accordance with
// <a href="https://www.google.com/policies/privacy">
// Google's Privacy Policy</a>
void SetUserId(const char* user_id) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setUserID:(user_id ? @(user_id) : nil)];
}

// Sets the duration of inactivity that terminates the current session.
void SetSessionTimeoutDuration(int64_t milliseconds) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics
      setSessionTimeoutInterval:static_cast<NSTimeInterval>(milliseconds) / kMillisecondsPerSecond];
}

void SetCurrentScreen(const char* screen_name, const char* screen_class) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setScreenName:screen_name ? @(screen_name) : nil
                  screenClass:screen_class ? @(screen_class) : nil];
}

void ResetAnalyticsData() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_resetter->Reset();
}

Future<std::string> GetAnalyticsInstanceId() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto* api = internal::FutureData::Get()->api();
  const auto future_handle = api->SafeAlloc<std::string>(
      internal::kAnalyticsFnGetAnalyticsInstanceId);
  static int kPollTimeMs = 100;
  Thread get_id_thread([](SafeFutureHandle<std::string>* handle) {
      for ( ; ; ) {
        {
          MutexLock lock(g_mutex);
          if (!internal::IsInitialized()) break;
          std::string instance_id = g_resetter->GetInstanceId();
          if (!instance_id.empty()) {
            internal::FutureData::Get()->api()->CompleteWithResult(
                *handle, 0, "", instance_id);
            break;
          }
        }
        firebase::internal::Sleep(kPollTimeMs);
      }
      delete handle;
    }, new SafeFutureHandle<std::string>(future_handle));
  get_id_thread.Detach();
  return Future<std::string>(api, future_handle.get());
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  return static_cast<const Future<std::string>&>(
      internal::FutureData::Get()->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}

}  // namespace measurement
}  // namespace firebase
