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
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <thread>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/functions.h"
#include "firebase/messaging.h"
#include "firebase/util.h"
#include "firebase_test_framework.h"  // NOLINT

#ifdef _MSC_VER
// Windows uses _stricmp instead of strcasecmp.
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif  // _MSC_VER

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

const char kNotificationLinkKey[] = "gcm.n.link";
const char kTestLink[] = "https://this-is-a-test-link/";

// Give each operation approximately 120 seconds before failing.
const int kTimeoutSeconds = 120;
const char kTestingNotificationKey[] = "fcm_testing_notification";

using app_framework::LogDebug;
using app_framework::LogError;
using app_framework::LogInfo;
using app_framework::LogWarning;

using app_framework::GetCurrentTimeInMicroseconds;
using app_framework::PathForResource;
using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

class FirebaseMessagingTest : public FirebaseTest {
 public:
  FirebaseMessagingTest();
  ~FirebaseMessagingTest() override;

  static void SetUpTestSuite();
  static void TearDownTestSuite();

  void SetUp() override;
  void TearDown() override;

  // Create a request and heads for a test message (returning false if unable to
  // do so). send_to can be a FCM token or a topic subscription.
  bool CreateTestMessage(
      const char* send_to, const char* notification_title,
      const char* notification_body,
      const std::map<std::string, std::string>& message_fields,
      std::string* request_out,
      std::map<std::string, std::string>* headers_out);
  // Send a message to a token or topic by using Firebase Functions to trigger
  // it.
  void SendTestMessageToToken(
      const char* send_to, const char* notification_title,
      const char* notification_body,
      const std::map<std::string, std::string>& message_fields);
  void SendTestMessageToTopic(
      const char* send_to, const char* notification_title,
      const char* notification_body,
      const std::map<std::string, std::string>& message_fields);
  // Called by the above.
  void SendTestMessage(
      const char* send_to, bool is_token, const char* notification_title,
      const char* notification_body,
      const std::map<std::string, std::string>& message_fields);

  // Get a unique message ID so we can confirm the correct message is being
  // received.
  std::string GetUniqueMessageId();

  // Wait to receive a token. Returns true if a token was received, and places
  // it in token_.
  static bool WaitForToken(int timeout = kTimeoutSeconds);
  // Request messaging permissions from the user. Returns true if permission was
  // granted.
  bool RequestPermission();
  // Wait to receive a message. Returns true if a message was received and
  // returns it in the message_out parameter.
  bool WaitForMessage(firebase::messaging::Message* message_out,
                      int timeout = kTimeoutSeconds);

  const std::string* shared_token() { return shared_token_; }

 protected:
  static firebase::App* shared_app_;
  static firebase::messaging::PollableListener* shared_listener_;
  static std::string* shared_token_;
  static bool is_desktop_stub_;

  static firebase::functions::Functions* functions_;
};

const char kObtainedPermissionKey[] = "messaging_got_permission";

std::string* FirebaseMessagingTest::shared_token_ = nullptr;
firebase::App* FirebaseMessagingTest::shared_app_ = nullptr;
firebase::messaging::PollableListener* FirebaseMessagingTest::shared_listener_ =
    nullptr;
bool FirebaseMessagingTest::is_desktop_stub_;
firebase::functions::Functions* FirebaseMessagingTest::functions_ = nullptr;

void FirebaseMessagingTest::SetUpTestSuite() {
  LogDebug("Initialize Firebase App.");

#if defined(__ANDROID__)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)

  LogDebug("Initializing Firebase Cloud Messaging.");
  shared_token_ = new std::string();

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      shared_app_, &shared_listener_, [](::firebase::App* app, void* userdata) {
        LogDebug("Try to initialize Firebase Messaging");
        firebase::messaging::PollableListener** listener =
            reinterpret_cast<firebase::messaging::PollableListener**>(userdata);
        *listener = new firebase::messaging::PollableListener();
        firebase::messaging::MessagingOptions options;
        // Prevent the app from requesting permission to show notifications
        // immediately upon starting up for the first time. Since it the prompt
        // is being suppressed, we must manually display it with a call to
        // RequestPermission() elsewhere.
        //
        // After the first time we've done this, we no longer will suppress the
        // permission prompt just for ease of initialization.
        std::string value;
        if (!GetPersistentString(kObtainedPermissionKey, &value) ||
            value.empty()) {
          // The first time, suppress the notification prompt so that
          // RequestPermission will be called.
          options.suppress_notification_permission_prompt = true;
        }

        return ::firebase::messaging::Initialize(*app, *listener, options);
      });

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Cloud Messaging.");
  functions_ = firebase::functions::Functions::GetInstance(shared_app_);
  is_desktop_stub_ = false;
