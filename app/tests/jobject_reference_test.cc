/*
 * Copyright 2019 Google LLC
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

#include "app/src/jni/string.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
// For ::firebase::jni::JStringToString.
#include "testing/run_all_tests.h"

using firebase::internal::jni::Global<jni::Object>;
using ::firebase::jni::JStringToString;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;

JOBJECT_REFERENCE(jni::Global<jni::Object>Alias);

// Tests for jni::Global<jni::Object>.
class jni::Global<jni::Object>Test : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = firebase::testing::cppsdk::GetTestJniEnv();
    ASSERT_TRUE(env_ != nullptr);
  }

  JNIEnv *env_;

  static const char *const kTestString;
};

const char *const jni::Global<jni::Object>Test::kTestString = "Testing testing 1 2 3";

TEST_F(jni::Global<jni::Object>Test, ConstructEmpty) {
  jni::Global<jni::Object> ref(env_);
  jni::Global<jni::Object>Alias alias(env_);
  EXPECT_THAT(ref.GetJNIEnv(), Eq(env_));
  EXPECT_THAT(ref.java_vm(), NotNull());
  EXPECT_THAT(ref.get(), IsNull());
  EXPECT_THAT(*ref, IsNull());
  EXPECT_THAT(alias.GetJNIEnv(), Eq(env_));
  EXPECT_THAT(alias.java_vm(), NotNull());
  EXPECT_THAT(alias.get(), IsNull());
  EXPECT_THAT(*alias, IsNull());
}

TEST_F(jni::Global<jni::Object>Test, ConstructDestruct) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jni::Global<jni::Object> ref(env_, java_string);
  jni::Global<jni::Object>Alias alias(env_, java_string);
  env_->DeleteLocalRef(java_string);
  EXPECT_THAT(JStringToString(env_, ref.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, *ref), Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, *alias), Eq(std::string(kTestString)));
}

TEST_F(jni::Global<jni::Object>Test, CopyConstruct) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jni::Global<jni::Object> ref1(env_, java_string);
  env_->DeleteLocalRef(java_string);
  jni::Global<jni::Object> ref2(ref1);
  jni::Global<jni::Object>Alias alias1(ref1);
  jni::Global<jni::Object>Alias alias2(alias1);
  EXPECT_THAT(JStringToString(env_, ref1.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, ref2.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias1.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias2.get()),
              Eq(std::string(kTestString)));
}

TEST_F(jni::Global<jni::Object>Test, Move) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jni::Global<jni::Object> ref1(env_, java_string);
  env_->DeleteLocalRef(java_string);
  jni::Global<jni::Object> ref2 = std::move(ref1);
  EXPECT_THAT(JStringToString(env_, ref2.get()),
              Eq(std::string(kTestString)));
  jni::Global<jni::Object>Alias alias1(std::move(ref2));
  EXPECT_THAT(JStringToString(env_, alias1.get()),
              Eq(std::string(kTestString)));
  jni::Global<jni::Object>Alias alias2(env_);
  alias2 = std::move(alias1);
  EXPECT_THAT(JStringToString(env_, alias2.get()),
              Eq(std::string(kTestString)));
}

TEST_F(jni::Global<jni::Object>Test, Copy) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jni::Global<jni::Object> ref1(env_, java_string);
  env_->DeleteLocalRef(java_string);
  jni::Global<jni::Object> ref2(env_);
  ref2 = ref1;
  jni::Global<jni::Object>Alias alias(env_);
  alias = ref2;
  EXPECT_THAT(JStringToString(env_, ref1.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, ref2.get()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias.get()),
              Eq(std::string(kTestString)));
}

TEST_F(jni::Global<jni::Object>Test, Set) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jni::Global<jni::Object> ref(env_, java_string);
  env_->DeleteLocalRef(java_string);
  EXPECT_THAT(JStringToString(env_, ref.get()),
              Eq(std::string(kTestString)));
  ref.Set(nullptr);
  EXPECT_THAT(ref.get(), IsNull());
}

TEST_F(jni::Global<jni::Object>Test, GetLocalRef) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jni::Global<jni::Object> ref(env_, java_string);
  jobject local = ref.GetLocalRef();
  EXPECT_THAT(JStringToString(env_, local), Eq(std::string(kTestString)));
  env_->DeleteLocalRef(local);

  jni::Global<jni::Object>Alias alias(env_, java_string);
  local = alias.GetLocalRef();
  EXPECT_THAT(JStringToString(env_, local), Eq(std::string(kTestString)));
  env_->DeleteLocalRef(local);
}

TEST_F(jni::Global<jni::Object>Test, FromGlobalReference) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jobject java_string_alias = env_->NewLocalRef(java_string);
  jni::Global<jni::Object> ref =
      jni::Global<jni::Object>::FromLocalReference(env_, java_string);
  jni::Global<jni::Object>Alias alias(
      jni::Global<jni::Object>Alias::FromLocalReference(env_, java_string_alias));
  EXPECT_NE(nullptr, ref.get());
  EXPECT_NE(nullptr, alias.get());

  jni::Global<jni::Object> nullref =
      jni::Global<jni::Object>::FromLocalReference(env_, nullptr);
  jni::Global<jni::Object>Alias alias_null(
      jni::Global<jni::Object>Alias::FromLocalReference(env_, nullptr));
  EXPECT_EQ(nullptr, nullref.get());
  EXPECT_EQ(nullptr, alias_null.get());
}
