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

#include "android/firestore_integration_test_android.h"

#include <string>

#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/string.h"

namespace firebase {
namespace firestore {
namespace {

using jni::ArrayList;
using jni::Env;
using jni::Local;
using jni::Object;
using jni::String;

using ::testing::Not;

TEST_F(FirestoreAndroidIntegrationTest, ToDebugStringWithNonNull) {
  Env env;

  std::string debug_string = ToDebugString(env.NewStringUtf("Test Value"));

  EXPECT_EQ(debug_string, "Test Value");
}

TEST_F(FirestoreAndroidIntegrationTest, ToDebugStringWithNull) {
  Object null_reference;

  std::string debug_string = ToDebugString(null_reference);

  EXPECT_EQ(debug_string, "null");
}

TEST_F(FirestoreAndroidIntegrationTest,
       ToDebugStringWithPendingExceptionAndNonNullObject) {
  Env env;
  Local<String> object = env.NewStringUtf("Test Value");
  env.Throw(CreateException(env, "forced exception"));
  ASSERT_FALSE(env.ok());

  std::string debug_string = ToDebugString(object);

  EXPECT_EQ(debug_string, "Test Value");
  env.ExceptionClear();
}

TEST_F(FirestoreAndroidIntegrationTest,
       ToDebugStringWithPendingExceptionAndNullObject) {
  Env env;
  Object null_reference;
  env.Throw(CreateException(env, "forced exception"));
  ASSERT_FALSE(env.ok());

  std::string debug_string = ToDebugString(null_reference);

  EXPECT_EQ(debug_string, "null");
  env.ExceptionClear();
}

TEST_F(FirestoreAndroidIntegrationTest, JavaEqShouldReturnTrueForEqualObjects) {
  Env env;
  Local<String> object1 = env.NewStringUtf("string");
  Local<String> object2 = env.NewStringUtf("string");

  EXPECT_THAT(object1, JavaEq(object2));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnFalseForUnequalObjects) {
  Env env;
  Local<String> object1 = env.NewStringUtf("string1");
  Local<String> object2 = env.NewStringUtf("string2");

  EXPECT_THAT(object1, Not(JavaEq(object2)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnTrueForTwoNullReferences) {
  Env env;
  Local<Object> null_reference1;
  Local<Object> null_reference2;

  EXPECT_THAT(null_reference1, JavaEq(null_reference2));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnFalseIfExactlyOneObjectIsNull) {
  Env env;
  Local<String> null_reference;
  Local<String> non_null_reference = env.NewStringUtf("string2");

  EXPECT_THAT(null_reference, Not(JavaEq(non_null_reference)));
  EXPECT_THAT(non_null_reference, Not(JavaEq(null_reference)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnFalseForObjectOfDifferentTypes) {
  Env env;
  Local<String> string_object = env.NewStringUtf("string2");
  Local<ArrayList> list_object = ArrayList::Create(env);

  EXPECT_THAT(string_object, Not(JavaEq(list_object)));
  EXPECT_THAT(list_object, Not(JavaEq(string_object)));
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
