/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/src/assert.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Ne;

const char kTestMessage[] = "TEST_MESSAGE";

struct CallbackData {
  LogLevel log_level;
  std::string message;
};

void TestLogCallback(LogLevel log_level, const char* message,
                     void* callback_data) {
  if (callback_data) {
    auto* data = static_cast<CallbackData*>(callback_data);
    data->log_level = log_level;
    data->message = message;
  }
}

class AssertTest : public ::testing::Test {
 public:
  ~AssertTest() override {
    LogSetCallback(nullptr, nullptr);
  }
};

// Tests that check the functionality of FIREBASE_ASSERT_* macros in both debug
// and release builds.

TEST_F(AssertTest, FirebaseAssertWithExpressionAborts) {
  EXPECT_DEATH(FIREBASE_ASSERT_WITH_EXPRESSION(false, FailureExpression), "");
}

TEST_F(AssertTest, FirebaseAssertAborts) {
  EXPECT_DEATH(FIREBASE_ASSERT(false), "");
}

int FirebaseAssertReturnInt(int return_value) {
  FIREBASE_ASSERT_RETURN(return_value, false);
  return 0;
}

TEST_F(AssertTest, FirebaseAssertReturnAborts) {
  EXPECT_DEATH(FirebaseAssertReturnInt(1), "");
}

