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

#include "app/src/secure/user_secure_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace app {
namespace secure {

using ::testing::Ne;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrEq;

const char kAppName1[] = "app_name_1";
const char kUserData1[] = "123456";

TEST(UserSecureManager, Constructor) {
  UniquePtr<UserSecureInternal> user_secure;
  UserSecureManager manager(std::move(user_secure));

  // Just making sure this constructor doesn't crash or leak memory. No further
  // tests.
}

class UserSecureInternalMock : public UserSecureInternal {
 public:
  MOCK_METHOD(std::string, LoadUserData, (const std::string& app_name),
              (override));
  MOCK_METHOD(void, SaveUserData,
              (const std::string& app_name, const std::string& user_data),
              (override));
  MOCK_METHOD(void, DeleteUserData, (const std::string& app_name), (override));
  MOCK_METHOD(void, DeleteAllData, (), (override));
};

class UserSecureManagerTest : public ::testing::Test {
 public:
  friend class UserSecureManager;
  void SetUp() override {
    user_secure_ = new testing::StrictMock<UserSecureInternalMock>();
    UniquePtr<UserSecureInternalMock> user_secure_ptr(user_secure_);

    manager_ = new UserSecureManager(std::move(user_secure_ptr));
  }

  void TearDown() override { delete manager_; }

  // Busy waits until |response_future| has completed.
  void WaitForResponse(const FutureBase& response_future) {
    ASSERT_THAT(response_future.status(),
                Ne(FutureStatus::kFutureStatusInvalid));
    while (true) {
      if (response_future.status() != FutureStatus::kFutureStatusPending) {
        break;
      }
    }
  }

 protected:
  UserSecureInternalMock* user_secure_;
  UserSecureManager* manager_;
};

TEST_F(UserSecureManagerTest, LoadUserData) {
  EXPECT_CALL(*user_secure_, LoadUserData(kAppName1))
      .WillOnce(Return(kUserData1));
  Future<std::string> load_future = manager_->LoadUserData(kAppName1);
  WaitForResponse(load_future);
  EXPECT_EQ(load_future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_THAT(load_future.result(), Pointee(StrEq(kUserData1)));
}

TEST_F(UserSecureManagerTest, SaveUserData) {
  EXPECT_CALL(*user_secure_, SaveUserData(kAppName1, kUserData1)).Times(1);
  Future<void> save_future = manager_->SaveUserData(kAppName1, kUserData1);
  WaitForResponse(save_future);
  EXPECT_EQ(save_future.status(), FutureStatus::kFutureStatusComplete);
}

TEST_F(UserSecureManagerTest, DeleteUserData) {
  EXPECT_CALL(*user_secure_, DeleteUserData(kAppName1)).Times(1);
  Future<void> delete_future = manager_->DeleteUserData(kAppName1);
  WaitForResponse(delete_future);
  EXPECT_EQ(delete_future.status(), FutureStatus::kFutureStatusComplete);
}

TEST_F(UserSecureManagerTest, DeleteAllData) {
  EXPECT_CALL(*user_secure_, DeleteAllData()).Times(1);
  Future<void> delete_all_future = manager_->DeleteAllData();
  WaitForResponse(delete_all_future);

  EXPECT_EQ(delete_all_future.status(), FutureStatus::kFutureStatusComplete);
}

TEST_F(UserSecureManagerTest, TestHexEncodingAndDecoding) {
  const char kBinaryData[] =
      "\x00\x05\x20\x3C\x40\x45\x50\x60\x70\x80\x90\x00\xA0\xB5\xC2\xD1\xF0"
      "\xFF\x00\xE0\x42";
  const char kBase64EncodedData[] = "#AAUgPEBFUGBwgJAAoLXC0fD/AOBC";
  const char kHexEncodedData[] = "$0005203C4045506070809000A0B5C2D1F0FF00E042";
  std::string binary_data(kBinaryData, sizeof(kBinaryData) - 1);
  std::string encoded;
  std::string decoded;

  UserSecureManager::BinaryToAscii(binary_data, &encoded);
  // Ensure that the data was Base64-encoded.
  EXPECT_EQ(encoded, kBase64EncodedData);
  // Ensure the data decodes back to the original.
  EXPECT_TRUE(UserSecureManager::AsciiToBinary(encoded, &decoded));
  EXPECT_EQ(decoded, binary_data);

  // Explicitly check decoding from hex and from base64.
  {
    std::string decoded_from_hex;
    EXPECT_TRUE(
        UserSecureManager::AsciiToBinary(kHexEncodedData, &decoded_from_hex));
    EXPECT_EQ(decoded_from_hex, binary_data);
  }
  {
    std::string decoded_from_base64;
    EXPECT_TRUE(UserSecureManager::AsciiToBinary(kBase64EncodedData,
                                                 &decoded_from_base64));
    EXPECT_EQ(decoded_from_base64, binary_data);
  }

  // Test encoding and decoding empty strings.
  std::string empty;
  UserSecureManager::BinaryToAscii("", &empty);
  EXPECT_EQ(empty, "#");
  EXPECT_TRUE(UserSecureManager::AsciiToBinary("#", &empty));
  EXPECT_EQ(empty, "");
  EXPECT_TRUE(UserSecureManager::AsciiToBinary("$", &empty));
  EXPECT_EQ(empty, "");

  std::string u;  // unused

  // Bad hex encodings.
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$11223", &u));  // odd size after header
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("ABCDEF01", &u));  // missing header
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$1A2GB34F", &u));  // bad characters
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$1A:23A4F", &u));  // bad characters
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$1A23A4$F", &u));  // bad characters
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$1A2BG34F", &u));  // bad characters
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$1A2:3A4F", &u));  // bad characters
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("$1A23A4F!", &u));  // bad characters

  // Bad base64 encodings.
  EXPECT_FALSE(UserSecureManager::AsciiToBinary("#*", &u));  // invalid base64
  EXPECT_FALSE(
      UserSecureManager::AsciiToBinary("#AAAA#AAAA", &u));  // bad characters
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
