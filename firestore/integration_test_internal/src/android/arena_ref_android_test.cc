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

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "android/firestore_integration_test_android.h"
#include "firestore/src/jni/env.h"

#include "gmock/gmock.h"
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
    for (jobject created_java_object : created_java_objects_) {
      env().get()->DeleteGlobalRef(created_java_object);
    }
  }

  /**
   * Creates and returns a brand new Java object.
   *
   * The returned object is a global reference that this object retains
   * ownership of. The global reference will be automatically deleted by this
   * object's destructor.
   *
   * If creating the new Java object fails, such as if this function is called
   * with a pending Java exception, then a null object is returned and the
   * calling test case will fail.
   *
   * @return a global reference to the newly-created Java object, or null if
   * creating the object fails.
   */
  jobject NewJavaObject() {
    SCOPED_TRACE("NewJavaObject");

    JNIEnv* jni_env = env().get();
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "NewJavaObject() called with a pending exception";
      return {};
    }

    jclass object_class = jni_env->FindClass("java/lang/Object");
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "JNIEnv::FindClass() failed";
      return {};
    }

    jmethodID object_constructor_id =
        jni_env->GetMethodID(object_class, "<init>", "()V");
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "JNIEnv::GetMethodID() failed";
      return {};
    }

    jobject object_local_ref =
        jni_env->NewObject(object_class, object_constructor_id);
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "JNIEnv::NewObject() failed";
      return {};
    }

    jobject object_global_ref = jni_env->NewGlobalRef(object_local_ref);
    jni_env->DeleteLocalRef(object_local_ref);
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "JNIEnv::NewGlobalRef() failed";
      return {};
    }

    created_java_objects_.push_back(object_global_ref);
    return object_global_ref;
  }

 private:
  std::vector<jobject> created_java_objects_;
};

/**
 * A googletest "matcher" that verifies that an ArenaRef refers to a null Java
 * object.
 *
 * Example:
 * ArenaRef arena_ref;
 * EXPECT_THAT(arena_ref, RefersToNullJavaObject());
 */
MATCHER(RefersToNullJavaObject, "") {
  static_assert(std::is_same<arg_type, const ArenaRef&>::value, "");

  Env env;
  if (!env.ok()) {
    ADD_FAILURE() << "RefersToNullJavaObject called with pending exception";
    return false;
  }

  Local<Object> object = arg.get(env);
  if (!env.ok()) {
    ADD_FAILURE() << "RefersToNullJavaObject ArenaRef.get() threw an exception";
    return false;
  }

  return object.get() == nullptr;
}

/**
 * A googletest "matcher" that verifies that an ArenaRef refers to the given
 * Java object, compared as if using the `==` operator in Java.
 *
 * Example:
 * jobject java_object = NewJavaObject();
 * ArenaRef arena_ref(env, java_object);
 * EXPECT_THAT(arena_ref, RefersToJavaObject(java_object));
 */
MATCHER_P(RefersToJavaObject, expected_jobject, "") {
  static_assert(std::is_same<arg_type, const ArenaRef&>::value, "");
  static_assert(std::is_same<expected_jobject_type, jobject>::value, "");

  Env env;
  if (!env.ok()) {
    ADD_FAILURE() << "RefersToJavaObject called with pending exception";
    return false;
  }

  Local<Object> object = arg.get(env);
  if (!env.ok()) {
    ADD_FAILURE() << "RefersToJavaObject ArenaRef.get() threw an exception";
    return false;
  }

  return env.get()->IsSameObject(object.get(), expected_jobject);
}

////////////////////////////////////////////////////////////////////////////////
// Tests for ArenaRef::ArenaRef()
////////////////////////////////////////////////////////////////////////////////

