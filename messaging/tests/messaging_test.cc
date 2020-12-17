// Copyright 2017 Google LLC
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

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "messaging/src/include/firebase/messaging.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif  // defined(__APPLE_)

#include "app/src/util.h"
#include "messaging/tests/messaging_test_util.h"
#include "testing/config.h"
#include "testing/reporter.h"
#include "testing/ticker.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::StrEq;

namespace firebase {
namespace messaging {

class MessagingTestListener : public Listener {
 public:
  void OnMessage(const Message& message) override;
  void OnTokenReceived(const char* token) override;

  const Message& GetMessage() const { return message_; }

  const std::string& GetToken() const { return token_; }

  int GetOnTokenReceivedCount() const { return on_token_received_count_; }

  int GetOnMessageReceivedCount() const { return on_message_received_count_; }

 private:
  Message message_;
  std::string token_;
  int on_token_received_count_ = 0;
  int on_message_received_count_ = 0;
};

class MessagingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Cache the local storage file and lockfile.
    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
    reporter_.reset();

    firebase_app_ = testing::CreateApp();
    InitializeMessagingTest();
    EXPECT_EQ(Initialize(*firebase_app_, &listener_), kInitResultSuccess);
  }

  void TearDown() override {
    firebase::testing::cppsdk::ConfigReset();
    Terminate();
    TerminateMessagingTest();
    delete firebase_app_;
    firebase_app_ = nullptr;
    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  void AddExpectationAndroid(const char* fake,
                             std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kAndroid,
                             args);
  }

  void AddExpectationApple(const char* fake,
                           std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kIos, args);
  }

  App* firebase_app_ = nullptr;
  MessagingTestListener listener_;
  firebase::testing::cppsdk::Reporter reporter_;
};

void MessagingTestListener::OnMessage(const Message& message) {
  message_ = message;
  on_message_received_count_++;
}

void MessagingTestListener::OnTokenReceived(const char* token) {
  token_ = token;
  on_token_received_count_++;
}

// Tests only run on Android for now.
TEST_F(MessagingTest, TestInitializeTwice) {
  MessagingTestListener listener;
  EXPECT_EQ(Initialize(*firebase_app_, &listener), kInitResultSuccess);
}

// The order of these matter because of the global flag
// g_registration_token_received
TEST_F(MessagingTest, TestSubscribeNoRegistration) {
  Subscribe("topic");
  SleepMessagingTest(1);
  // Android should cache the call, iOS will subscribe right away.
  AddExpectationApple("-[FIRMessaging subscribeToTopic:completion:]",
                      {"topic"});
}

// TODO(westarle): break up this test when subscriber queuing is testable.
TEST_F(MessagingTest, TestSubscribeBeforeRegistration) {
  Subscribe("$invalid");
  Subscribe("subscribe_topic1");
  Subscribe("subscribe_topic2");
  Unsubscribe("$invalid");
  Unsubscribe("unsubscribe_topic1");
  Unsubscribe("unsubscribe_topic2");
  AddExpectationApple("-[FIRMessaging subscribeToTopic:completion:]",
                      {"$invalid", "subscribe_topic1", "subscribe_topic2"});
  AddExpectationApple("-[FIRMessaging unsubscribeFromTopic:completion:]",
                      {"$invalid", "unsubscribe_topic1", "unsubscribe_topic2"});

  // No requests to Java API yet, iOS should go ahead and forward.
  EXPECT_THAT(reporter_.getFakeReports(),
              ::testing::Eq(reporter_.getExpectations()));

  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  AddExpectationAndroid("FirebaseMessaging.subscribeToTopic",
                        {"$invalid", "subscribe_topic1", "subscribe_topic2"});

  AddExpectationAndroid(
      "FirebaseMessaging.unsubscribeFromTopic",
      {"$invalid", "unsubscribe_topic1", "unsubscribe_topic2"});
}

TEST_F(MessagingTest, TestSubscribeAfterRegistration) {
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  Subscribe("topic");

  AddExpectationAndroid("FirebaseMessaging.subscribeToTopic", {"topic"});
  AddExpectationApple("-[FIRMessaging subscribeToTopic:completion:]",
                      {"topic"});
}

TEST_F(MessagingTest, TestUnsubscribeAfterRegistration) {
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  Unsubscribe("topic");
  AddExpectationAndroid("FirebaseMessaging.unsubscribeFromTopic", {"topic"});
  AddExpectationApple("-[FIRMessaging unsubscribeFromTopic:completion:]",
                      {"topic"});
}

