/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/jni/arena_ref.h"

#include <string>
#include <vector>

#include "android/firestore_integration_test_android.h"
#include "firestore/src/jni/env.h"

#include "gtest/gtest.h"

namespace {

using firebase::firestore::FirestoreAndroidIntegrationTest;
using firebase::firestore::jni::AdoptExisting;
using firebase::firestore::jni::ArenaRef;
using firebase::firestore::jni::Env;
using firebase::firestore::jni::Local;
using firebase::firestore::jni::Object;

class ArenaRefTest : public FirestoreAndroidIntegrationTest {
 public:
  ~ArenaRefTest() override {
    Env env;
    for (jstring java_string : java_strings_) {
      env.get()->DeleteGlobalRef(java_string);
    }
  }

  jstring NewJavaString(Env& env, const char* contents_modified_utf8) {
    jstring java_string = env.get()->NewStringUTF(contents_modified_utf8);
    java_strings_.push_back(java_string);
    return java_string;
  }

 private:
  std::vector<jstring> java_strings_;
};

TEST_F(ArenaRefTest, DefaultConstructorShouldCreateInvalidObject) {
  ArenaRef default_constructed_arena_ref;
  EXPECT_EQ(default_constructed_arena_ref.is_valid(), false);
}

TEST_F(ArenaRefTest, AdoptingConstructorShouldAcceptNull) {
  GTEST_SKIP();
  Env env;

  ArenaRef arena_ref_with_null_object(env, nullptr, AdoptExisting::kYes);

  EXPECT_EQ(arena_ref_with_null_object.Get(env).get(), nullptr);
}

TEST_F(ArenaRefTest, AdoptingConstructorShouldAcceptNonNull) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");

  ArenaRef arena_ref_with_non_null_object(env, java_string, AdoptExisting::kYes);

  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_with_non_null_object.Get(env).get(), java_string));
}

}  // namespace
