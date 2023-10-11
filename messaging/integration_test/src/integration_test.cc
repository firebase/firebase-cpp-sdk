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

// Your Firebase project's Server Key for Cloud Messaging goes here.
// You can get this from Firebase Console, in your Project settings under Cloud
// Messaging.
const char kFcmServerKey[] = "REPLACE_WITH_YOUR_SERVER_KEY";

const char kRestEndpoint[] = "https://fcm.googleapis.com/fcm/send";

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
  // Send a message previously created by CreateTestMessage.
  void SendTestMessage(const std::string& request,
                       const std::map<std::string, std::string>& headers);
  // Convenience method combining the above.
  void SendTestMessage(
      const char* send_to, const char* notification_title,
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
};

const char kObtainedPermissionKey[] = "messaging_got_permission";

std::string* FirebaseMessagingTest::shared_token_ = nullptr;
firebase::App* FirebaseMessagingTest::shared_app_ = nullptr;
firebase::messaging::PollableListener* FirebaseMessagingTest::shared_listener_ =
    nullptr;
bool FirebaseMessagingTest::is_desktop_stub_;

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

// send_to can be a FCM token or a topic subscription.
bool FirebaseMessagingTest::CreateTestMessage(
    const char* send_to, const char* notification_title,
    const char* notification_body,
    const std::map<std::string, std::string>& message_fields,
    std::string* request_out, std::map<std::string, std::string>* headers_out) {
  if (is_desktop_stub_) {
    // Don't send HTTP requests in stub mode.
    return false;
  }
  if (strcasecmp(kFcmServerKey, "replace_with_your_server_key") == 0) {
    LogWarning(
        "Please put your Firebase Cloud Messaging server key in "
        "kFcmServerKey.");
    LogWarning("Without a server key, most of these tests will fail.");
  }
  std::map<std::string, std::string> headers;
  headers.insert(std::make_pair("Content-type", "application/json"));
  headers.insert(
      std::make_pair("Authorization", std::string("key=") + kFcmServerKey));
  std::string request;  // Build a JSON request.
  request += "{\"notification\":{\"title\":\"";
  request += notification_title ? notification_title : "";
  request += "\",\"body\":\"";
  request += notification_body ? notification_body : "";
  request += "\"},\"data\":{";
  for (auto i = message_fields.begin(); i != message_fields.end(); ++i) {
    if (i != message_fields.begin()) request += ",";
    request += "\"";
    request += i->first;
    request += "\":\"";
    request += i->second;
    request += "\"";
  }
  request += "}, \"to\":\"";
  request += send_to;
  // Messages will expire after 5 minutes, so if there are stale/leftover
  // messages from a previous run, just wait 5 minutes and try again.
  request += "\", \"time_to_live\":300}";
  *request_out = request;
  *headers_out = headers;
  return true;
}

void FirebaseMessagingTest::SendTestMessage(
    const char* send_to, const char* notification_title,
    const char* notification_body,
    const std::map<std::string, std::string>& message_fields) {
  std::string request;
  std::map<std::string, std::string> headers;
  EXPECT_TRUE(CreateTestMessage(send_to, notification_title, notification_body,
                                message_fields, &request, &headers));
  SendTestMessage(request, headers);
}