TEST_F(ArenaRefTest, DefaultConstructorShouldReferToNull) {
  ArenaRef arena_ref;

  EXPECT_THAT(arena_ref, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest,
       DefaultConstructorShouldSucceedIfCalledWithPendingException) {
  ThrowException();
  ClearCurrentExceptionAfterTest();

  ArenaRef arena_ref;

  env().ClearExceptionOccurred();
  EXPECT_THAT(arena_ref, RefersToNullJavaObject());
}

////////////////////////////////////////////////////////////////////////////////
// Tests for ArenaRef::ArenaRef(Env&, jobject)
////////////////////////////////////////////////////////////////////////////////

TEST_F(ArenaRefTest, AdoptingConstructorWithNullptrShouldReferToNull) {
  const ArenaRef arena_ref(env(), nullptr);

  EXPECT_THAT(arena_ref, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest, AdoptingConstructorShouldReferToTheGivenObject) {
  jobject java_object = NewJavaObject();

  ArenaRef arena_ref(env(), java_object);

  EXPECT_THAT(arena_ref, RefersToJavaObject(java_object));
}

TEST_F(ArenaRefTest,
       AdoptingConstructorShouldReferToNullIfCalledWithPendingException) {
  jobject java_object = NewJavaObject();
  ThrowException();
  ClearCurrentExceptionAfterTest();

  ArenaRef arena_ref(env(), java_object);

  env().ClearExceptionOccurred();
  EXPECT_THAT(arena_ref, RefersToNullJavaObject());
}

////////////////////////////////////////////////////////////////////////////////
// Tests for ArenaRef::ArenaRef(const ArenaRef&) a.k.a. (copy constructor)
////////////////////////////////////////////////////////////////////////////////

TEST_F(ArenaRefTest, CopyConstructorWithDefaultConstructedInstance) {
  const ArenaRef default_arena_ref;

  ArenaRef arena_ref_copy_dest(default_arena_ref);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest, CopyConstructorWithNull) {
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);

  ArenaRef arena_ref_copy_dest(arena_ref_referring_to_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest, CopyConstructorWithNonNull) {
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);

  ArenaRef arena_ref_copy_dest(arena_ref_referring_to_non_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToJavaObject(java_object));
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
}

TEST_F(ArenaRefTest, CopyConstructorShouldCopyIfCalledWithPendingException) {
  const ArenaRef default_arena_ref;
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  ThrowException();
  ClearCurrentExceptionAfterTest();

  ArenaRef default_arena_ref_copy_dest(default_arena_ref);
  ArenaRef arena_ref_referring_to_null_copy_dest(arena_ref_referring_to_null);
  ArenaRef arena_ref_referring_to_non_null_copy_dest(
      arena_ref_referring_to_non_null);

  env().ClearExceptionOccurred();
  EXPECT_THAT(default_arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    ObjectCreatedWithTheCopyConstructorShouldBeUnaffectedByChangesToTheCopiedObject) {
  auto default_arena_ref = std::make_unique<ArenaRef>();
  const ArenaRef default_arena_ref_copy_dest(*default_arena_ref);
  auto arena_ref_referring_to_null = std::make_unique<ArenaRef>(env(), nullptr);
  const ArenaRef arena_ref_referring_to_null_copy_dest(
      *arena_ref_referring_to_null);
  jobject java_object = NewJavaObject();
  auto arena_ref_referring_to_non_null =
      std::make_unique<ArenaRef>(env(), java_object);
  const ArenaRef arena_ref_referring_to_non_null_copy_dest(
      *arena_ref_referring_to_non_null);

  jobject java_object1 = NewJavaObject();
  default_arena_ref->reset(env(), Object(java_object1));
  jobject java_object2 = NewJavaObject();
  arena_ref_referring_to_null->reset(env(), Object(java_object2));
  jobject java_object3 = NewJavaObject();
  arena_ref_referring_to_non_null->reset(env(), Object(java_object3));

  EXPECT_THAT(default_arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object));
  EXPECT_THAT(*default_arena_ref, RefersToJavaObject(java_object1));
  EXPECT_THAT(*arena_ref_referring_to_null, RefersToJavaObject(java_object2));
  EXPECT_THAT(*arena_ref_referring_to_non_null,
              RefersToJavaObject(java_object3));

  default_arena_ref.reset();
  arena_ref_referring_to_null.reset();
  arena_ref_referring_to_non_null.reset();

  EXPECT_THAT(default_arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    ChangesToAnObjectCreatedWithTheCopyConstructorShouldNotAffectTheCopiedObject) {
  const ArenaRef default_arena_ref;
  auto default_arena_ref_copy_dest =
      std::make_unique<ArenaRef>(default_arena_ref);
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);
  auto arena_ref_referring_to_null_copy_dest =
      std::make_unique<ArenaRef>(arena_ref_referring_to_null);
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  auto arena_ref_referring_to_non_null_copy_dest =
      std::make_unique<ArenaRef>(arena_ref_referring_to_non_null);

  jobject java_object1 = NewJavaObject();
  default_arena_ref_copy_dest->reset(env(), Object(java_object1));
  jobject java_object2 = NewJavaObject();
  arena_ref_referring_to_null_copy_dest->reset(env(), Object(java_object2));
  jobject java_object3 = NewJavaObject();
  arena_ref_referring_to_non_null_copy_dest->reset(env(), Object(java_object3));

  EXPECT_THAT(*default_arena_ref_copy_dest, RefersToJavaObject(java_object1));
  EXPECT_THAT(*arena_ref_referring_to_null_copy_dest,
              RefersToJavaObject(java_object2));
  EXPECT_THAT(*arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object3));
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));

  default_arena_ref_copy_dest.reset();
  arena_ref_referring_to_null_copy_dest.reset();
  arena_ref_referring_to_non_null_copy_dest.reset();

  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
}