TEST_F(AssertTest, FirebaseAssertReturnReturnsInt) {
  auto* callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int return_value = 1;
  EXPECT_THAT(FirebaseAssertReturnInt(return_value), Eq(return_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr("false"));
  delete callback_data;
}

TEST_F(AssertTest, FirebaseAssertReturnVoidAborts) {
  EXPECT_DEATH(FIREBASE_ASSERT_RETURN_VOID(false), "");
}

void FirebaseAssertReturnVoid(int in_value, int* out_value) {
  FIREBASE_ASSERT_RETURN_VOID(false);
  *out_value = in_value;
}

TEST_F(AssertTest, FirebaseAssertReturnVoidReturnsVoid) {
  auto* callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int in_value = 1;
  int out_value = 0;
  FirebaseAssertReturnVoid(in_value, &out_value);
  EXPECT_THAT(out_value, Ne(in_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr("false"));
  delete callback_data;
}

TEST_F(AssertTest, FirebaseAssertMessageWithExpressionAborts) {
  EXPECT_DEATH(FIREBASE_ASSERT_MESSAGE_WITH_EXPRESSION(
                   false, FailureExpression, "Test Message: %s", kTestMessage),
               "");
}

TEST_F(AssertTest, FirebaseAssertMessageAborts) {
  EXPECT_DEATH(FIREBASE_ASSERT_MESSAGE(false, "Test Message: %s", kTestMessage),
               "");
}

int FirebaseAssertMessageReturnInt(int return_value) {
  FIREBASE_ASSERT_MESSAGE_RETURN(return_value, false, "Test Message: %s",
                                 kTestMessage);
  return 0;
}

TEST_F(AssertTest, FirebaseAssertMessageReturnAborts) {
  EXPECT_DEATH(FirebaseAssertMessageReturnInt(1), "");
}

TEST_F(AssertTest, FirebaseAssertMessageReturnReturnsInt) {
  auto callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int return_value = 1;
  EXPECT_THAT(FirebaseAssertMessageReturnInt(return_value), Eq(return_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr(kTestMessage));
  delete callback_data;
}

TEST_F(AssertTest, FirebaseAssertMessageReturnVoidAborts) {
  EXPECT_DEATH(FIREBASE_ASSERT_MESSAGE_RETURN_VOID(false, "Test Message: %s",
                                                   kTestMessage),
               "");
}

void FirebaseAssertMessageReturnVoid(int in_value, int* out_value) {
  FIREBASE_ASSERT_MESSAGE_RETURN_VOID(false, "Test Message: %s", kTestMessage);
  *out_value = in_value;
}

TEST_F(AssertTest, FirebaseAssertMessageReturnVoidReturnsVoid) {
  auto callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int in_value = 1;
  int out_value = 0;
  FirebaseAssertMessageReturnVoid(in_value, &out_value);
  EXPECT_THAT(out_value, Ne(in_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr(kTestMessage));
  delete callback_data;
}

#if !defined(NDEBUG)

// Tests that check the functionality of FIREBASE_DEV_ASSERT_* macros in debug
// builds only.

TEST_F(AssertTest, FirebaseDevAssertWithExpressionAborts) {
  EXPECT_DEATH(FIREBASE_DEV_ASSERT_WITH_EXPRESSION(false, FailureExpression),
               "");
}

TEST_F(AssertTest, FirebaseDevAssertAborts) {
  EXPECT_DEATH(FIREBASE_DEV_ASSERT(false), "");
}

TEST_F(AssertTest, FirebaseDevAssertReturnAborts) {
  EXPECT_DEATH(FirebaseAssertReturnInt(1), "");
}

TEST_F(AssertTest, FirebaseDevAssertReturnReturnsInt) {
  auto* callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int return_value = 1;
  EXPECT_THAT(FirebaseAssertReturnInt(return_value), Eq(return_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr("false"));
  delete callback_data;
}

TEST_F(AssertTest, FirebaseDevAssertReturnVoidAborts) {
  EXPECT_DEATH(FIREBASE_DEV_ASSERT_RETURN_VOID(false), "");
}

TEST_F(AssertTest, FirebaseDevAssertReturnVoidReturnsVoid) {
  auto* callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int in_value = 1;
  int out_value = 0;
  FirebaseAssertReturnVoid(in_value, &out_value);
  EXPECT_THAT(out_value, Ne(in_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr("false"));
  delete callback_data;
}

TEST_F(AssertTest, FirebaseDevAssertMessageWithExpressionAborts) {
  EXPECT_DEATH(FIREBASE_DEV_ASSERT_MESSAGE_WITH_EXPRESSION(
                   false, FailureExpression, "Test Message: %s", kTestMessage),
               "");
}

TEST_F(AssertTest, FirebaseDevAssertMessageAborts) {
  EXPECT_DEATH(
      FIREBASE_DEV_ASSERT_MESSAGE(false, "Test Message: %s", kTestMessage), "");
}

TEST_F(AssertTest, FirebaseDevAssertMessageReturnAborts) {
  EXPECT_DEATH(FirebaseAssertMessageReturnInt(1), "");
}

TEST_F(AssertTest, FirebaseDevAssertMessageReturnReturnsInt) {
  auto callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int return_value = 1;
  EXPECT_THAT(FirebaseAssertMessageReturnInt(return_value), Eq(return_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr(kTestMessage));
  delete callback_data;
}

TEST_F(AssertTest, FirebaseDevAssertMessageReturnVoidAborts) {
  EXPECT_DEATH(FIREBASE_DEV_ASSERT_MESSAGE_RETURN_VOID(
                   false, "Test Message: %s", kTestMessage),
               "");
}

TEST_F(AssertTest, FirebaseDevAssertMessageReturnVoidReturnsVoid) {
  auto callback_data = new CallbackData();
  LogSetCallback(TestLogCallback, callback_data);
  int in_value = 1;
  int out_value = 0;
  FirebaseAssertMessageReturnVoid(in_value, &out_value);
  EXPECT_THAT(out_value, Ne(in_value));
  EXPECT_THAT(callback_data->log_level, Eq(LogLevel::kLogLevelAssert));
  EXPECT_THAT(callback_data->message, HasSubstr(kTestMessage));
  delete callback_data;
}

#else

// Tests that check that FIREBASE_DEV_ASSERT_* macros are compiled out of
// release builds.

TEST_F(AssertTest, FirebaseDevAssertWithExpressionCompiledOut) {
  FIREBASE_DEV_ASSERT_WITH_EXPRESSION(false, FailureExpression);
}

TEST_F(AssertTest, FirebaseDevAssertCompiledOut) { FIREBASE_DEV_ASSERT(false); }

TEST_F(AssertTest, FirebaseDevAssertReturnCompiledOut) {
  FIREBASE_DEV_ASSERT_RETURN(1, false);
}

TEST_F(AssertTest, FirebaseDevAssertReturnVoidCompiledOut) {
  FIREBASE_DEV_ASSERT_RETURN_VOID(false);
}

TEST_F(AssertTest, FirebaseDevAssertMessageWithExpressionCompiledOut) {
  FIREBASE_DEV_ASSERT_MESSAGE_WITH_EXPRESSION(false, FailureExpression,
                                              "Test Message: %s", kTestMessage);
}

TEST_F(AssertTest, FirebaseDevAssertMessageCompiledOut) {
  FIREBASE_DEV_ASSERT_MESSAGE(false, "Test Message: %s", kTestMessage);
}

TEST_F(AssertTest, FirebaseDevAssertMessageReturnCompiledOut){
    FIREBASE_DEV_ASSERT_MESSAGE_RETURN(1, false, "Test Message: %s",
                                       kTestMessage)}

TEST_F(AssertTest, FirebaseDevAssertMessageReturnVoidAborts) {
  FIREBASE_DEV_ASSERT_MESSAGE_RETURN_VOID(false, "Test Message: %s",
                                          kTestMessage);
}

#endif  // !defined(NDEBUG)

}  // namespace
}  // namespace firebase