#if !defined(ANDROID) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  is_desktop_stub_ = true;
#endif  // !defined(ANDROID) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
}

void FirebaseMessagingTest::TearDownTestSuite() {
  LogDebug("All tests finished, cleaning up.");
  firebase::messaging::SetListener(nullptr);
  delete shared_listener_;
  shared_listener_ = nullptr;
  delete shared_token_;
  shared_token_ = nullptr;

  LogDebug("Shutdown Firebase Cloud Messaging.");
  firebase::messaging::Terminate();
  if (functions_) {
    LogDebug("Shutdown the Functions library.");
    delete functions_;
    functions_ = nullptr;
  }
  LogDebug("Shutdown Firebase App.");
  delete shared_app_;
  shared_app_ = nullptr;

  // On iOS/FTL, most or all of the tests are skipped, so add a delay so the app
  // doesn't finish too quickly, as this makes test results flaky.
  ProcessEvents(1000);
}

FirebaseMessagingTest::FirebaseMessagingTest() {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseMessagingTest::~FirebaseMessagingTest() {}

void FirebaseMessagingTest::SetUp() { FirebaseTest::SetUp(); }

void FirebaseMessagingTest::TearDown() { FirebaseTest::TearDown(); }

std::string FirebaseMessagingTest::GetUniqueMessageId() {
  int64_t time_in_microseconds = GetCurrentTimeInMicroseconds();
  char buffer[21] = {0};
  snprintf(buffer, sizeof(buffer), "%lld",
           static_cast<long long>(time_in_microseconds));  // NOLINT
  return std::string(buffer);
}

void FirebaseMessagingTest::SendTestMessageToToken(
    const char* send_to, const char* notification_title,
    const char* notification_body,
    const std::map<std::string, std::string>& message_fields) {
  SendTestMessage(send_to, true, notification_title, notification_body,
                  message_fields);
}

void FirebaseMessagingTest::SendTestMessageToTopic(
    const char* send_to, const char* notification_title,
    const char* notification_body,
    const std::map<std::string, std::string>& message_fields) {
  SendTestMessage(send_to, false, notification_title, notification_body,
                  message_fields);
}

void FirebaseMessagingTest::SendTestMessage(
    const char* send_to, bool is_token, const char* notification_title,
    const char* notification_body,
    const std::map<std::string, std::string>& message_fields) {
  firebase::Variant data(firebase::Variant::EmptyMap());
  data.map()["sendTo"] = firebase::Variant(send_to);
  data.map()["isToken"] = firebase::Variant(is_token);
  data.map()["notificationTitle"] = firebase::Variant(notification_title);
  data.map()["notificationBody"] = firebase::Variant(notification_body);
  data.map()["messageFields"] = firebase::Variant(message_fields);

  firebase::functions::HttpsCallableReference ref =
      functions_->GetHttpsCallable("sendMessage");

  LogDebug("Triggering FCM message via Firebase Functions...");
  firebase::Future<firebase::functions::HttpsCallableResult> future =
      ref.Call(data);

  EXPECT_TRUE(WaitForCompletion(future, "SendTestMessage"));
}

bool FirebaseMessagingTest::WaitForToken(int timeout) {
  if (!shared_token_->empty()) return true;

  if (is_desktop_stub_) {
    // On desktop, just set a stub token.
    *shared_token_ = "FcmDesktopStubToken";
    return true;
  }

  std::string new_token;
  // No new or old token immediately, so wait for a new token.
  int seconds = 0;
  while (seconds <= timeout) {
    if (shared_listener_->PollRegistrationToken(&new_token)) {
      if (!new_token.empty()) {
        *shared_token_ = new_token;
        LogInfo("Got token: %s", shared_token_->c_str());
        return true;
      }
    }
    seconds++;
    ProcessEvents(1000);
  }
  // Failed to get a token.
  *shared_token_ = "";
  return false;
}

bool FirebaseMessagingTest::WaitForMessage(
    firebase::messaging::Message* message_out, int timeout) {
  int seconds = 0;
  while (seconds <= timeout) {
    if (shared_listener_->PollMessage(message_out)) {
      LogDebug("Received a message.");
      return true;
    }
    seconds++;
    ProcessEvents(1000);
  }
  LogDebug("Did not receive a message.");
  *message_out = firebase::messaging::Message();
  return false;
}

bool FirebaseMessagingTest::RequestPermission() {
  std::string value;
  if (GetPersistentString(kObtainedPermissionKey, &value) && value == "1") {
    return true;  // Already got permission.
  }

  bool b = WaitForCompletion(firebase::messaging::RequestPermission(),
                             "RequestPermission");
  if (b) {
#if TARGET_OS_IPHONE
    // We only need to pause for permission on iOS.
    LogDebug("Pausing so user can grant permission (if needed).");
    ProcessEvents(10000);
#endif  // TARGET_OS_IPHONE
    SetPersistentString(kObtainedPermissionKey, "1");
  }
  return b;
}

// Test cases below.

TEST_F(FirebaseMessagingTest, TestRequestPermission) {
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;

  // This test may request a permission from the user; if so, the user must
  // respond affirmatively.
  EXPECT_TRUE(RequestPermission());
}

TEST_F(FirebaseMessagingTest, TestReceiveToken) {
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;

  SKIP_TEST_ON_ANDROID_EMULATOR;

  EXPECT_TRUE(RequestPermission());

  EXPECT_TRUE(::firebase::messaging::IsTokenRegistrationOnInitEnabled());

  FLAKY_TEST_SECTION_BEGIN();

  EXPECT_TRUE(WaitForToken());
  EXPECT_NE(*shared_token_, "");

  FLAKY_TEST_SECTION_END();

  // Add in a small delay to make sure that the backend is ready to send
  // targeted messages to the new device.
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

TEST_F(FirebaseMessagingTest, TestSubscribeAndUnsubscribe) {
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;

  // TODO(b/196589796) Test fails on Android emulators and causes failures in
  // our CI. Since we don't have a good way to deterine if the runtime is an
  // emulator or real device, we should disable the test in CI until we find
  // the cause of problem.
  TEST_REQUIRES_USER_INTERACTION_ON_ANDROID;

  EXPECT_TRUE(RequestPermission());
  EXPECT_TRUE(WaitForToken());
  EXPECT_TRUE(WaitForCompletion(firebase::messaging::Subscribe("SubscribeTest"),
                                "Subscribe"));
  EXPECT_TRUE(WaitForCompletion(
      firebase::messaging::Unsubscribe("SubscribeTest"), "Unsubscribe"));
}

TEST_F(FirebaseMessagingTest, TestSendMessageToToken) {
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;
  SKIP_TEST_ON_DESKTOP;

  SKIP_TEST_ON_ANDROID_EMULATOR;

  EXPECT_TRUE(RequestPermission());
  EXPECT_TRUE(WaitForToken());

  FLAKY_TEST_SECTION_BEGIN();

  std::string unique_id = GetUniqueMessageId();
  const char kNotificationTitle[] = "Token Test";
  const char kNotificationBody[] = "Token Test notification body";
  SendTestMessageToToken(shared_token()->c_str(), kNotificationTitle,
                         kNotificationBody,
                         {{"message", "Hello, world!"},
                          {"unique_id", unique_id},
                          {kNotificationLinkKey, kTestLink}});
  LogDebug("Waiting for message.");
  firebase::messaging::Message message;
  EXPECT_TRUE(WaitForMessage(&message));
  EXPECT_EQ(message.data["unique_id"], unique_id);
  EXPECT_NE(message.notification, nullptr);
  if (message.notification) {
    EXPECT_EQ(message.notification->title, kNotificationTitle);
    EXPECT_EQ(message.notification->body, kNotificationBody);
  }
  EXPECT_EQ(message.link, kTestLink);

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseMessagingTest, TestSendMessageToTopic) {
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;
  SKIP_TEST_ON_DESKTOP;

  SKIP_TEST_ON_ANDROID_EMULATOR;

  EXPECT_TRUE(RequestPermission());
  EXPECT_TRUE(WaitForToken());

  FLAKY_TEST_SECTION_BEGIN();

  std::string unique_id = GetUniqueMessageId();
  const char kNotificationTitle[] = "Topic Test";
  const char kNotificationBody[] = "Topic Test notification body";
  // Create a somewhat unique topic name using 2 digits near the end
  // of unique_id (but not the LAST 2 digits, due to timestamp
  // resolution on some platforms).
  std::string unique_id_tag =
      (unique_id.length() >= 7 ? unique_id.substr(unique_id.length() - 5, 2)
                               : "00");
  std::string topic = "FCMTestTopic" + unique_id_tag;
  firebase::Future<void> sub = firebase::messaging::Subscribe(topic.c_str());
  WaitForCompletion(sub, ("Subscribe " + topic).c_str());
  SendTestMessageToTopic(
      ("/topics/" + topic).c_str(), kNotificationTitle, kNotificationBody,
      {{"message", "Hello, world!"}, {"unique_id", unique_id}});
  firebase::messaging::Message message;
  EXPECT_TRUE(WaitForMessage(&message));

  EXPECT_EQ(message.data["unique_id"], unique_id);
  if (message.notification) {
    EXPECT_EQ(message.notification->title, kNotificationTitle);
    EXPECT_EQ(message.notification->body, kNotificationBody);
  }
  firebase::Future<void> unsub =
      firebase::messaging::Unsubscribe(topic.c_str());
  WaitForCompletion(unsub, "Unsubscribe");

  // Ensure that we *don't* receive a message now.
  unique_id = GetUniqueMessageId();
  SendTestMessageToTopic(
      ("/topics/" + topic).c_str(), "Topic Title 2", "Topic Body 2",
      {{"message", "Hello, world!"}, {"unique_id", unique_id}});

  // If this returns true, it means we received a message but
  // shouldn't have.
  EXPECT_FALSE(WaitForMessage(&message, 5));

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseMessagingTest, TestChangingListener) {
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;
  SKIP_TEST_ON_DESKTOP;

  SKIP_TEST_ON_ANDROID_EMULATOR;

  EXPECT_TRUE(RequestPermission());
  EXPECT_TRUE(WaitForToken());

  FLAKY_TEST_SECTION_BEGIN();

  // Back up the previous listener object and create a new one.
  firebase::messaging::PollableListener* old_listener_ = shared_listener_;
  // WaitForMessage() uses whatever shared_listener_ is set to.
  shared_listener_ = new firebase::messaging::PollableListener();
  firebase::messaging::SetListener(shared_listener_);
  // Pause a moment to make sure old listeners are deleted.
  ProcessEvents(1000);

  std::string unique_id = GetUniqueMessageId();
  const char kNotificationTitle[] = "New Listener Test";
  const char kNotificationBody[] = "New Listener Test notification body";
  SendTestMessageToToken(
      shared_token_->c_str(), kNotificationTitle, kNotificationBody,
      {{"message", "Hello, world!"}, {"unique_id", unique_id}});
  LogDebug("Waiting for message.");
  firebase::messaging::Message message;
  EXPECT_TRUE(WaitForMessage(&message));
  EXPECT_EQ(message.data["unique_id"], unique_id);
  if (message.notification) {
    EXPECT_EQ(message.notification->title, kNotificationTitle);
    EXPECT_EQ(message.notification->body, kNotificationBody);
  }

  // Set back to the previous listener.
  firebase::messaging::SetListener(old_listener_);
  delete shared_listener_;
  shared_listener_ = old_listener_;

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseMessagingTest, DeliverMetricsToBigQuery) {
  // These setters and getters are not implemented on all platforms, so we run
  // them here to make sure they don't crash, and then validate the values
  // received below only on the platforms they are implemented on.

  bool initial_value =
      firebase::messaging::DeliveryMetricsExportToBigQueryEnabled();
  // This one should always default to false unless it has been set.
  EXPECT_FALSE(initial_value);

  firebase::messaging::SetDeliveryMetricsExportToBigQuery(true);
  bool result_after_setting =
      firebase::messaging::DeliveryMetricsExportToBigQueryEnabled();

  firebase::messaging::SetDeliveryMetricsExportToBigQuery(false);
  bool result_after_clearing =
      firebase::messaging::DeliveryMetricsExportToBigQueryEnabled();

#if defined(ANDROID)
  EXPECT_TRUE(result_after_setting);
  EXPECT_FALSE(result_after_clearing);
#else
  (void)result_after_setting;
  (void)result_after_clearing;
#endif
}

}  // namespace firebase_testapp_automated