////////////////////////////////////////////////////////////////////////////////
// Tests for ArenaRef::ArenaRef(ArenaRef&&) (move constructor)
////////////////////////////////////////////////////////////////////////////////

TEST_F(ArenaRefTest, MoveConstructorWithDefaultConstructedInstance) {
  ArenaRef default_arena_ref;

  ArenaRef arena_ref_move_dest(std::move(default_arena_ref));

  EXPECT_THAT(arena_ref_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest, MoveConstructorWithNull) {
  ArenaRef arena_ref_referring_to_null(env(), nullptr);

  ArenaRef arena_ref_move_dest(std::move(arena_ref_referring_to_null));

  EXPECT_THAT(arena_ref_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest, MoveConstructorWithNonNull) {
  jobject java_object = NewJavaObject();
  ArenaRef arena_ref_referring_to_non_null(env(), java_object);

  ArenaRef arena_ref_move_dest(std::move(arena_ref_referring_to_non_null));

  EXPECT_THAT(arena_ref_move_dest, RefersToJavaObject(java_object));
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToNullJavaObject());
}

TEST_F(ArenaRefTest,
       MoveConstructorShouldSuccessfullyMoveEvenIfCalledWithPendingException) {
  ArenaRef default_arena_ref;
  ArenaRef arena_ref_referring_to_null(env(), nullptr);
  jobject java_object = NewJavaObject();
  ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  ThrowException();
  ClearCurrentExceptionAfterTest();

  ArenaRef default_arena_ref_move_dest(std::move(default_arena_ref));
  ArenaRef arena_ref_referring_to_null_move_dest(
      std::move(arena_ref_referring_to_null));
  ArenaRef arena_ref_referring_to_non_null_move_dest(
      std::move(arena_ref_referring_to_non_null));

  env().ClearExceptionOccurred();
  EXPECT_THAT(default_arena_ref_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_move_dest,
              RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    ObjectCreatedWithTheMoveConstructorShouldBeUnaffectedByChangesToTheMovedFromObject) {
  auto default_arena_ref = std::make_unique<ArenaRef>();
  ArenaRef default_arena_ref_move_dest(std::move(*default_arena_ref));
  auto arena_ref_referring_to_null = std::make_unique<ArenaRef>(env(), nullptr);
  ArenaRef arena_ref_referring_to_null_move_dest(
      std::move(*arena_ref_referring_to_null));
  jobject java_object = NewJavaObject();
  auto arena_ref_referring_to_non_null =
      std::make_unique<ArenaRef>(env(), java_object);
  ArenaRef arena_ref_referring_to_non_null_move_dest(
      std::move(*arena_ref_referring_to_non_null));

  jobject java_object1 = NewJavaObject();
  default_arena_ref->reset(env(), Object(java_object1));
  jobject java_object2 = NewJavaObject();
  arena_ref_referring_to_null->reset(env(), Object(java_object2));
  jobject java_object3 = NewJavaObject();
  arena_ref_referring_to_non_null->reset(env(), Object(java_object3));

  EXPECT_THAT(default_arena_ref_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_move_dest,
              RefersToJavaObject(java_object));
  EXPECT_THAT(*default_arena_ref, RefersToJavaObject(java_object1));
  EXPECT_THAT(*arena_ref_referring_to_null, RefersToJavaObject(java_object2));
  EXPECT_THAT(*arena_ref_referring_to_non_null,
              RefersToJavaObject(java_object3));

  default_arena_ref.reset();
  arena_ref_referring_to_null.reset();
  arena_ref_referring_to_non_null.reset();

  EXPECT_THAT(default_arena_ref_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_move_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_move_dest,
              RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    ChangesToAnObjectCreatedTheWithMoveConstructorShouldNotAffectTheMovedFromObject) {
  ArenaRef default_arena_ref;
  auto default_arena_ref_move_dest =
      std::make_unique<ArenaRef>(std::move(default_arena_ref));
  ArenaRef arena_ref_referring_to_null(env(), nullptr);
  auto arena_ref_referring_to_null_move_dest =
      std::make_unique<ArenaRef>(std::move(arena_ref_referring_to_null));
  jobject java_object = NewJavaObject();
  ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  auto arena_ref_referring_to_non_null_move_dest =
      std::make_unique<ArenaRef>(std::move(arena_ref_referring_to_non_null));

  jobject java_object1 = NewJavaObject();
  default_arena_ref_move_dest->reset(env(), Object(java_object1));
  jobject java_object2 = NewJavaObject();
  arena_ref_referring_to_null_move_dest->reset(env(), Object(java_object2));
  jobject java_object3 = NewJavaObject();
  arena_ref_referring_to_non_null_move_dest->reset(env(), Object(java_object3));

  EXPECT_THAT(*default_arena_ref_move_dest, RefersToJavaObject(java_object1));
  EXPECT_THAT(*arena_ref_referring_to_null_move_dest,
              RefersToJavaObject(java_object2));
  EXPECT_THAT(*arena_ref_referring_to_non_null_move_dest,
              RefersToJavaObject(java_object3));
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToNullJavaObject());

  default_arena_ref_move_dest.reset();
  arena_ref_referring_to_null_move_dest.reset();
  arena_ref_referring_to_non_null_move_dest.reset();

  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToNullJavaObject());
}

////////////////////////////////////////////////////////////////////////////////
// Tests for ArenaRef::operator=(const ArenaRef&) (copy assignment operator)
////////////////////////////////////////////////////////////////////////////////

TEST_F(ArenaRefTest,
       CopyAssignmentOpCorrectlyAssignsADefaultInstanceFromADefaultInstance) {
  ArenaRef arena_ref_copy_dest;
  const ArenaRef default_arena_ref;

  const ArenaRef& return_value = (arena_ref_copy_dest = default_arena_ref);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsADefaultInstanceFromAnInstanceReferringToNull) {
  ArenaRef arena_ref_copy_dest;
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);

  const ArenaRef& return_value =
      (arena_ref_copy_dest = arena_ref_referring_to_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsADefaultInstanceFromAnInstanceReferringToNonNull) {
  ArenaRef arena_ref_copy_dest;
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);

  const ArenaRef& return_value =
      (arena_ref_copy_dest = arena_ref_referring_to_non_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToJavaObject(java_object));
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsAnInstanceReferringToNullFromADefaultInstance) {
  ArenaRef arena_ref_copy_dest(env(), nullptr);
  const ArenaRef default_arena_ref;

  const ArenaRef& return_value = (arena_ref_copy_dest = default_arena_ref);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsAnInstanceReferringToNullFromAnInstanceReferringToNull) {
  ArenaRef arena_ref_copy_dest(env(), nullptr);
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);

  const ArenaRef& return_value =
      (arena_ref_copy_dest = arena_ref_referring_to_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsAnInstanceReferringToNullFromAnInstanceReferringToNonNull) {
  ArenaRef arena_ref_copy_dest(env(), nullptr);
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);

  const ArenaRef& return_value =
      (arena_ref_copy_dest = arena_ref_referring_to_non_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToJavaObject(java_object));
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsAnInstanceReferringToNonNullFromADefaultInstance) {
  jobject java_object = NewJavaObject();
  ArenaRef arena_ref_copy_dest(env(), java_object);
  const ArenaRef default_arena_ref;

  const ArenaRef& return_value = (arena_ref_copy_dest = default_arena_ref);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsAnInstanceReferringToNonNullFromAnInstanceReferringToNull) {
  jobject java_object = NewJavaObject();
  ArenaRef arena_ref_copy_dest(env(), java_object);
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);

  const ArenaRef& return_value =
      (arena_ref_copy_dest = arena_ref_referring_to_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsAnInstanceReferringToNonNullFromAnInstanceReferringToNonNull) {
  jobject original_java_object = NewJavaObject();
  ArenaRef arena_ref_copy_dest(env(), original_java_object);
  jobject new_java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), new_java_object);

  const ArenaRef& return_value =
      (arena_ref_copy_dest = arena_ref_referring_to_non_null);

  EXPECT_THAT(arena_ref_copy_dest, RefersToJavaObject(new_java_object));
  EXPECT_THAT(arena_ref_referring_to_non_null,
              RefersToJavaObject(new_java_object));
  EXPECT_EQ(&return_value, &arena_ref_copy_dest);
}

