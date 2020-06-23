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

#include "app/src/jobject_reference.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
// For firebase::util::JStringToString.
#include "app/src/util_android.h"
#include "testing/run_all_tests.h"

using firebase::internal::JObjectReference;
using firebase::util::JStringToString;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;

JOBJECT_REFERENCE(JObjectReferenceAlias);

// Tests for JObjectReference.
class JObjectReferenceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = firebase::testing::cppsdk::GetTestJniEnv();
    ASSERT_TRUE(env_ != nullptr);
  }

  JNIEnv *env_;

  static const char *const kTestString;
};

const char *const JObjectReferenceTest::kTestString = "Testing testing 1 2 3";

TEST_F(JObjectReferenceTest, ConstructEmpty) {
  JObjectReference ref(env_);
  JObjectReferenceAlias alias(env_);
  EXPECT_THAT(ref.GetJNIEnv(), Eq(env_));
  EXPECT_THAT(ref.java_vm(), NotNull());
  EXPECT_THAT(ref.object(), IsNull());
  EXPECT_THAT(*ref, IsNull());
  EXPECT_THAT(alias.GetJNIEnv(), Eq(env_));
  EXPECT_THAT(alias.java_vm(), NotNull());
  EXPECT_THAT(alias.object(), IsNull());
  EXPECT_THAT(*alias, IsNull());
}

TEST_F(JObjectReferenceTest, ConstructDestruct) {
  jobject java_string = env_->NewStringUTF(kTestString);
  JObjectReference ref(env_, java_string);
  JObjectReferenceAlias alias(env_, java_string);
  env_->DeleteLocalRef(java_string);
  EXPECT_THAT(JStringToString(env_, ref.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, *ref), Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, *alias), Eq(std::string(kTestString)));
}

TEST_F(JObjectReferenceTest, CopyConstruct) {
  jobject java_string = env_->NewStringUTF(kTestString);
  JObjectReference ref1(env_, java_string);
  env_->DeleteLocalRef(java_string);
  JObjectReference ref2(ref1);
  JObjectReferenceAlias alias1(ref1);
  JObjectReferenceAlias alias2(alias1);
  EXPECT_THAT(JStringToString(env_, ref1.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, ref2.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias1.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias2.object()),
              Eq(std::string(kTestString)));
}

TEST_F(JObjectReferenceTest, Move) {
  jobject java_string = env_->NewStringUTF(kTestString);
  JObjectReference ref1(env_, java_string);
  env_->DeleteLocalRef(java_string);
  JObjectReference ref2 = std::move(ref1);
  EXPECT_THAT(JStringToString(env_, ref2.object()),
              Eq(std::string(kTestString)));
  JObjectReferenceAlias alias1(std::move(ref2));
  EXPECT_THAT(JStringToString(env_, alias1.object()),
              Eq(std::string(kTestString)));
  JObjectReferenceAlias alias2(env_);
  alias2 = std::move(alias1);
  EXPECT_THAT(JStringToString(env_, alias2.object()),
              Eq(std::string(kTestString)));
}

TEST_F(JObjectReferenceTest, Copy) {
  jobject java_string = env_->NewStringUTF(kTestString);
  JObjectReference ref1(env_, java_string);
  env_->DeleteLocalRef(java_string);
  JObjectReference ref2(env_);
  ref2 = ref1;
  JObjectReferenceAlias alias(env_);
  alias = ref2;
  EXPECT_THAT(JStringToString(env_, ref1.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, ref2.object()),
              Eq(std::string(kTestString)));
  EXPECT_THAT(JStringToString(env_, alias.object()),
              Eq(std::string(kTestString)));
}

TEST_F(JObjectReferenceTest, Set) {
  jobject java_string = env_->NewStringUTF(kTestString);
  JObjectReference ref(env_, java_string);
  env_->DeleteLocalRef(java_string);
  EXPECT_THAT(JStringToString(env_, ref.object()),
              Eq(std::string(kTestString)));
  ref.Set(nullptr);
  EXPECT_THAT(ref.object(), IsNull());
}

TEST_F(JObjectReferenceTest, GetLocalRef) {
  jobject java_string = env_->NewStringUTF(kTestString);
  JObjectReference ref(env_, java_string);
  jobject local = ref.GetLocalRef();
  EXPECT_THAT(JStringToString(env_, local), Eq(std::string(kTestString)));
  env_->DeleteLocalRef(local);

  JObjectReferenceAlias alias(env_, java_string);
  local = alias.GetLocalRef();
  EXPECT_THAT(JStringToString(env_, local), Eq(std::string(kTestString)));
  env_->DeleteLocalRef(local);
}

TEST_F(JObjectReferenceTest, FromGlobalReference) {
  jobject java_string = env_->NewStringUTF(kTestString);
  jobject java_string_alias = env_->NewLocalRef(java_string);
  JObjectReference ref =
      JObjectReference::FromLocalReference(env_, java_string);
  JObjectReferenceAlias alias(
      JObjectReferenceAlias::FromLocalReference(env_, java_string_alias));
  EXPECT_NE(nullptr, ref.object());
  EXPECT_NE(nullptr, alias.object());

  JObjectReference nullref =
      JObjectReference::FromLocalReference(env_, nullptr);
  JObjectReferenceAlias alias_null(
      JObjectReferenceAlias::FromLocalReference(env_, nullptr));
  EXPECT_EQ(nullptr, nullref.object());
  EXPECT_EQ(nullptr, alias_null.object());
}
