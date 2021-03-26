// Copyright 2019 Google LLC
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

#include <future>  // NOLINT
#include <string>

#include "app/src/secure/user_secure_internal.h"
#include "app/src/secure/user_secure_manager.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

// If FORCE_FAKE_SECURE_STORAGE is defined, force usage of fake (non-secure)
// storage, suitable for testing only, NOT for production use. Otherwise, use
// the default secure storage type for each platform, except on Linux if not
// running locally, which also forces fake storage (as libsecret requires that
// you are running locally), or on unknown other platforms (as there is no
// platform-independent secure storage solution).

#if !defined(FORCE_FAKE_SECURE_STORAGE)
#if defined(_WIN32)
#include "app/src/secure/user_secure_windows_internal.h"
#define USER_SECURE_TYPE UserSecureWindowsInternal
#define USER_SECURE_TEST_HELPER UserSecureEmptyTestHelper
#define USER_SECURE_TEST_NAMESPACE kTestNameSpace

#elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
#include "app/src/secure/user_secure_darwin_internal.h"
#include "app/src/secure/user_secure_darwin_internal_testlib.h"
#define USER_SECURE_TYPE UserSecureDarwinInternal
#define USER_SECURE_TEST_HELPER UserSecureDarwinTestHelper
#define USER_SECURE_TEST_NAMESPACE kTestNameSpace

#elif defined(__linux__) && defined(USER_SECURE_LOCAL_TEST)
#include "app/src/secure/user_secure_linux_internal.h"
#define USER_SECURE_TYPE UserSecureLinuxInternal
#define USER_SECURE_TEST_HELPER UserSecureEmptyTestHelper
#define USER_SECURE_TEST_NAMESPACE kTestNameSpace

#else  // Unknown platform, or linux test running non-locally, use fake version.
#define FORCE_FAKE_SECURE_STORAGE
#endif  // platform selector
#endif  // !defined(FORCE_FAKE_SECURE_STORAGE)

#ifdef FORCE_FAKE_SECURE_STORAGE
#include "app/src/secure/user_secure_fake_internal.h"
#define USER_SECURE_TYPE UserSecureFakeInternal
#define USER_SECURE_TEST_HELPER UserSecureEmptyTestHelper
#define USER_SECURE_TEST_NAMESPACE GetTestTmpDir(kTestNameSpaceShort).c_str()
#if defined(_WIN32)
// For GetEnvironmentVariable to read TEST_TEMPDIR.
#include <winbase.h>
#else
#include <stdlib.h>
#endif  // defined(_WIN32)
#endif  // FORCE_FAKE_SECURE_STORAGE

namespace firebase {
namespace app {
namespace secure {

using ::testing::Eq;
using ::testing::StrEq;

class UserSecureEmptyTestHelper {};

#if defined(_WIN32)
static const char kDirectorySeparator[] = "\\";
#else
static const char kDirectorySeparator[] = "/";
#endif  // defined(_WIN32)

static std::string GetTestTmpDir(const char test_namespace[]) {
#if defined(_WIN32)
  char buf[MAX_PATH + 1];
  if (GetEnvironmentVariable("TEST_TMPDIR", buf, sizeof(buf))) {
    return std::string(buf) + kDirectorySeparator + test_namespace;
  }
#else
  // Linux and OS X should either have the TEST_TMPDIR environment variable set.
  if (const char* value = getenv("TEST_TMPDIR")) {
    return std::string(value) + kDirectorySeparator + test_namespace;
  }
#endif  // defined(_WIN32)
  // If we weren't able to get TEST_TMPDIR, just use a subdirectory.
  return test_namespace;
}

// test app name and data
const char kAppName1[] = "app1";
const char kUserData1[] = "123456";
const char kAppName2[] = "app2";
const char kUserData2[] = "654321";

const char kDomain[] = "integration_test";

// NOLINTNEXTLINE
const char kTestNameSpace[] = "com.google.firebase.TestKeys";
// NOLINTNEXTLINE
const char kTestNameSpaceShort[] = "firebase_test";

class UserSecureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    user_secure_test_helper_ = MakeUnique<USER_SECURE_TEST_HELPER>();
    UserSecureInternal* internal =
        new USER_SECURE_TYPE(kDomain, USER_SECURE_TEST_NAMESPACE);
    UniquePtr<UserSecureInternal> user_secure_ptr(internal);
    manager_ = new UserSecureManager(std::move(user_secure_ptr));
    CleanUpTestData();
  }

  void TearDown() override {
    CleanUpTestData();
    delete manager_;
  }

  void CleanUpTestData() {
    Future<void> delete_all_future = manager_->DeleteAllData();
    WaitForResponse(delete_all_future);
    user_secure_test_helper_ = nullptr;
  }