TEST_F(MessagingTest, TestTokenReceived) {
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  EXPECT_THAT(listener_.GetToken(), StrEq("my_token"));
}

TEST_F(MessagingTest, TestTokenReceivedBeforeInitialize) {
  Terminate();
  OnTokenReceived("my_token");
  EXPECT_EQ(Initialize(*firebase_app_, &listener_), kInitResultSuccess);
  SleepMessagingTest(1);
  EXPECT_THAT(listener_.GetToken(), StrEq("my_token"));
}

TEST_F(MessagingTest, TestTwoTokensReceivedBeforeInitialize) {
  Terminate();
  OnTokenReceived("my_token1");
  OnTokenReceived("my_token2");
  EXPECT_EQ(Initialize(*firebase_app_, &listener_), kInitResultSuccess);
  SleepMessagingTest(1);
  EXPECT_THAT(listener_.GetToken(), StrEq("my_token2"));
}

TEST_F(MessagingTest, TestTwoTokensReceivedAfterInitialize) {
  OnTokenReceived("my_token1");
  OnTokenReceived("my_token2");
  SleepMessagingTest(1);
  EXPECT_THAT(listener_.GetToken(), StrEq("my_token2"));
  EXPECT_EQ(listener_.GetOnTokenReceivedCount(), 2);
}

TEST_F(MessagingTest, TestTwoIdenticalTokensReceived) {
  OnTokenReceived("my_token");
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  EXPECT_THAT(listener_.GetToken(), StrEq("my_token"));
  EXPECT_EQ(listener_.GetOnTokenReceivedCount(), 1);
}

TEST_F(MessagingTest, TestTokenReceivedNoListener) {
  Terminate();
  EXPECT_EQ(Initialize(*firebase_app_, nullptr), kInitResultSuccess);
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  SetListener(&listener_);
  SleepMessagingTest(1);
  EXPECT_THAT(listener_.GetToken(), StrEq("my_token"));
  EXPECT_EQ(listener_.GetOnTokenReceivedCount(), 1);
}

TEST_F(MessagingTest, TestSubscribeInvalidTopic) {
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  Subscribe("$invalid");
  AddExpectationAndroid("FirebaseMessaging.subscribeToTopic", {"$invalid"});
  AddExpectationApple("-[FIRMessaging subscribeToTopic:completion:]",
                      {"$invalid"});
}

TEST_F(MessagingTest, TestUnsubscribeInvalidTopic) {
  OnTokenReceived("my_token");
  SleepMessagingTest(1);
  Unsubscribe("$invalid");
  AddExpectationAndroid("FirebaseMessaging.unsubscribeFromTopic", {"$invalid"});
  AddExpectationApple("-[FIRMessaging unsubscribeFromTopic:completion:]",
                      {"$invalid"});
}

TEST_F(MessagingTest, TestDataMessageReceived) {
  Message message;
  message.from = "my_from";
  message.data["my_key"] = "my_value";
  OnMessageReceived(message);
  SleepMessagingTest(1);
  EXPECT_EQ(listener_.GetOnMessageReceivedCount(), 1);
  EXPECT_THAT(listener_.GetMessage().from, StrEq("my_from"));
  EXPECT_THAT(listener_.GetMessage().message_id, StrEq(""));
  EXPECT_THAT(listener_.GetMessage().message_type, StrEq(""));
  EXPECT_THAT(listener_.GetMessage().error, StrEq(""));
  EXPECT_THAT(listener_.GetMessage().data.at("my_key"), StrEq("my_value"));
}

