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
#define __ANDROID__
#include <jni.h>

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

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
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
    ASSERT_NE(firebase_app_, nullptr) << "Init app failed";

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
    reporter_.addExpectation("FirebaseInstallations.construct", "");
    reporter_.addExpectation("FirebaseInstallations.construct", "");
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    reporter_.addExpectation("FirebaseInstallations.initialize", "");
    reporter_.addExpectation("FirebaseInstallations.initialize", "");
#endif
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
      firebase::testing::cppsdk::TickerElapse();
    }
  }

  // Validate that a future completed successfully and has the specified
  // result.
  template <typename T>
  static void CheckSuccessWithValue(const Future<T>& future, const T& result) {
    WaitForFutureWithTimeout<T>(future);
    ASSERT_THAT(future.status(), Eq(kFutureStatusComplete));
    EXPECT_THAT(future.error(), Eq(kInstallationsErrorNone));
    ASSERT_THAT(future.result(), NotNull());
    EXPECT_THAT(*future.result(), Eq(result));
  }

  // Validate that a future completed successfully.
  static void CheckSuccess(const Future<void>& future) {
    WaitForFutureWithTimeout<void>(future);
    ASSERT_THAT(future.status(), Eq(kFutureStatusComplete));
    EXPECT_THAT(future.error(), Eq(kInstallationsErrorNone));
  }

  App* firebase_app_ = nullptr;
  firebase::testing::cppsdk::Reporter reporter_;
  // Default time to wait for future status changes.
  static const int kFutureTimeoutMilliseconds = 1000;
};

// Check SetUp and TearDown working well.
TEST_F(InstallationsTest, InitializeAndTerminate) {
  InitResult init_result = kInitResultFailedMissingDependency;
  auto* installations =
      Installations::GetInstance(firebase_app_, &init_result);
  ASSERT_THAT(installations, NotNull());
  EXPECT_EQ(init_result, kInitResultSuccess);
  EXPECT_TRUE(installations->is_valid());
  EXPECT_EQ(installations->app(), firebase_app_);
  delete installations;
}

TEST_F(InstallationsTest, InitializeNullAppReturnsNull) {
  // Override SetUp expectations since GetInstance(nullptr) does not initialize.
  reporter_.reset();
  InitResult init_result = kInitResultSuccess;
  EXPECT_EQ(Installations::GetInstance(nullptr, &init_result), nullptr);
  EXPECT_EQ(init_result, kInitResultFailedMissingDependency);
}

TEST_F(InstallationsTest, InitializeTwice) {
  auto* installations1 = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations1, NotNull());

#if defined(__APPLE__) && TARGET_OS_IPHONE
  reporter_.addExpectation("FirebaseInstallations.initialize", "");
#endif
  auto* installations2 = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations2, NotNull());

  EXPECT_EQ(installations1, installations2);
  delete installations1;
}

TEST_F(InstallationsTest, GetId) {
  auto* installations = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations, NotNull());
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  reporter_.addExpectation("FirebaseInstallations.getId", "");
#endif
  const std::string expected_value("FakeId");
  CheckSuccessWithValue(installations->GetId(), expected_value);
  CheckSuccessWithValue(installations->GetIdLastResult(), expected_value);
  delete installations;
}

TEST_F(InstallationsTest, GetToken) {
  auto* installations = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations, NotNull());
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  reporter_.addExpectation("FirebaseInstallations.getToken", "");
#endif
  const std::string expected_value("FakeToken");
  CheckSuccessWithValue(installations->GetToken(false), expected_value);
  CheckSuccessWithValue(installations->GetTokenLastResult(), expected_value);
  delete installations;
}

TEST_F(InstallationsTest, GetTokenForceRefresh) {
  auto* installations = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations, NotNull());
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  reporter_.addExpectation("FirebaseInstallations.getToken", "");
#endif
  const std::string expected_value("FakeTokenForceRefresh");
  CheckSuccessWithValue(installations->GetToken(true), expected_value);
  CheckSuccessWithValue(installations->GetTokenLastResult(), expected_value);
  delete installations;
}

TEST_F(InstallationsTest, GetTokenResult) {
  auto* installations = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations, NotNull());
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  reporter_.addExpectation("FirebaseInstallations.getToken", "");
#endif
  Future<InstallationTokenResult> future = installations->GetTokenResult(false);
  WaitForFutureWithTimeout(future);
  ASSERT_EQ(future.status(), kFutureStatusComplete);
  EXPECT_EQ(future.error(), kInstallationsErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->token, "FakeToken");
  EXPECT_EQ(installations->GetTokenResultLastResult().result()->token,
            "FakeToken");
  delete installations;
}

TEST_F(InstallationsTest, Delete) {
  auto* installations = Installations::GetInstance(firebase_app_);
  ASSERT_THAT(installations, NotNull());
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  reporter_.addExpectation("FirebaseInstallations.getId", "");
  reporter_.addExpectation("FirebaseInstallations.delete", "");
  reporter_.addExpectation("FirebaseInstallations.getId", "");
#endif
  CheckSuccessWithValue(installations->GetId(), std::string("FakeId"));
  CheckSuccess(installations->Delete());
  CheckSuccess(installations->DeleteLastResult());
  // Verify Delete() did not overwrite GetIdLastResult().
  CheckSuccessWithValue(installations->GetIdLastResult(),
                        std::string("FakeId"));
#if !defined(FIREBASE_ANDROID_FOR_DESKTOP) && \
    !(defined(__APPLE__) && TARGET_OS_IPHONE)
  // Desktop stub rotates ID after Delete().
  CheckSuccessWithValue(installations->GetId(), std::string("FakeId_1"));
#else
  CheckSuccessWithValue(installations->GetId(), std::string("FakeId"));
#endif
  delete installations;
}

TEST_F(InstallationsTest, ErrorMessages) {
  reporter_.reset();
  EXPECT_STREQ(GetErrorMessage(kErrorNone), "");
  EXPECT_NE(std::string(GetErrorMessage(kErrorFailure)), "");
  EXPECT_NE(std::string(GetErrorMessage(kErrorInvalidConfiguration)), "");
  EXPECT_NE(std::string(GetErrorMessage(kErrorUnableToLoad)), "");
  EXPECT_NE(std::string(GetErrorMessage(kErrorNoAccess)), "");
}

}  // namespace installations
}  // namespace firebase
