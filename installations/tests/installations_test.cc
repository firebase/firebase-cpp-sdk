// Copyright 2020 Google LLC
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

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#include <unistd.h>
#define __ANDROID__
#include <jni.h>

#include <functional>

#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif  // defined(__APPLE__)

// [START installations_includes]
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "installations/src/include/firebase/installations.h"
// [END installations_includes]

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/reporter.h"
#include "testing/ticker.h"

using testing::Eq;
using testing::NotNull;

namespace firebase {
namespace installations {

class InstallationsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
    reporter_.reset();

    firebase_app_ = testing::CreateApp();
    EXPECT_NE(firebase_app_, nullptr) << "Init app failed";
  }

  void TearDown() override {
    delete firebase_app_;
    firebase_app_ = nullptr;

    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  // Wait for a future up to the specified number of milliseconds.
  template <typename T>
  static void WaitForFutureWithTimeout(
      const Future<T>& future,
      int timeout_milliseconds = kFutureTimeoutMilliseconds,
      FutureStatus expected_status = kFutureStatusComplete) {
    while (future.status() != expected_status && timeout_milliseconds-- > 0) {
      ::firebase::internal::Sleep(1);
    }
  }

  // Validate that a future completed successfully and has the specified
  // result.
  template <typename T>
  static void CheckSuccessWithValue(const Future<T>& future, const T& result) {
    WaitForFutureWithTimeout<T>(future);
    EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
    EXPECT_THAT(*future.result(), Eq(result));
  }

  // Validate that a future completed successfully.
  static void CheckSuccess(const Future<void>& future) {
    WaitForFutureWithTimeout<void>(future);
    EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
  }

  App* firebase_app_ = nullptr;
  firebase::testing::cppsdk::Reporter reporter_;
  // Default time to wait for future status changes.
  static const int kFutureTimeoutMilliseconds = 1000;
};

// Check SetUp and TearDown working well.
TEST_F(InstallationsTest, InitializeAndTerminate) {
  auto installations =
      absl::WrapUnique(Installations::GetInstance(firebase_app_));
  EXPECT_THAT(installations, NotNull());
}

TEST_F(InstallationsTest, InitializeTwice) {
  auto installations1 =
      absl::WrapUnique(Installations::GetInstance(firebase_app_));
  EXPECT_THAT(installations1, NotNull());

  auto installations2 =
      absl::WrapUnique(Installations::GetInstance(firebase_app_));
  EXPECT_THAT(installations2, NotNull());

  EXPECT_EQ(installations1, installations2);
}

TEST_F(InstallationsTest, GetId) {
  auto* installations = Installations::GetInstance(firebase_app_);
  const std::string expected_value("FakeId");
  CheckSuccessWithValue(installations->GetId(), expected_value);
  CheckSuccessWithValue(installations->GetIdLastResult(), expected_value);
  delete installations;
}

TEST_F(InstallationsTest, GetToken) {
  auto* installations = Installations::GetInstance(firebase_app_);
  const std::string expected_value("FakeToken");
  CheckSuccessWithValue(installations->GetToken(false), expected_value);
  CheckSuccessWithValue(installations->GetTokenLastResult(), expected_value);
  delete installations;
}

TEST_F(InstallationsTest, GetTokenForceRefresh) {
  auto* installations = Installations::GetInstance(firebase_app_);
  const std::string expected_value("FakeTokenForceRefresh");
  CheckSuccessWithValue(installations->GetToken(false), expected_value);
  CheckSuccessWithValue(installations->GetTokenLastResult(), expected_value);
  delete installations;
}

}  // namespace installations
}  // namespace firebase