void FirebaseMessagingTest::SendTestMessage(
    const std::string& request,
    const std::map<std::string, std::string>& headers) {
  LogDebug("Request: %s", request.c_str());
  LogDebug("Triggering FCM message from server...");
  EXPECT_TRUE(
      SendHttpPostRequest(kRestEndpoint, headers, request, nullptr, nullptr));
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

static std::string ConstructHtmlToSendMessage(
    const std::string& request,
    const std::map<std::string, std::string>& headers, int delay_seconds) {
  // Generate some simple HTML/Javascript to pause a few seconds, then send the
  // POST request via XMLHttpRequest.
  std::string h;
  h += "<script>window.onload = function(e){"
       "document.write('<h1>FCM Integration Test</h1>');"
       "document.write('<h2>Pausing a moment...</h2>');"
       "setTimeout(function(e2){"
       "document.write('<h2>Sending message request...</h2>');"
       "let xhttp = new XMLHttpRequest();"
       "xhttp.open('POST','";
  h += kRestEndpoint;
  h += "',false);";
  for (auto i = headers.begin(); i != headers.end(); ++i) {
    h += "xhttp.setRequestHeader('" + i->first + "','" + i->second + "');";
  }
  h += "xhttp.send('" + request + "', false);";
  h += "if(xhttp.status==200){"
#if defined(__ANDROID__)
       "document.write('<h2>Notification sent.<br>Open system tray and tap "
       "notification to return to tests.</h2>');"
#else
       "document.write('<h2>Notification sent.<br>Tap notification to return "
       "to tests.</h2>');"
#endif  // defined(__ANDROID__)
       "}else{"
       "document.write('<h1>Failed to send notification.</h1>');"
       "document.write('Status '+xhttp.status+': '+xhttp.response);"
       "}},";
  char delay_seconds_string[22];
  snprintf(delay_seconds_string, 22, "%d", delay_seconds);
  h += delay_seconds_string;
  h += ");}</script>";
  return h;
}

TEST_F(FirebaseMessagingTest, TestNotification) {
  TEST_REQUIRES_USER_INTERACTION;
  SKIP_TEST_ON_DESKTOP;

  EXPECT_TRUE(RequestPermission());
  EXPECT_TRUE(WaitForToken());

  // To test notifications, this test app must be running in the background. To
  // accomplish this, switch over to the device's web browser, loading an HTML
  // page that will, after a short delay, send the FCM message request to the
  // app in the background. This will produce the system notification that you
  // can then click on to go back into the app and continue the test.

  std::string unique_id = GetUniqueMessageId();
  std::string token = *shared_token_;
  const char kNotificationTitle[] = "FCM Integration Test";
  const char kNotificationBody[] = "Test notification, open to resume testing.";
  std::string value;
  if (!GetPersistentString(kTestingNotificationKey, &value) || value.empty()) {
    // If the notification test is already in progress, just go straight to the
    // waiting part. This can happen if you wait too long to click on the
    // notification and the app is no longer running in the background.
    std::string request;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> message_fields = {
      {"message", "This is a notification."},
      {"unique_id", unique_id},
#if defined(__ANDROID__)
      // Duplicate notification.title and notification.body here, see
      // below for why.
      {"notification_title", kNotificationTitle},
      {"notification_body", kNotificationBody},
#endif  // defined(__ANDROID__)
    };
    EXPECT_TRUE(CreateTestMessage(shared_token_->c_str(), kNotificationTitle,
                                  kNotificationBody, message_fields, &request,
                                  &headers));
    std::string html = ConstructHtmlToSendMessage(request, headers, 5);
    // We now have some HTML/Javascript to send the message request. Embed it in
    // a data: url so we can try receiving a message with the app in the
    // background.
    // Encode the HTML into base64.
    std::string html_encoded;
    EXPECT_TRUE(Base64Encode(html, &html_encoded));
    std::string url = std::string("data:text/html;base64,") + html_encoded;
    LogInfo("Opening browser to trigger FCM message.");
    if (OpenUrlInBrowser(url.c_str())) {
      SetPersistentString(kTestingNotificationKey, "1");
    } else {
      FAIL() << "Failed to open URL in browser.";
    }
  }
  SetPersistentString(kTestingNotificationKey, nullptr);
  LogDebug("Waiting for message.");
  firebase::messaging::Message message;
  EXPECT_TRUE(WaitForMessage(&message, 120));
  EXPECT_EQ(message.data["unique_id"], unique_id);
  EXPECT_TRUE(message.notification_opened);

#if defined(__ANDROID__)
  // On Android, if the app is running in the background, FCM does not deliver
  // both the "notification" and the "data". So for our purposes, duplicate the
  // notification fields we are checking into the data fields so we can still
  // check that it's correct.
  EXPECT_EQ(message.notification, nullptr);
  EXPECT_EQ(message.data["notification_title"], kNotificationTitle);
  EXPECT_EQ(message.data["notification_body"], kNotificationBody);
#else
  // On iOS, we do get the notification.
  EXPECT_NE(message.notification, nullptr);
  if (message.notification) {
    EXPECT_EQ(message.notification->title, kNotificationTitle);
    EXPECT_EQ(message.notification->body, kNotificationBody);
  }
#endif  // defined(__ANDROID__)
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
  SendTestMessage(shared_token()->c_str(), kNotificationTitle,
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
  WaitForCompletion(sub, "Subscribe");
  SendTestMessage(("/topics/" + topic).c_str(), kNotificationTitle,
                  kNotificationBody,
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
  SendTestMessage(("/topics/" + topic).c_str(), "Topic Title 2", "Topic Body 2",
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
  SendTestMessage(shared_token_->c_str(), kNotificationTitle, kNotificationBody,
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
