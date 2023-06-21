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
using jni::Global;
using jni::Local;
using jni::Object;
using jni::String;
using jni::Throwable;

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StrEq;

TEST_F(FirestoreAndroidIntegrationTest, ToDebugStringWithNonNull) {
  std::string debug_string = ToDebugString(env().NewStringUtf("Test Value"));

  EXPECT_EQ(debug_string, "Test Value");
}

TEST_F(FirestoreAndroidIntegrationTest, ToDebugStringWithNull) {
  Object null_reference;

  std::string debug_string = ToDebugString(null_reference);

  EXPECT_EQ(debug_string, "null");
}

TEST_F(FirestoreAndroidIntegrationTest,
       ToDebugStringWithPendingExceptionAndNonNullObject) {
  Local<String> object = env().NewStringUtf("Test Value");
  ThrowException();
  ASSERT_FALSE(env().ok());

  std::string debug_string = ToDebugString(object);

  EXPECT_EQ(debug_string, "Test Value");
}

TEST_F(FirestoreAndroidIntegrationTest,
       ToDebugStringWithPendingExceptionAndNullObject) {
  Object null_reference;
  ThrowException();
  ASSERT_FALSE(env().ok());

  std::string debug_string = ToDebugString(null_reference);

  EXPECT_EQ(debug_string, "null");
}

TEST_F(FirestoreAndroidIntegrationTest, JavaEqShouldReturnTrueForEqualObjects) {
  Local<String> object1 = env().NewStringUtf("string");
  Local<String> object2 = env().NewStringUtf("string");

  EXPECT_THAT(object1, JavaEq(object2));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnFalseForUnequalObjects) {
  Local<String> object1 = env().NewStringUtf("string1");
  Local<String> object2 = env().NewStringUtf("string2");

  EXPECT_THAT(object1, Not(JavaEq(object2)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnTrueForTwoNullReferences) {
  Local<Object> null_reference1;
  Local<Object> null_reference2;

  EXPECT_THAT(null_reference1, JavaEq(null_reference2));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnFalseIfExactlyOneObjectIsNull) {
  Local<String> null_reference;
  Local<String> non_null_reference = env().NewStringUtf("string2");

  EXPECT_THAT(null_reference, Not(JavaEq(non_null_reference)));
  EXPECT_THAT(non_null_reference, Not(JavaEq(null_reference)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       JavaEqShouldReturnFalseForObjectOfDifferentTypes) {
  Local<String> string_object = env().NewStringUtf("string2");
  Local<ArrayList> list_object = ArrayList::Create(env());

  EXPECT_THAT(string_object, Not(JavaEq(list_object)));
  EXPECT_THAT(list_object, Not(JavaEq(string_object)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       RefersToSameJavaObjectAsShouldReturnTrueForSameObjects) {
  Local<String> object1 = env().NewStringUtf("string");
  Global<String> object2 = object1;

  EXPECT_THAT(object1, RefersToSameJavaObjectAs(object2));
}

TEST_F(FirestoreAndroidIntegrationTest,
       RefersToSameJavaObjectAsShouldReturnTrueForTwoNullReferences) {
  Local<Object> null_reference1;
  Local<Object> null_reference2;

  EXPECT_THAT(null_reference1, RefersToSameJavaObjectAs(null_reference2));
}

TEST_F(FirestoreAndroidIntegrationTest,
       RefersToSameJavaObjectAsShouldReturnFalseForDistinctObjects) {
  Local<String> object1 = env().NewStringUtf("test string");
  Local<String> object2 = env().NewStringUtf("test string");
  ASSERT_FALSE(env().IsSameObject(object1, object2));

  EXPECT_THAT(object1, Not(RefersToSameJavaObjectAs(object2)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       RefersToSameJavaObjectAsShouldReturnFalseIfExactlyOneObjectIsNull) {
  Local<String> null_reference;
  Local<String> non_null_reference = env().NewStringUtf("string2");

  EXPECT_THAT(null_reference,
              Not(RefersToSameJavaObjectAs(non_null_reference)));
  EXPECT_THAT(non_null_reference,
              Not(RefersToSameJavaObjectAs(null_reference)));
}

TEST_F(FirestoreAndroidIntegrationTest,
       ThrowExceptionWithNoMessageShouldSetPendingExceptionWithAMessage) {
  Local<Throwable> throw_exception_return_value = ThrowException();
  Local<Throwable> actually_thrown_exception = env().ClearExceptionOccurred();
  ASSERT_TRUE(actually_thrown_exception);
  EXPECT_THAT(actually_thrown_exception,
              RefersToSameJavaObjectAs(throw_exception_return_value));
  EXPECT_THAT(actually_thrown_exception.GetMessage(env()), Not(IsEmpty()));
}

TEST_F(FirestoreAndroidIntegrationTest,
       ThrowExceptionWithAMessageShouldSetPendingExceptionWithTheGivenMessage) {
  Local<Throwable> throw_exception_return_value =
      ThrowException("my test message");
  Local<Throwable> actually_thrown_exception = env().ClearExceptionOccurred();
  ASSERT_TRUE(actually_thrown_exception);
  EXPECT_THAT(actually_thrown_exception,
              RefersToSameJavaObjectAs(throw_exception_return_value));
  EXPECT_THAT(actually_thrown_exception.GetMessage(env()),
              StrEq("my test message"));
}

TEST_F(FirestoreAndroidIntegrationTest,
       CreateExceptionWithNoMessageShouldReturnAnExceptionWithAMessage) {
  Local<Throwable> exception = CreateException();
  ASSERT_TRUE(exception);
  EXPECT_THAT(exception.GetMessage(env()), Not(IsEmpty()));
}

TEST_F(FirestoreAndroidIntegrationTest,
       CreateExceptionWithAMessageShouldReturnAnExceptionWithTheGivenMessage) {
  Local<Throwable> exception = CreateException("my test message");
  ASSERT_TRUE(exception);
  EXPECT_THAT(exception.GetMessage(env()), StrEq("my test message"));
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