TEST_F(MessagingTest, TestNotificationReceived) {
  Message send_message;
  send_message.from = "my_from";
  send_message.to = "my_to";
  send_message.message_id = "id";
  send_message.message_type = "type";
  send_message.error = "";
  send_message.data["my_key"] = "my_value";
  send_message.notification = new Notification;
  send_message.notification->title = "my_title";
  send_message.notification->body = "my_body";
  send_message.notification->icon = "my_icon";
  send_message.notification->sound = "my_sound";
  send_message.notification->tag = "my_tag";
  send_message.notification->color = "my_color";
  send_message.notification->click_action = "my_click_action";
  send_message.notification->body_loc_key = "my_body_localization_key";
  send_message.notification->body_loc_args.push_back(
      "my_body_localization_item");
  send_message.notification->title_loc_key = "my_title_localization_key";
  send_message.notification->title_loc_args.push_back(
      "my_title_localization_item");
  send_message.notification_opened = true;
  send_message.notification->android = new AndroidNotificationParams;
  send_message.notification->android->channel_id = "my_android_channel_id";
  send_message.collapse_key = "my_collapse_key";
  send_message.priority = "my_priority";
  send_message.original_priority = "normal";
  send_message.time_to_live = 1234;
  send_message.sent_time = 5678;
  OnMessageReceived(send_message);
  SleepMessagingTest(1);
  const Message& message = listener_.GetMessage();
  EXPECT_EQ(listener_.GetOnMessageReceivedCount(), 1);
  EXPECT_THAT(message.from, StrEq("my_from"));
  EXPECT_THAT(message.to, StrEq("my_to"));
  EXPECT_THAT(message.message_id, StrEq("id"));
  EXPECT_THAT(message.message_type, StrEq("type"));
  EXPECT_THAT(message.error, StrEq(""));
  EXPECT_THAT(message.data.at("my_key"), StrEq("my_value"));
  EXPECT_TRUE(message.notification_opened);
  EXPECT_THAT(message.notification->title, StrEq("my_title"));
  EXPECT_THAT(message.notification->body, StrEq("my_body"));
  EXPECT_THAT(message.notification->sound, StrEq("my_sound"));
  EXPECT_THAT(message.collapse_key, StrEq("my_collapse_key"));
  EXPECT_THAT(message.priority, StrEq("my_priority"));
  EXPECT_EQ(message.time_to_live, 1234);
#if !TARGET_OS_IPHONE
  EXPECT_THAT(message.original_priority, StrEq("normal"));
  EXPECT_EQ(message.sent_time, 5678);
#endif  // !TARGET_OS_IPHONE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_THAT(message.notification->icon, StrEq("my_icon"));
  EXPECT_THAT(message.notification->tag, StrEq("my_tag"));
  EXPECT_THAT(message.notification->color, StrEq("my_color"));
  EXPECT_THAT(message.notification->click_action, StrEq("my_click_action"));
  EXPECT_THAT(
      message.notification->body_loc_key, StrEq("my_body_localization_key"));
  EXPECT_THAT(message.notification->body_loc_args[0],
            StrEq("my_body_localization_item"));
  EXPECT_THAT(
      message.notification->title_loc_key, StrEq("my_title_localization_key"));
  EXPECT_THAT(message.notification->title_loc_args[0],
            StrEq("my_title_localization_item"));
  EXPECT_THAT(message.notification->android->channel_id,
              StrEq("my_android_channel_id"));
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)

TEST_F(MessagingTest, TestOnDeletedMessages) {
  OnDeletedMessages();
  SleepMessagingTest(1);
  EXPECT_EQ(listener_.GetOnMessageReceivedCount(), 1);
  EXPECT_THAT(listener_.GetMessage().from, StrEq(""));
  EXPECT_THAT(listener_.GetMessage().message_id, StrEq(""));
  EXPECT_THAT(listener_.GetMessage().message_type, StrEq("deleted_messages"));
  EXPECT_THAT(listener_.GetMessage().error, StrEq(""));
}

TEST_F(MessagingTest, TestOnMessageSent) {
  OnMessageSent("my_message_id");
  SleepMessagingTest(1);
  EXPECT_EQ(listener_.GetOnMessageReceivedCount(), 1);
  EXPECT_THAT(listener_.GetMessage().message_id, StrEq("my_message_id"));
  EXPECT_THAT(listener_.GetMessage().message_type, StrEq("send_event"));
}

TEST_F(MessagingTest, TestOnSendError) {
  OnMessageSentError("my_message_id", "my_exception");
  SleepMessagingTest(1);
  EXPECT_EQ(listener_.GetOnMessageReceivedCount(), 1);
  EXPECT_THAT(listener_.GetMessage().message_id, StrEq("my_message_id"));
  EXPECT_THAT(listener_.GetMessage().message_type, StrEq("send_error"));
  EXPECT_THAT(listener_.GetMessage().error, StrEq("my_exception"));
}

TEST_F(MessagingTest, TestGetToken) {
  Future<std::string> result = GetToken();
  SleepMessagingTest(1);
  EXPECT_THAT(*result.result(), StrEq("StubToken"));
  AddExpectationAndroid("FirebaseMessaging.getToken", {});
}

TEST_F(MessagingTest, TestDeleteToken) {
  Future<void> result = DeleteToken();
  SleepMessagingTest(1);
  AddExpectationAndroid("FirebaseMessaging.deleteToken", {});
}

#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

}  // namespace messaging
}  // namespace firebase
