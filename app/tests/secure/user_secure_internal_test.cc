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

#include <string>

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
const char kUserData1Alt[] = "12345ABC";
const char kUserData1ReAdd[] = "123456789";
const char kAppName2[] = "app2";
const char kUserData2[] = "654321";
const char kAppNameNoExist[] = "app_no_exist";

const char kDomain[] = "internal_test";

// NOLINTNEXTLINE
const char kTestNameSpace[] = "com.google.firebase.TestKeys";
// NOLINTNEXTLINE
const char kTestNameSpaceShort[] = "firebase_test";

class UserSecureInternalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    user_secure_test_helper_ = MakeUnique<USER_SECURE_TEST_HELPER>();
    user_secure_ =
        MakeUnique<USER_SECURE_TYPE>(kDomain, USER_SECURE_TEST_NAMESPACE);
    CleanUpTestData();
  }

  void TearDown() override {
    CleanUpTestData();
    user_secure_ = nullptr;
    user_secure_test_helper_ = nullptr;
  }

  void CleanUpTestData() { user_secure_->DeleteAllData(); }

  UniquePtr<USER_SECURE_TYPE> user_secure_;
  UniquePtr<USER_SECURE_TEST_HELPER> user_secure_test_helper_;
};

TEST_F(UserSecureInternalTest, NoData) {
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(""));
}

TEST_F(UserSecureInternalTest, SetDataGetData) {
  // Add Data
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check the added key for correctness
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
}

TEST_F(UserSecureInternalTest, SetDataDeleteDataGetNoData) {
  // Add Data
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check save succeed.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  // Delete Data
  user_secure_->DeleteUserData(kAppName1);
  // Check data empty
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(""));
}

TEST_F(UserSecureInternalTest, SetTwoDataDeleteOneGetData) {
  // Add Data1
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check save succeed.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  // Add Data2
  user_secure_->SaveUserData(kAppName2, kUserData2);
  // Check save succeed.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName2), StrEq(kUserData2));
  // Check previous save is still valid.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  // Delete Data1
  user_secure_->DeleteUserData(kAppName1);
  // Check the data2
  EXPECT_THAT(user_secure_->LoadUserData(kAppName2), StrEq(kUserData2));
}

TEST_F(UserSecureInternalTest, CheckDeleteAll) {
  // Add Data1
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check save succeed.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  // Add Data2
  user_secure_->SaveUserData(kAppName2, kUserData2);
  // Check save succeed.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName2), StrEq(kUserData2));
  // Delete all data
  user_secure_->DeleteAllData();
  // Check data1 empty
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(""));
  // Check data2 empty
  EXPECT_THAT(user_secure_->LoadUserData(kAppName2), StrEq(""));
}

TEST_F(UserSecureInternalTest, SetGetAfterDeleteAll) {
  // Delete all data
  user_secure_->DeleteAllData();
  // Add Data1
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check data1 correctness.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
}

TEST_F(UserSecureInternalTest, AddOverride) {
  // Add Data1
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check data1 correctness.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  // Override same key with Data1ReAdd.
  user_secure_->SaveUserData(kAppName1, kUserData1ReAdd);
  // Check Data1ReAdd correctness.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1ReAdd));
}

TEST_F(UserSecureInternalTest, DeleteAndAddWithSameKey) {
  // Add Data1
  user_secure_->SaveUserData(kAppName1, kUserData1);
  // Check data1 correctness.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  // Delete Data1
  user_secure_->DeleteUserData(kAppName1);
  // Add Data1ReAdd to same key.
  user_secure_->SaveUserData(kAppName1, kUserData1ReAdd);
  // Check Data1ReAdd correctness.
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1ReAdd));
}

TEST_F(UserSecureInternalTest, DeleteKeyNotExist) {
  // Delete Data1
  user_secure_->DeleteUserData(kAppNameNoExist);
  // Check data1 empty
  EXPECT_THAT(user_secure_->LoadUserData(kAppNameNoExist), StrEq(""));
}

TEST_F(UserSecureInternalTest, SetLargeDataThenDeleteIt) {
  // Set up a large buffer of data.
  const size_t kSize = 20000;
  char data[kSize];
  for (int i = 0; i < kSize - 1; ++i) {
    data[i] = 'A' + (i % 26);
  }
  data[kSize - 1] = '\0';
  std::string user_data(data);
  // Add Data
  user_secure_->SaveUserData(kAppName1, user_data);
  // Check the added key for correctness
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(user_data));
  // Check that we can delete the large data.
  user_secure_->DeleteUserData(kAppName1);
  // Check the added key for correctness
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(""));
}

TEST_F(UserSecureInternalTest, TestMultipleDomains) {
  // Set up an alternate UserSecureInternal with a different domain.
  UniquePtr<USER_SECURE_TYPE> alt_user_secure = MakeUnique<USER_SECURE_TYPE>(
      "alternate_test", USER_SECURE_TEST_NAMESPACE);
  alt_user_secure->DeleteAllData();

  user_secure_->SaveUserData(kAppName1, kUserData1);
  user_secure_->SaveUserData(kAppName2, kUserData2);
  alt_user_secure->SaveUserData(kAppName1, kUserData1Alt);

  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1))
      << "Modifying a key in alt_user_secure changed a key in user_secure_";
  EXPECT_THAT(alt_user_secure->LoadUserData(kAppName1), StrEq(kUserData1Alt));
  EXPECT_THAT(alt_user_secure->LoadUserData(kAppName2), StrEq(""));

  // Ensure deleting data from one UserSecureInternal doesn't delete data in the
  // other.
  alt_user_secure->DeleteUserData(kAppName1);
  EXPECT_THAT(alt_user_secure->LoadUserData(kAppName1), StrEq(""));
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));

  alt_user_secure->SaveUserData(kAppName1, kUserData1Alt);
  alt_user_secure->SaveUserData(kAppName2, kUserData2);
  // Ensure deleting ALL data from one UserSecureInternal doesn't delete the
  // other.
  alt_user_secure->DeleteAllData();
  EXPECT_THAT(user_secure_->LoadUserData(kAppName1), StrEq(kUserData1));
  EXPECT_THAT(user_secure_->LoadUserData(kAppName2), StrEq(kUserData2));
  EXPECT_THAT(alt_user_secure->LoadUserData(kAppName1), StrEq(""));
  EXPECT_THAT(alt_user_secure->LoadUserData(kAppName2), StrEq(""));
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