  // Busy waits until |response_future| has completed.
  void WaitForResponse(const FutureBase& response_future) {
    while (true) {
      if (response_future.status() != FutureStatus::kFutureStatusPending) {
        break;
      }
    }
  }

  UserSecureManager* manager_;
  UniquePtr<USER_SECURE_TEST_HELPER> user_secure_test_helper_;
};

TEST_F(UserSecureTest, NoData) {
  Future<std::string> load_future = manager_->LoadUserData(kAppName1);
  WaitForResponse(load_future);
  EXPECT_THAT(load_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(load_future.error(), kNoEntry);
  EXPECT_THAT(*(load_future.result()), StrEq(""));
}

TEST_F(UserSecureTest, SetDataGetData) {
  // Add Data
  Future<void> save_future = manager_->SaveUserData(kAppName1, kUserData1);
  WaitForResponse(save_future);
  EXPECT_THAT(save_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(save_future.error(), kSuccess);
  // Check the added key for correctness
  Future<std::string> load_future = manager_->LoadUserData(kAppName1);
  WaitForResponse(load_future);
  EXPECT_THAT(load_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(load_future.error(), kSuccess);
  std::string originalString(kUserData1);
  EXPECT_THAT(*(load_future.result()), StrEq(originalString));
}

TEST_F(UserSecureTest, SetDataDeleteDataGetNoData) {
  // Add Data
  Future<void> save_future = manager_->SaveUserData(kAppName1, kUserData1);
  WaitForResponse(save_future);
  EXPECT_THAT(save_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(save_future.error(), kSuccess);
  // Delete Data
  Future<void> delete_future = manager_->DeleteUserData(kAppName1);
  WaitForResponse(delete_future);
  EXPECT_THAT(delete_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(delete_future.error(), kSuccess);
  // Check data empty
  Future<std::string> load_future = manager_->LoadUserData(kAppName1);
  WaitForResponse(load_future);
  EXPECT_THAT(load_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(load_future.error(), kNoEntry);
  EXPECT_THAT(*(load_future.result()), StrEq(""));
}

TEST_F(UserSecureTest, SetTwoDataDeleteOneGetData) {
  // Add Data1
  Future<void> save_future1 = manager_->SaveUserData(kAppName1, kUserData1);
  WaitForResponse(save_future1);
  EXPECT_THAT(save_future1.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(save_future1.error(), kSuccess);
  // Add Data2
  Future<void> save_future2 = manager_->SaveUserData(kAppName2, kUserData2);
  WaitForResponse(save_future2);
  EXPECT_THAT(save_future2.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(save_future2.error(), kSuccess);

  // Delete Data1
  Future<void> delete_future = manager_->DeleteUserData(kAppName1);
  WaitForResponse(delete_future);
  EXPECT_THAT(delete_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(delete_future.error(), kSuccess);

  // Check the data2
  Future<std::string> load_future = manager_->LoadUserData(kAppName2);
  WaitForResponse(load_future);
  EXPECT_THAT(load_future.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(load_future.error(), kSuccess);
  std::string originalString(kUserData2);
  EXPECT_THAT(*(load_future.result()), StrEq(originalString));
}

TEST_F(UserSecureTest, CheckDeleteAll) {
  // Add Data1
  Future<void> save_future1 = manager_->SaveUserData(kAppName1, kUserData1);
  WaitForResponse(save_future1);
  EXPECT_THAT(save_future1.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(save_future1.error(), kSuccess);
  // Add Data2
  Future<void> save_future2 = manager_->SaveUserData(kAppName2, kUserData2);
  WaitForResponse(save_future2);
  EXPECT_THAT(save_future2.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(save_future2.error(), kSuccess);

  // Delete all data
  Future<void> delete_all_future = manager_->DeleteAllData();
  WaitForResponse(delete_all_future);
  EXPECT_THAT(delete_all_future.status(),
              Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(delete_all_future.error(), kSuccess);
  // Check data1 empty
  Future<std::string> load_future1 = manager_->LoadUserData(kAppName1);
  WaitForResponse(load_future1);
  EXPECT_THAT(load_future1.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(load_future1.error(), kNoEntry);
  EXPECT_THAT(*(load_future1.result()), StrEq(""));

  // Check data2 empty
  Future<std::string> load_future2 = manager_->LoadUserData(kAppName2);
  WaitForResponse(load_future2);
  EXPECT_THAT(load_future2.status(), Eq(FutureStatus::kFutureStatusComplete));
  EXPECT_THAT(load_future2.error(), kNoEntry);
  EXPECT_THAT(*(load_future2.result()), StrEq(""));
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
