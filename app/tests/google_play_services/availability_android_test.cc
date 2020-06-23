/*
 * Copyright 2017 Google LLC
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

#include <unistd.h>

#include "absl/strings/str_format.h"

#if !defined(__ANDROID__)
// We need enum definition in the header, which is only available for android.
// However, we cannot compile the entire test for android due to build error in
// portable //base library.
#define __ANDROID__
#include "app/src/google_play_services/availability_android.h"
#undef __ANDROID__
#endif  // !defined(__ANDROID__)

#include "base/stringprintf.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/run_all_tests.h"
#include "testing/testdata_config_generated.h"
#include "testing/ticker.h"

namespace google_play_services {

// Wait for a future up to the specified number of milliseconds.
template <typename T>
static void WaitForFutureWithTimeout(
    const firebase::Future<T>& future,
    int timeout_milliseconds = 1000 /* 1 second */,
    firebase::FutureStatus expected_status = firebase::kFutureStatusComplete) {
  while (future.status() != expected_status && timeout_milliseconds-- > 0) {
    usleep(1000 /* microseconds per millisecond */);
  }
}

TEST(AvailabilityAndroidTest, Initialize) {
  // Initialization should succeed.
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));

  // Clean up afterwards.
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

TEST(AvailabilityAndroidTest, InitializeTwice) {
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));
  // Should be fine if called again.
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));

  // Terminate needs to be called twice to properly clean up.
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

TEST(AvailabilityAndroidTest, CheckAvailabilityOther) {
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));

  // Get null from getInstance(). Result is unavailable (other).
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'GoogleApiAvailability.getInstance'}"
      "  ]"
      "}");
  EXPECT_EQ(kAvailabilityUnavailableOther,
            CheckAvailability(firebase::testing::cppsdk::GetTestJniEnv(),
                              firebase::testing::cppsdk::GetTestActivity()));

  // We do not care about result 10 and specify it as other.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'GoogleApiAvailability.isGooglePlayServicesAvailable',"
      "     futureint:{value:10}}"
      "  ]"
      "}");
  EXPECT_EQ(kAvailabilityUnavailableOther,
            CheckAvailability(firebase::testing::cppsdk::GetTestJniEnv(),
                              firebase::testing::cppsdk::GetTestActivity()));

  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

TEST(AvailabilityAndroidTest, CheckAvailabilityCases) {
  // Enums are defined in com.google.android.gms.common.ConnectionResult.
  const int kTestData[] = {
      0,   // SUCCESS
      1,   // SERVICE_MISSING
      2,   // SERVICE_VERSION_UPDATE_REQUIRED
      3,   // SERVICE_DISABLED
      9,   // SERVICE_INVALID
      18,  // SERVICE_UPDATING
      19   // SERVICE_MISSING_PERMISSION
  };
  const Availability kExpected[7] = {kAvailabilityAvailable,
                                     kAvailabilityUnavailableMissing,
                                     kAvailabilityUnavailableUpdateRequired,
                                     kAvailabilityUnavailableDisabled,
                                     kAvailabilityUnavailableInvalid,
                                     kAvailabilityUnavailableUpdating,
                                     kAvailabilityUnavailablePermissions};
  // Now test each of the specific status.
  for (int i = 0; i < sizeof(kTestData) / sizeof(kTestData[0]); ++i) {
    EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                           firebase::testing::cppsdk::GetTestActivity()));
    std::string testdata = absl::StrFormat(
        "{"
        "  config:["
        "    {fake:'GoogleApiAvailability.isGooglePlayServicesAvailable',"
        "     futureint:{value:%d}}"
        "  ]"
        "}",
        kTestData[i]);
    firebase::testing::cppsdk::ConfigSet(testdata.c_str());
    EXPECT_EQ(kExpected[i],
              CheckAvailability(firebase::testing::cppsdk::GetTestJniEnv(),
                                firebase::testing::cppsdk::GetTestActivity()));
    Terminate(firebase::testing::cppsdk::GetTestJniEnv());
  }
}

TEST(AvailabilityAndroidTest, CheckAvailabilityCached) {
  const int kTestData[] = {
      0,   // SUCCESS
      1,   // SERVICE_MISSING
      2,   // SERVICE_VERSION_UPDATE_REQUIRED
  };
  const Availability kExpected = kAvailabilityAvailable;
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));
  for (int i = 0; i < sizeof(kTestData) / sizeof(kTestData[0]); ++i) {
    std::string testdata = absl::StrFormat(
        "{"
        "  config:["
        "    {fake:'GoogleApiAvailability.isGooglePlayServicesAvailable',"
        "     futureint:{value:%d}}"
        "  ]"
        "}",
        kTestData[i]);
    firebase::testing::cppsdk::ConfigSet(testdata.c_str());
    EXPECT_EQ(kExpected,
              CheckAvailability(firebase::testing::cppsdk::GetTestJniEnv(),
                                firebase::testing::cppsdk::GetTestActivity()));
  }
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

TEST(AvailabilityAndroidTest, MakeAvailableAlreadyAvailable) {
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));
  // Google play services are already available.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'GoogleApiAvailabilityHelper.makeGooglePlayServicesAvailable',"
      "     futurebool:{value:True}, futureint:{value:0, ticker:0}}"
      "  ]"
      "}");
  {
    firebase::Future<void> result = MakeAvailable(
        firebase::testing::cppsdk::GetTestJniEnv(),
        firebase::testing::cppsdk::GetTestActivity());
    WaitForFutureWithTimeout(result);
    EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
    EXPECT_EQ(0, result.error());
    EXPECT_STREQ("result code is 0", result.error_message());
  }
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

TEST(AvailabilityAndroidTest, MakeAvailableFailed) {
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));
  // We cannot make Google play services available.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'GoogleApiAvailabilityHelper.makeGooglePlayServicesAvailable',"
      "     futurebool:{value:False}, futureint:{value:0, ticker:-1}}"
      "  ]"
      "}");
  {
    firebase::Future<void> result = MakeAvailable(
        firebase::testing::cppsdk::GetTestJniEnv(),
        firebase::testing::cppsdk::GetTestActivity());
    WaitForFutureWithTimeout(result);
    EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
    EXPECT_EQ(-1, result.error());
    EXPECT_STREQ("Call to makeGooglePlayServicesAvailable failed.",
                 result.error_message());
  }
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

TEST(AvailabilityAndroidTest, MakeAvailableWithStatus) {
  EXPECT_TRUE(Initialize(firebase::testing::cppsdk::GetTestJniEnv(),
                         firebase::testing::cppsdk::GetTestActivity()));
  firebase::testing::cppsdk::TickerReset();
  // We try to make Google play services available. The only difference between
  // succeeded status and failed status is the result code. The logic is in the
  // java helper code and transparent to the C++ code. So here we use an
  // arbitrary status code 7 instead of testing each one by one.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'GoogleApiAvailabilityHelper.makeGooglePlayServicesAvailable',"
      "     futurebool:{value:True}, futureint:{value:7, ticker:1}}"
      "  ]"
      "}");
  {
    firebase::Future<void> result = MakeAvailable(
        firebase::testing::cppsdk::GetTestJniEnv(),
        firebase::testing::cppsdk::GetTestActivity());
    EXPECT_EQ(firebase::kFutureStatusPending, result.status());
    firebase::testing::cppsdk::TickerElapse();
    WaitForFutureWithTimeout(result);
    EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
    EXPECT_EQ(7, result.error());
    EXPECT_STREQ("result code is 7", result.error_message());
  }
  Terminate(firebase::testing::cppsdk::GetTestJniEnv());
}

}  // namespace google_play_services
