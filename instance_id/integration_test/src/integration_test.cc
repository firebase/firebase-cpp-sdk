// Copyright 2019 Google Inc. All rights reserved.
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
#include "firebase/instance_id.h"
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

using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

class FirebaseInstanceIdTest : public FirebaseTest {
 public:
  FirebaseInstanceIdTest();
  ~FirebaseInstanceIdTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App and Firebase IID.
  void Initialize();
  // Shut down Firebase IID and Firebase App.
  void Terminate();

  bool initialized_;
  firebase::instance_id::InstanceId* instance_id_;
};

FirebaseInstanceIdTest::FirebaseInstanceIdTest()
    : initialized_(false), instance_id_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseInstanceIdTest::~FirebaseInstanceIdTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(instance_id_ == nullptr);
}

void FirebaseInstanceIdTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
}

void FirebaseInstanceIdTest::TearDown() {
  // Delete the shared path, if there is one.
  if (initialized_) {
    Terminate();
  }
  FirebaseTest::TearDown();
}

void FirebaseInstanceIdTest::Initialize() {
  if (initialized_) return;

  InitializeApp();

  LogDebug("Initializing Firebase Instance ID.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      app_, &instance_id_, [](::firebase::App* app, void* target) {
        LogDebug("Try to initialize Firebase Instance ID");
        firebase::InitResult result;
        firebase::instance_id::InstanceId** iid_ptr =
            reinterpret_cast<firebase::instance_id::InstanceId**>(target);
        *iid_ptr =
            firebase::instance_id::InstanceId::GetInstanceId(app, &result);
        return result;
      });

  FirebaseTest::WaitForCompletion(initializer.InitializeLastResult(),
                                  "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Instance ID.");

  initialized_ = true;
}

void FirebaseInstanceIdTest::Terminate() {
  if (!initialized_) return;

  if (instance_id_) {
    LogDebug("Shutdown the Instance ID library.");
    delete instance_id_;
    instance_id_ = nullptr;
  }

  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

TEST_F(FirebaseInstanceIdTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseInstanceIdTest, TestCanGetId) {
  firebase::Future<std::string> id = instance_id_->GetId();
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");
}

TEST_F(FirebaseInstanceIdTest, TestGettingIdTwiceMatches) {
  firebase::Future<std::string> id = instance_id_->GetId();
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");
  std::string first_id = *id.result();
  id = instance_id_->GetId();
  WaitForCompletion(id, "GetId 2");
  EXPECT_EQ(*id.result(), first_id);
}
TEST_F(FirebaseInstanceIdTest, TestDeleteIdGivesNewIdNextTime) {
  firebase::Future<std::string> id = instance_id_->GetId();
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");
  std::string first_id = *id.result();

  // Try deleting the IID, but it can sometimes fail due to
  // sporadic network issues, so allow retrying.
  firebase::Future<void> del =
    RunWithRetry<void>([](firebase::instance_id::InstanceId* iid) {
                         return iid->DeleteId();
                 }, instance_id_);
  WaitForCompletion(del, "DeleteId");

  // Ensure that we now get a different IID.
  id = instance_id_->GetId();
  WaitForCompletion(id, "GetId 2");
  EXPECT_NE(*id.result(), "");
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // Desktop is a stub and returns the same ID, but on mobile it should
  // return a new ID.
  EXPECT_NE(*id.result(), first_id);
#endif  // defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)
}

TEST_F(FirebaseInstanceIdTest, TestCanGetToken) {
  firebase::Future<std::string> token = instance_id_->GetToken();
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");
}

TEST_F(FirebaseInstanceIdTest, TestGettingTokenTwiceMatches) {
  firebase::Future<std::string> token = instance_id_->GetToken();
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");
  std::string first_token = *token.result();
  token = instance_id_->GetToken();
  WaitForCompletion(token, "GetToken 2");
  EXPECT_EQ(*token.result(), first_token);
}

// Test disabled due to flakiness (b/143697451).
TEST_F(FirebaseInstanceIdTest, DISABLED_TestDeleteTokenGivesNewTokenNextTime) {
  firebase::Future<std::string> token = instance_id_->GetToken();
  WaitForCompletion(token, "GetToken");
  EXPECT_NE(*token.result(), "");
  std::string first_token = *token.result();

  firebase::Future<void> del = instance_id_->DeleteToken();
  WaitForCompletion(del, "DeleteToken");

  // Ensure that we now get a different IID.
  token = instance_id_->GetToken();
  WaitForCompletion(token, "GetToken 2");
  EXPECT_NE(*token.result(), "");
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // Desktop is a stub and returns the same token, but on mobile it should
  // return a new token.
  EXPECT_NE(*token.result(), first_token);
#endif  // defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)
}

TEST_F(FirebaseInstanceIdTest, TestCanGetIdAndTokenTogether) {
  firebase::Future<std::string> id = instance_id_->GetId();
  firebase::Future<std::string> token = instance_id_->GetToken();
  WaitForCompletion(token, "GetToken");
  WaitForCompletion(id, "GetId");
  EXPECT_NE(*id.result(), "");
  EXPECT_NE(*token.result(), "");
}

}  // namespace firebase_testapp_automated
