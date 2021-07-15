// Copyright 2020 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <inttypes.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/installations.h"
#include "firebase/util.h"
#include "firebase_test_framework.h"  // NOLINT

// The TO_STRING macro is useful for command line defined strings as the quotes
// get stripped.
#define TO_STRING_EXPAND(X) #X
#define TO_STRING(X) TO_STRING_EXPAND(X)

// Path to the Firebase config file to load.
#ifdef FIREBASE_CONFIG
#define FIREBASE_CONFIG_STRING TO_STRING(FIREBASE_CONFIG)
#else
#define FIREBASE_CONFIG_STRING ""
#endif  // FIREBASE_CONFIG

namespace firebase_testapp_automated {

using app_framework::LogDebug;
using app_framework::LogError;

using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

class FirebaseInstallationsTest : public FirebaseTest {
 public:
  FirebaseInstallationsTest();
  ~FirebaseInstallationsTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App and Firebase FIS.
  void Initialize();
  // Shut down Firebase FIS and Firebase App.
  void Terminate();

  bool initialized_;
  firebase::installations::Installations* installations_;
};

FirebaseInstallationsTest::FirebaseInstallationsTest()
    : initialized_(false), installations_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseInstallationsTest::~FirebaseInstallationsTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(installations_ == nullptr);
}

void FirebaseInstallationsTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
}

void FirebaseInstallationsTest::TearDown() {
  // Delete the shared path, if there is one.
  if (initialized_) {
    Terminate();
  }
  FirebaseTest::TearDown();
}

void FirebaseInstallationsTest::Initialize() {
  if (initialized_) return;

  InitializeApp();

  LogDebug("Initializing Firebase Installations.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      app_, &installations_, [](::firebase::App* app, void* target) {
        LogDebug("Try to initialize Firebase Installations");
        firebase::installations::Installations** installations_ptr =
            reinterpret_cast<firebase::installations::Installations**>(target);
        *installations_ptr =
            firebase::installations::Installations::GetInstance(app);
        return firebase::kInitResultSuccess;
      });

  FirebaseTest::WaitForCompletion(initializer.InitializeLastResult(),
                                  "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Installations.");

  initialized_ = true;
}

void FirebaseInstallationsTest::Terminate() {
  if (!initialized_) return;

  if (installations_) {
    LogDebug("Shutdown the Installations library.");
    delete installations_;
    installations_ = nullptr;
  }

  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

TEST_F(FirebaseInstallationsTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseInstallationsTest, TestCanGetId) {
  firebase::Future<std::string> id = installations_->GetId();
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");
}

TEST_F(FirebaseInstallationsTest, TestGettingIdTwiceMatches) {
  FLAKY_TEST_SECTION_BEGIN();

  firebase::Future<std::string> id = installations_->GetId();
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");  // ensure non-blank
  std::string first_id = *id.result();
  id = installations_->GetId();
  WaitForCompletion(id, "GetId 2");
  EXPECT_NE(*id.result(), "");  // ensure non-blank

  // Ensure the second ID returned is the same as the first.
  EXPECT_EQ(*id.result(), first_id);

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseInstallationsTest, TestDeleteGivesNewIdNextTime) {
  FLAKY_TEST_SECTION_BEGIN();

  firebase::Future<std::string> id = installations_->GetId();
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");  // ensure non-blank
  std::string first_id = *id.result();

  firebase::Future<void> del = installations_->Delete();
  WaitForCompletion(del, "Delete");

  // Ensure that we now get a different installations id.
  id = installations_->GetId();
  WaitForCompletion(id, "GetId 2");
  EXPECT_NE(*id.result(), "");  // ensure non-blank
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // Desktop is a stub and returns the same ID, but on mobile it
  // should return a new ID.
  EXPECT_NE(*id.result(), first_id);
#endif  // defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseInstallationsTest, TestCanGetToken) {
  firebase::Future<std::string> token = installations_->GetToken(true);
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");
}

TEST_F(FirebaseInstallationsTest, TestGettingTokenTwiceMatches) {
  FLAKY_TEST_SECTION_BEGIN();

  firebase::Future<std::string> token = installations_->GetToken(false);
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");  // ensure non-blank
  std::string first_token = *token.result();
  token = installations_->GetToken(false);
  WaitForCompletion(token, "GetToken 2");
  EXPECT_NE(*token.result(), "");  // ensure non-blank
  EXPECT_EQ(*token.result(), first_token);

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseInstallationsTest, TestDeleteGivesNewTokenNextTime) {
  FLAKY_TEST_SECTION_BEGIN();

  firebase::Future<std::string> token = installations_->GetToken(false);
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");  // ensure non-blank
  std::string first_token = *token.result();

  firebase::Future<void> del = installations_->Delete();
  WaitForCompletion(del, "Delete");

  // Ensure that we now get a different installations token.
  token = installations_->GetToken(false);
  WaitForCompletion(token, "GetToken 2");
  EXPECT_NE(*token.result(), "");  // ensure non-blank
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // Desktop is a stub and returns the same token, but on mobile it
  // should return a new token.
  EXPECT_NE(*token.result(), first_token);
#endif  // defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseInstallationsTest, TestCanGetIdAndTokenTogether) {
  firebase::Future<std::string> id = installations_->GetId();
  firebase::Future<std::string> token = installations_->GetToken(false);
  WaitForCompletion(token, "GetToken");
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");
  EXPECT_NE(*token.result(), "");
}

TEST_F(FirebaseInstallationsTest, TestGetTokenForceRefresh) {
  firebase::Future<std::string> token = installations_->GetToken(false);
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");
  std::string first_token = *token.result();
  token = installations_->GetToken(true);
  WaitForCompletion(token, "GetToken 2");
  EXPECT_NE(*token.result(), first_token);
}

}  // namespace firebase_testapp_automated
