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

// Tests for the macOS desktop Firebase Apple SDK Installations backend that do
// not require the Apple SDK to be embedded (this test binary does not embed
// it).

#include "installations/src/desktop/installations_desktop_apple.h"

#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/time.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "installations/src/include/firebase/installations.h"

namespace firebase {
namespace installations {
namespace internal {
namespace apple {

TEST(InstallationsDesktopAppleTest, ParseBackendMode) {
  EXPECT_EQ(ParseBackendMode(nullptr), kBackendModeAuto);
  EXPECT_EQ(ParseBackendMode(""), kBackendModeAuto);
  EXPECT_EQ(ParseBackendMode("bogus"), kBackendModeAuto);
  for (const char* value : {"disabled", "DISABLED", "0", "off", "false"}) {
    EXPECT_EQ(ParseBackendMode(value), kBackendModeDisabled) << value;
  }
  for (const char* value :
       {"enabled", "Enabled", "1", "on", "true", "debug", "Debug"}) {
    EXPECT_EQ(ParseBackendMode(value), kBackendModeEnabled) << value;
  }
}

TEST(InstallationsDesktopAppleTest, IsAppleGoogleAppId) {
  EXPECT_TRUE(IsAppleGoogleAppId("1:1234567890:ios:321abc456def7890"));
  EXPECT_FALSE(IsAppleGoogleAppId("1:1234567890:android:321abc456def7890"));
  EXPECT_FALSE(IsAppleGoogleAppId("1:1234567890:web:321abc456def7890"));
  EXPECT_FALSE(IsAppleGoogleAppId(""));
  EXPECT_FALSE(IsAppleGoogleAppId(nullptr));
}

class InstallationsDesktopAppleFallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SetBackendMode(kBackendModeEnabled);
    app_ = testing::CreateApp();
  }

  void TearDown() override {
    delete app_;
    app_ = nullptr;
    SetBackendMode(kBackendModeAuto);
  }

  App* app_ = nullptr;
};

TEST_F(InstallationsDesktopAppleFallbackTest, FallsBackToStubsWithoutAppleSdk) {
  auto* installations = Installations::GetInstance(app_);
  ASSERT_NE(installations, nullptr);

  Future<std::string> id = installations->GetId();
  for (int i = 0; i < 1000 && id.status() != kFutureStatusComplete; ++i) {
    ::firebase::internal::Sleep(1);
  }
  ASSERT_EQ(id.status(), kFutureStatusComplete);
  EXPECT_EQ(std::string("FakeId"), *id.result());

  Future<std::string> token = installations->GetToken(false);
  for (int i = 0; i < 1000 && token.status() != kFutureStatusComplete; ++i) {
    ::firebase::internal::Sleep(1);
  }
  ASSERT_EQ(token.status(), kFutureStatusComplete);
  EXPECT_EQ(std::string("FakeToken"), *token.result());

  Future<void> del = installations->Delete();
  for (int i = 0; i < 1000 && del.status() != kFutureStatusComplete; ++i) {
    ::firebase::internal::Sleep(1);
  }
  ASSERT_EQ(del.status(), kFutureStatusComplete);
  ASSERT_EQ(installations->DeleteLastResult().status(), kFutureStatusComplete);

  delete installations;
}

TEST_F(InstallationsDesktopAppleFallbackTest,
       RapidCreateDeleteWhileFuturesExistIsSafe) {
  for (int cycle = 0; cycle < 5; ++cycle) {
    auto* installations = Installations::GetInstance(app_);
    ASSERT_NE(installations, nullptr);
    Future<std::string> id = installations->GetId();
    Future<std::string> token = installations->GetToken(true);
    Future<void> del = installations->Delete();
    delete installations;
  }
}

}  // namespace apple
}  // namespace internal
}  // namespace installations
}  // namespace firebase
