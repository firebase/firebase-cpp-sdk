// Copyright 2020 Google
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

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#include <jni.h>
#include "testing/run_all_tests.h"
#elif defined(__APPLE__) && TARGET_OS_IPHONE
#include "testing/config_ios.h"
#else  // defined(FIREBASE_ANDROID_FOR_DESKTOP), defined(__APPLE__)
#include "testing/config_desktop.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP), defined(__APPLE__)

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/testdata_config_generated.h"
#include "flatbuffers/idl.h"

namespace firebase {
namespace testing {
namespace cppsdk {

const constexpr int64_t kNullObject = -1;
const constexpr int64_t kException = -2; // NOLINT

// Mimic what fake will do to get the test data provided by test user.
int64_t GetFutureBoolTicker(const char* fake) {
  int64_t result;

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)

  // Normally, we only send test data but not read test data in C++. Android
  // fakes read test data, which is in Java code. Here we use JNI calls to
  // simulate that scenario.
  JNIEnv* android_jni_env = GetTestJniEnv();
  jstring jfake = android_jni_env->NewStringUTF(fake);

  jclass config_cls = android_jni_env->FindClass(
      "com/google/testing/ConfigAndroid");
  jobject jrow = android_jni_env->CallStaticObjectMethod(
      config_cls,
      android_jni_env->GetStaticMethodID(
          config_cls, "get",
          "(Ljava/lang/String;)Lcom/google/testing/ConfigRow;"),
      jfake);

  // Catch any Java exception and thus the test itself does not die.
  if (android_jni_env->ExceptionCheck()) {
    android_jni_env->ExceptionDescribe();
    android_jni_env->ExceptionClear();
    result = kException;
  } else if (jrow == nullptr) {
    result = kNullObject;
  } else {
    jclass row_cls = android_jni_env->FindClass(
        "com/google/testing/ConfigRow");
    jobject jfuturebool = android_jni_env->CallObjectMethod(
        jrow, android_jni_env->GetMethodID(
                  row_cls, "futurebool",
                  "()Lcom/google/testing/FutureBool;"));
    EXPECT_EQ(android_jni_env->ExceptionCheck(), JNI_FALSE);
    android_jni_env->ExceptionClear();
    jclass futurebool_cls = android_jni_env->FindClass(
        "com/google/testing/FutureBool");
    jlong jticker = android_jni_env->CallLongMethod(
        jfuturebool,
        android_jni_env->GetMethodID(futurebool_cls, "ticker", "()J"));
    EXPECT_EQ(android_jni_env->ExceptionCheck(), JNI_FALSE);
    android_jni_env->ExceptionClear();

    android_jni_env->DeleteLocalRef(futurebool_cls);
    android_jni_env->DeleteLocalRef(jfuturebool);
    android_jni_env->DeleteLocalRef(row_cls);
    android_jni_env->DeleteLocalRef(jrow);
    result = jticker;
  }
  android_jni_env->DeleteLocalRef(config_cls);
  android_jni_env->DeleteLocalRef(jfake);

#else  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

  const ConfigRow* config = ConfigGet(fake);
  if (config == nullptr) {
    result = kNullObject;
  } else {
    EXPECT_EQ(fake, config->fake()->str());
    result = config->futurebool()->ticker();
  }

#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

  return result;
}

// Verify fake gets the data set by test user.
TEST(ConfigTest, TestConfigSetAndGet) {
  ConfigSet(
      "{"
      "  config:["
      "    {fake:'key',"
      "     futurebool:{value:Error,ticker:10}}"
      "  ]"
      "}");
  EXPECT_EQ(10, GetFutureBoolTicker("key"));
}

// Verify fake gets provided data for multiple fake case.
TEST(ConfigTest, TestConfigSetMultipleAndGet) {
  ConfigSet(
      "{"
      "  config:["
      "    {fake:'1',futurebool:{ticker:1}},"
      "    {fake:'7',futurebool:{ticker:7}},"
      "    {fake:'2',futurebool:{ticker:2}},"
      "    {fake:'6',futurebool:{ticker:6}},"
      "    {fake:'3',futurebool:{ticker:3}},"
      "    {fake:'5',futurebool:{ticker:5}},"
      "    {fake:'4',futurebool:{ticker:4}}"
      "  ]"
      "}");
  char fake[] = {0, 0};
  for (int i = 1; i <= 7; ++i) {
    fake[0] = '0' + i;
    EXPECT_EQ(i, GetFutureBoolTicker(fake));
  }
}

// Verify fake gets null if it is not specified by test user.
TEST(ConfigTest, TestConfigSetAndGetNothing) {
  ConfigSet(
      "{"
      "  config:["
      "    {fake:'key',"
      "     futurebool:{value:False,ticker:10}}"
      "  ]"
      "}");
  EXPECT_EQ(kNullObject, GetFutureBoolTicker("absence"));
}

// Test the reset of test config. Nothing to verify except to make sure code
// nothing is not broken.
TEST(ConfigTest, TestConfigReset) {
  ConfigSet("{}");
  ConfigReset();
}

// Verify exception raises when access the unset config.
TEST(ConfigDeathTest, TestConfigResetAndGet) {
  ConfigSet("{}");
  ConfigReset();
// Somehow the death test does not work on android emulator nor ios emulator.
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
  EXPECT_DEATH(GetFutureBoolTicker("absence"), "");
#endif  // !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
