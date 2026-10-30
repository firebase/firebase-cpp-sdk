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

// Tests for the macOS desktop Firebase Apple SDK backend that don't require
// the Apple SDK to be embedded (this test binary does not embed it).

#include "analytics/src/analytics_desktop_apple.h"

#include <cstdlib>
#include <string>

#include "analytics/src/include/firebase/analytics.h"
#include "app/src/include/firebase/app.h"
#include "app/src/time.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"

namespace firebase {
namespace analytics {
namespace internal {
namespace apple {

TEST(AnalyticsDesktopAppleTest, ParseBackendMode) {
  EXPECT_EQ(ParseBackendMode(nullptr), kBackendModeAuto);
  EXPECT_EQ(ParseBackendMode(""), kBackendModeAuto);
  EXPECT_EQ(ParseBackendMode("bogus"), kBackendModeAuto);
  for (const char* value : {"disabled", "DISABLED", "0", "off", "false"}) {
    EXPECT_EQ(ParseBackendMode(value), kBackendModeDisabled) << value;
  }
  for (const char* value : {"enabled", "Enabled", "1", "on", "true"}) {
    EXPECT_EQ(ParseBackendMode(value), kBackendModeEnabled) << value;
  }
  EXPECT_EQ(ParseBackendMode("debug"), kBackendModeDebug);
  EXPECT_EQ(ParseBackendMode("Debug"), kBackendModeDebug);
}

TEST(AnalyticsDesktopAppleTest, IsAppleGoogleAppId) {
  EXPECT_TRUE(IsAppleGoogleAppId("1:1234567890:ios:321abc456def7890"));
  EXPECT_FALSE(IsAppleGoogleAppId("1:1234567890:android:321abc456def7890"));
  EXPECT_FALSE(IsAppleGoogleAppId("1:1234567890:web:321abc456def7890"));
  EXPECT_FALSE(IsAppleGoogleAppId(""));
  EXPECT_FALSE(IsAppleGoogleAppId(nullptr));
}

class AnalyticsDesktopAppleFallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Even when explicitly requested, the backend must not activate without
    // the Apple SDK linked in.
    SetBackendMode(kBackendModeEnabled);
    app_ = testing::CreateApp();
    analytics::Initialize(*app_);
  }

  void TearDown() override {
    analytics::Terminate();
    delete app_;
    app_ = nullptr;
    SetBackendMode(kBackendModeAuto);
  }

  App* app_ = nullptr;
};

TEST_F(AnalyticsDesktopAppleFallbackTest, FallsBackToStubsWithoutAppleSdk) {
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(analytics::IsDesktopInitialized());

  // Stub behavior is unchanged.
  Future<std::string> instance_id = analytics::GetAnalyticsInstanceId();
  for (int i = 0; i < 1000 && instance_id.status() != kFutureStatusComplete;
       ++i) {
    ::firebase::internal::Sleep(1);
  }
  ASSERT_EQ(instance_id.status(), kFutureStatusComplete);
  EXPECT_EQ(std::string("FakeAnalyticsInstanceId0"), *instance_id.result());

  // These must be safe no-ops.
  analytics::LogEvent("test_event", "param", "value");
  analytics::SetConsent({{kConsentTypeAnalyticsStorage, kConsentStatusDenied}});
  analytics::SetDefaultEventParameters(nullptr, 0);
  analytics::SetDesktopDebugMode(false);
}

}  // namespace apple
}  // namespace internal
}  // namespace analytics
}  // namespace firebase