TEST_F(ArenaRefTest,
       CopyAssignmentOpCorrectlyAssignsSelfWhenSelfIsDefaultInstance) {
  ArenaRef default_arena_ref;

  const ArenaRef& return_value = (default_arena_ref = default_arena_ref);

  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &default_arena_ref);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsSelfWhenSelfIsAnInstanceReferringToNull) {
  ArenaRef arena_ref_referring_to_null(env(), nullptr);

  const ArenaRef& return_value =
      (arena_ref_referring_to_null = arena_ref_referring_to_null);

  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_EQ(&return_value, &arena_ref_referring_to_null);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpCorrectlyAssignsSelfWhenSelfIsAnInstanceReferringToNonNull) {
  jobject java_object = NewJavaObject();
  ArenaRef arena_ref_referring_to_non_null(env(), java_object);

  const ArenaRef& return_value =
      (arena_ref_referring_to_non_null = arena_ref_referring_to_non_null);

  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
  EXPECT_EQ(&return_value, &arena_ref_referring_to_non_null);
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpOnADefaultInstanceShouldCopyIfCalledWithPendingException) {
  ArenaRef default_arena_ref;
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  ThrowException();
  ClearCurrentExceptionAfterTest();

  const ArenaRef& return_value =
      (default_arena_ref = arena_ref_referring_to_non_null);

  env().ClearExceptionOccurred();
  EXPECT_THAT(default_arena_ref, RefersToJavaObject(java_object));
  EXPECT_EQ(&return_value, &default_arena_ref);
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpOnAnInstanceReferringToNullShouldCopyIfCalledWithPendingException) {
  ArenaRef arena_ref_referring_to_null(env(), nullptr);
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  ThrowException();
  ClearCurrentExceptionAfterTest();

  const ArenaRef& return_value =
      (arena_ref_referring_to_null = arena_ref_referring_to_non_null);

  env().ClearExceptionOccurred();
  EXPECT_THAT(arena_ref_referring_to_null, RefersToJavaObject(java_object));
  EXPECT_EQ(&return_value, &arena_ref_referring_to_null);
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    CopyAssignmentOpOnAnInstanceReferringToNonNullShouldCopyIfCalledWithPendingException) {
  ArenaRef arena_ref_referring_to_non_null(env(), NewJavaObject());
  jobject java_object = NewJavaObject();
  const ArenaRef another_arena_ref_referring_to_non_null(env(), java_object);
  ThrowException();
  ClearCurrentExceptionAfterTest();

  const ArenaRef& return_value = (arena_ref_referring_to_non_null =
                                      another_arena_ref_referring_to_non_null);

  env().ClearExceptionOccurred();
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
  EXPECT_EQ(&return_value, &arena_ref_referring_to_non_null);
  EXPECT_THAT(another_arena_ref_referring_to_non_null,
              RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    DestObjectOfCopyAssignmentOperatorShouldBeUnaffectedByChangesToSourceObject) {
  FAIL() << "NOT IMPLEMENTED!";
  auto default_arena_ref = std::make_unique<ArenaRef>();
  const ArenaRef default_arena_ref_copy_dest(*default_arena_ref);
  auto arena_ref_referring_to_null = std::make_unique<ArenaRef>(env(), nullptr);
  const ArenaRef arena_ref_referring_to_null_copy_dest(
      *arena_ref_referring_to_null);
  jobject java_object = NewJavaObject();
  auto arena_ref_referring_to_non_null =
      std::make_unique<ArenaRef>(env(), java_object);
  const ArenaRef arena_ref_referring_to_non_null_copy_dest(
      *arena_ref_referring_to_non_null);

  jobject java_object1 = NewJavaObject();
  default_arena_ref->reset(env(), Object(java_object1));
  jobject java_object2 = NewJavaObject();
  arena_ref_referring_to_null->reset(env(), Object(java_object2));
  jobject java_object3 = NewJavaObject();
  arena_ref_referring_to_non_null->reset(env(), Object(java_object3));

  EXPECT_THAT(default_arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object));
  EXPECT_THAT(*default_arena_ref, RefersToJavaObject(java_object1));
  EXPECT_THAT(*arena_ref_referring_to_null, RefersToJavaObject(java_object2));
  EXPECT_THAT(*arena_ref_referring_to_non_null,
              RefersToJavaObject(java_object3));

  default_arena_ref.reset();
  arena_ref_referring_to_null.reset();
  arena_ref_referring_to_non_null.reset();

  EXPECT_THAT(default_arena_ref_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null_copy_dest, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object));
}

