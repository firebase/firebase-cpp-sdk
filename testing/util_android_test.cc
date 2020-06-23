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

#include <jni.h>

#include <string>
#include <vector>

#include "testing/run_all_tests.h"
#include "testing/util_android.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace testing {
namespace cppsdk {

class UtilTest : public ::testing::Test {
 protected:
  JNIEnv* env_ = GetTestJniEnv();
};

TEST_F(UtilTest, JavaStringToString) {
  jstring java_string = env_->NewStringUTF("hello world");
  std::string cc_string = util::JavaStringToStdString(env_, java_string);
  env_->DeleteLocalRef(java_string);
  EXPECT_EQ(cc_string, "hello world");
}

TEST_F(UtilTest, JavaStringToStringWithEmptyJavaString) {
  jstring java_string = env_->NewStringUTF(nullptr);
  std::string cc_string = util::JavaStringToStdString(env_, java_string);
  env_->DeleteLocalRef(java_string);
  EXPECT_EQ(cc_string, "");
}

TEST_F(UtilTest, JavaStringListToStdStringVector) {
  std::vector<std::string> arr = {"one", "two", "three", "four", "five"};

  jclass jarray_list_class = env_->FindClass("java/util/ArrayList");
  jobject jarray_list = env_->NewObject(
      jarray_list_class, env_->GetMethodID(jarray_list_class, "<init>", "()V"));

  for (const std::string& s : arr) {
    jstring java_string = env_->NewStringUTF(s.c_str());
    env_->CallBooleanMethod(
        jarray_list,
        env_->GetMethodID(jarray_list_class, "add", "(Ljava/lang/Object;)Z"),
        java_string);
    util::CheckAndClearException(env_);
    env_->DeleteLocalRef(java_string);
  }

  EXPECT_THAT(util::JavaStringListToStdStringVector(env_, jarray_list),
              ::testing::Eq(arr));

  env_->DeleteLocalRef(jarray_list_class);
  env_->DeleteLocalRef(jarray_list);
}

TEST_F(UtilTest, JavaStringListToStdStringVectorWithEmptyJavaList) {
  jclass jarray_list_class = env_->FindClass("java/util/ArrayList");
  jobject jarray_list = env_->NewObject(
      jarray_list_class, env_->GetMethodID(jarray_list_class, "<init>", "()V"));

  EXPECT_THAT(util::JavaStringListToStdStringVector(env_, jarray_list),
              ::testing::Eq(std::vector<std::string>()));

  env_->DeleteLocalRef(jarray_list_class);
  env_->DeleteLocalRef(jarray_list);
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