TEST_F(
    ArenaRefTest,
    SourceObjectOfCopyAssignmentOperatorShouldBeUnaffectedByChangesToDestObject) {
  FAIL() << "NOT IMPLEMENTED!";
  const ArenaRef default_arena_ref;
  auto default_arena_ref_copy_dest =
      std::make_unique<ArenaRef>(default_arena_ref);
  const ArenaRef arena_ref_referring_to_null(env(), nullptr);
  auto arena_ref_referring_to_null_copy_dest =
      std::make_unique<ArenaRef>(arena_ref_referring_to_null);
  jobject java_object = NewJavaObject();
  const ArenaRef arena_ref_referring_to_non_null(env(), java_object);
  auto arena_ref_referring_to_non_null_copy_dest =
      std::make_unique<ArenaRef>(arena_ref_referring_to_non_null);

  jobject java_object1 = NewJavaObject();
  default_arena_ref_copy_dest->reset(env(), Object(java_object1));
  jobject java_object2 = NewJavaObject();
  arena_ref_referring_to_null_copy_dest->reset(env(), Object(java_object2));
  jobject java_object3 = NewJavaObject();
  arena_ref_referring_to_non_null_copy_dest->reset(env(), Object(java_object3));

  EXPECT_THAT(*default_arena_ref_copy_dest, RefersToJavaObject(java_object1));
  EXPECT_THAT(*arena_ref_referring_to_null_copy_dest,
              RefersToJavaObject(java_object2));
  EXPECT_THAT(*arena_ref_referring_to_non_null_copy_dest,
              RefersToJavaObject(java_object3));
  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));

  default_arena_ref_copy_dest.reset();
  arena_ref_referring_to_null_copy_dest.reset();
  arena_ref_referring_to_non_null_copy_dest.reset();

  EXPECT_THAT(default_arena_ref, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_null, RefersToNullJavaObject());
  EXPECT_THAT(arena_ref_referring_to_non_null, RefersToJavaObject(java_object));
}

}  // namespace
