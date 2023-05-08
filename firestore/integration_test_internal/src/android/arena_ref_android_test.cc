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
    for (jobject created_java_object : created_java_objects_) {
      env.get()->DeleteGlobalRef(created_java_object);
    }
  }

  jstring NewJavaString(Env& env, const char* contents_modified_utf8) {
    SCOPED_TRACE("NewJavaString");
    JNIEnv* jni_env = env.get();

    jstring java_string_localref = jni_env->NewStringUTF(contents_modified_utf8);
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "NewStringUTF(\"" << contents_modified_utf8 << "\") failed";
      return {};
    }

    jobject java_string_globalref = jni_env->NewGlobalRef(java_string_localref);
    jni_env->DeleteLocalRef(java_string_localref);
    if (jni_env->ExceptionCheck()) {
      jni_env->ExceptionDescribe();
      ADD_FAILURE() << "NewGlobalRef(\"" << contents_modified_utf8 << "\") failed";
      return {};
    }

    created_java_objects_.push_back(java_string_globalref);
    return (jstring)java_string_globalref;
  }

 private:
  std::vector<jobject> created_java_objects_;
};

TEST_F(ArenaRefTest, DefaultConstructorShouldCreateInvalidObject) {
  ArenaRef default_constructed_arena_ref;
  EXPECT_EQ(default_constructed_arena_ref.is_valid(), false);
}

TEST_F(ArenaRefTest, AdoptingConstructorShouldAcceptNull) {
  Env env;

  ArenaRef arena_ref_with_null_object(env, nullptr);

  EXPECT_EQ(arena_ref_with_null_object.get(env).get(), nullptr);
}

TEST_F(ArenaRefTest, AdoptingConstructorShouldAcceptNonNull) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");

  ArenaRef arena_ref_with_non_null_object(env, java_string);

  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_with_non_null_object.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, CopyConstructorShouldCopyInvalidInstance) {
  Env env;
  ArenaRef invalid_arena_ref_copy_src;

  ArenaRef invalid_arena_ref_copy_dest(invalid_arena_ref_copy_src);

  EXPECT_FALSE(invalid_arena_ref_copy_src.is_valid());
  EXPECT_FALSE(invalid_arena_ref_copy_dest.is_valid());
}

TEST_F(ArenaRefTest, CopyConstructorShouldCopyValidInstance) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef arena_ref_copy_src(env, java_string);

  ArenaRef arena_ref_copy_dest(arena_ref_copy_src);

  ASSERT_TRUE(arena_ref_copy_src.is_valid());
  ASSERT_TRUE(arena_ref_copy_dest.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_src.get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, CopyConstructorShouldCreateAnIndependentInstance) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  auto arena_ref_copy_src = std::make_unique<ArenaRef>(env, java_string);

  auto arena_ref_copy_dest1 = std::make_unique<ArenaRef>(*arena_ref_copy_src);
  auto arena_ref_copy_dest2 = std::make_unique<ArenaRef>(*arena_ref_copy_src);

  // Verify that all 3 ArenaRef objects refer to the same Java object.
  ASSERT_TRUE(arena_ref_copy_src->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest1->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_src->get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest1->get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));

  // Delete the original "source" ArenaRef and verify that the remaining two
  // ArenaRef objects still refer to the same Java object.
  arena_ref_copy_src.reset();
  ASSERT_TRUE(arena_ref_copy_dest1->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest1->get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));

  // Delete the first "copy" ArenaRef and verify that the remaining
  // ArenaRef object still refers to the same Java object.
  arena_ref_copy_dest1.reset();
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));
}

TEST_F(ArenaRefTest, MoveConstructorShouldMoveInvalidInstance) {
  Env env;
  ArenaRef invalid_arena_ref_move_src;

  ArenaRef invalid_arena_ref_move_dest(std::move(invalid_arena_ref_move_src));

  EXPECT_FALSE(invalid_arena_ref_move_src.is_valid());
  EXPECT_FALSE(invalid_arena_ref_move_dest.is_valid());
}

TEST_F(ArenaRefTest, MoveConstructorShouldMoveValidInstance) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef arena_ref_move_src(env, java_string);

  ArenaRef arena_ref_move_dest(std::move(arena_ref_move_src));

  ASSERT_FALSE(arena_ref_move_src.is_valid());
  ASSERT_TRUE(arena_ref_move_dest.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, MoveConstructorShouldCreateAnIndependentInstance) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  auto arena_ref_move_src = std::make_unique<ArenaRef>(env, java_string);

  auto arena_ref_move_dest = std::make_unique<ArenaRef>(std::move(*arena_ref_move_src));

  // Delete the moved-from ArenaRef and verify that the moved-to ArenaRef still
  // refers to the same Java object.
  arena_ref_move_src.reset();
  ASSERT_TRUE(arena_ref_move_dest->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest->get(env).get(), java_string));
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldCopyInvalidToInvalid) {
  ArenaRef invalid_arena_ref_copy_src;
  ArenaRef originally_invalid_arena_ref_copy_dest;

  originally_invalid_arena_ref_copy_dest = invalid_arena_ref_copy_src;

  EXPECT_FALSE(invalid_arena_ref_copy_src.is_valid());
  EXPECT_FALSE(originally_invalid_arena_ref_copy_dest.is_valid());
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldCopyValidToInvalid) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef arena_ref_copy_src(env, java_string);
  ArenaRef originally_invalid_arena_ref_copy_dest;

  originally_invalid_arena_ref_copy_dest = arena_ref_copy_src;

  ASSERT_TRUE(arena_ref_copy_src.is_valid());
  ASSERT_TRUE(originally_invalid_arena_ref_copy_dest.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_src.get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(originally_invalid_arena_ref_copy_dest.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldCopyInvalidToValid) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef invalid_arena_ref_copy_src;
  ArenaRef originally_valid_arena_ref_copy_dest(env, java_string);

  originally_valid_arena_ref_copy_dest = invalid_arena_ref_copy_src;

  EXPECT_FALSE(invalid_arena_ref_copy_src.is_valid());
  EXPECT_FALSE(originally_valid_arena_ref_copy_dest.is_valid());
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldCopyValidToValid) {
  Env env;
  jstring java_string_src = NewJavaString(env, "hello world 1");
  jstring java_string_dest = NewJavaString(env, "hello world 2");
  ArenaRef arena_ref_copy_src(env, java_string_src);
  ArenaRef arena_ref_copy_dest(env, java_string_dest);

  arena_ref_copy_dest = arena_ref_copy_src;

  ASSERT_TRUE(arena_ref_copy_src.is_valid());
  ASSERT_TRUE(arena_ref_copy_dest.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_src.get(env).get(), java_string_src));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest.get(env).get(), java_string_src));
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldCopySelfWhenInvalid) {
  ArenaRef arena_ref;

  arena_ref = arena_ref;

  EXPECT_FALSE(arena_ref.is_valid());
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldCopySelfWhenValid) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef arena_ref(env, java_string);

  arena_ref = arena_ref;

  ASSERT_TRUE(arena_ref.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldKeepOriginallyInvalidInstancesIndependent) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  auto invalid_arena_ref_copy_src = std::make_unique<ArenaRef>();
  ArenaRef valid_arena_ref(env, java_string);

  auto arena_ref_copy_dest1 = std::make_unique<ArenaRef>(*invalid_arena_ref_copy_src);
  auto arena_ref_copy_dest2 = std::make_unique<ArenaRef>(*invalid_arena_ref_copy_src);

  // Re-assign the "copy source" ArenaRef to a new value and verify that the
  // copies are unaffected.
  *invalid_arena_ref_copy_src = valid_arena_ref;
  EXPECT_FALSE(arena_ref_copy_dest1->is_valid());
  EXPECT_FALSE(arena_ref_copy_dest2->is_valid());
  ASSERT_TRUE(invalid_arena_ref_copy_src->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(invalid_arena_ref_copy_src->get(env).get(), java_string));

  // Delete the "copy source" ArenaRef and verify that the copies are unaffected.
  invalid_arena_ref_copy_src.reset();
  EXPECT_FALSE(arena_ref_copy_dest1->is_valid());
  EXPECT_FALSE(arena_ref_copy_dest2->is_valid());

  // Re-assign one of the "copy dest" ArenaRef objects and verify that the other
  // copy is unaffected.
  *arena_ref_copy_dest1 = valid_arena_ref;
  EXPECT_FALSE(arena_ref_copy_dest2->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest1->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest1->get(env).get(), java_string));

  // Delete the "copy dest" ArenaRef object that was re-assigned and verify that
  // the other copy is unaffected.
  arena_ref_copy_dest1.reset();
  EXPECT_FALSE(arena_ref_copy_dest2->is_valid());
}

TEST_F(ArenaRefTest, CopyAssignmentOperatorShouldKeepOriginallyValidInstancesIndependent) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  auto arena_ref_copy_src = std::make_unique<ArenaRef>(env, java_string);
  ArenaRef invalid_arena_ref;

  auto arena_ref_copy_dest1 = std::make_unique<ArenaRef>(*arena_ref_copy_src);
  auto arena_ref_copy_dest2 = std::make_unique<ArenaRef>(*arena_ref_copy_src);

  // Re-assign the "copy source" ArenaRef to a new value and verify that the
  // copies are unaffected.
  *arena_ref_copy_src = invalid_arena_ref;
  EXPECT_FALSE(arena_ref_copy_src->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest1->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest1->get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));

  // Delete the "copy source" ArenaRef and verify that the copies are unaffected.
  arena_ref_copy_src.reset();
  ASSERT_TRUE(arena_ref_copy_dest1->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest1->get(env).get(), java_string));
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));

  // Re-assign one of the "copy dest" ArenaRef objects and verify that the other
  // copy is unaffected.
  *arena_ref_copy_dest1 = invalid_arena_ref;
  EXPECT_FALSE(arena_ref_copy_dest1->is_valid());
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));

  // Delete the "copy dest" ArenaRef object that was re-assigned and verify that
  // the other copy is unaffected.
  arena_ref_copy_dest1.reset();
  ASSERT_TRUE(arena_ref_copy_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_copy_dest2->get(env).get(), java_string));
}













TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldMoveInvalidToInvalid) {
  ArenaRef invalid_arena_ref_move_src;
  ArenaRef originally_invalid_arena_ref_move_dest;
  
  originally_invalid_arena_ref_move_dest = std::move(invalid_arena_ref_move_src);

  EXPECT_FALSE(originally_invalid_arena_ref_move_dest.is_valid());
  EXPECT_FALSE(invalid_arena_ref_move_src.is_valid());
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldMoveValidToInvalid) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef arena_ref_move_src(env, java_string);
  ArenaRef originally_invalid_arena_ref_move_dest;

  originally_invalid_arena_ref_move_dest = std::move(arena_ref_move_src);

  ASSERT_TRUE(originally_invalid_arena_ref_move_dest.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(originally_invalid_arena_ref_move_dest.get(env).get(), java_string));
  EXPECT_FALSE(arena_ref_move_src.is_valid());
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldMoveInvalidToValid) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef invalid_arena_ref_move_src;
  ArenaRef originally_valid_arena_ref_move_dest(env, java_string);

  originally_valid_arena_ref_move_dest = std::move(invalid_arena_ref_move_src);

  EXPECT_FALSE(originally_valid_arena_ref_move_dest.is_valid());
  ASSERT_TRUE(invalid_arena_ref_move_src.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(invalid_arena_ref_move_src.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldMoveValidToValid) {
  Env env;
  jstring java_string_src = NewJavaString(env, "hello world 1");
  jstring java_string_dest = NewJavaString(env, "hello world 2");
  ArenaRef arena_ref_move_src(env, java_string_src);
  ArenaRef arena_ref_move_dest(env, java_string_dest);

  arena_ref_move_dest = std::move(arena_ref_move_src);

  ASSERT_TRUE(arena_ref_move_dest.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest.get(env).get(), java_string_src));
  ASSERT_TRUE(arena_ref_move_src.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_src.get(env).get(), java_string_dest));
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldMoveSelfWhenInvalid) {
  ArenaRef arena_ref;

  arena_ref = std::move(arena_ref);

  EXPECT_FALSE(arena_ref.is_valid());
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldMoveSelfWhenValid) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  ArenaRef arena_ref(env, java_string);

  arena_ref = std::move(arena_ref);

  ASSERT_TRUE(arena_ref.is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref.get(env).get(), java_string));
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldKeepOriginallyInvalidInstancesIndependent) {
  Env env;
  jstring java_string = NewJavaString(env, "hello world");
  auto invalid_arena_ref_move_src = std::make_unique<ArenaRef>();
  ArenaRef valid_arena_ref(env, java_string);

  auto arena_ref_move_dest1 = std::make_unique<ArenaRef>(std::move(*invalid_arena_ref_move_src));
  auto arena_ref_move_dest2 = std::make_unique<ArenaRef>(std::move(*arena_ref_move_dest1));

  // Re-assign the "move source" ArenaRef to a new value and verify that the
  // copies are unaffected.
  *invalid_arena_ref_move_src = valid_arena_ref;
  EXPECT_FALSE(arena_ref_move_dest1->is_valid());
  EXPECT_FALSE(arena_ref_move_dest2->is_valid());
  ASSERT_TRUE(invalid_arena_ref_move_src->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(invalid_arena_ref_move_src->get(env).get(), java_string));

  // Delete the "move source" ArenaRef and verify that the copies are unaffected.
  invalid_arena_ref_move_src.reset();
  EXPECT_FALSE(arena_ref_move_dest1->is_valid());
  EXPECT_FALSE(arena_ref_move_dest2->is_valid());

  // Re-assign one of the "move dest" ArenaRef objects and verify that the other
  // move is unaffected.
  *arena_ref_move_dest1 = valid_arena_ref;
  EXPECT_FALSE(arena_ref_move_dest2->is_valid());
  ASSERT_TRUE(arena_ref_move_dest1->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest1->get(env).get(), java_string));

  // Delete the "move dest" ArenaRef object that was re-assigned and verify that
  // the other move is unaffected.
  arena_ref_move_dest1.reset();
  EXPECT_FALSE(arena_ref_move_dest2->is_valid());
}

TEST_F(ArenaRefTest, MoveAssignmentOperatorShouldKeepOriginallyValidInstancesIndependent) {
  Env env;
  jstring java_string1 = NewJavaString(env, "hello world 1");
  jstring java_string2 = NewJavaString(env, "hello world2");
  auto arena_ref_move_src = std::make_unique<ArenaRef>(env, java_string1);
  const ArenaRef invalid_arena_ref;
  const ArenaRef valid_arena_ref(env, java_string2);

  auto arena_ref_move_dest1 = std::make_unique<ArenaRef>(std::move(*arena_ref_move_src));
  auto arena_ref_move_dest2 = std::make_unique<ArenaRef>(std::move(*arena_ref_move_dest1));

  // Re-assign the "move source" ArenaRef to a new value and verify that the
  // copies are unaffected.
  *arena_ref_move_src = invalid_arena_ref;
  EXPECT_FALSE(arena_ref_move_src->is_valid());
  EXPECT_FALSE(arena_ref_move_dest1->is_valid());
  ASSERT_TRUE(arena_ref_move_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest2->get(env).get(), java_string1));

  // Delete the "move source" ArenaRef and verify that the copies are unaffected.
  arena_ref_move_src.reset();
  EXPECT_FALSE(arena_ref_move_dest1->is_valid());
  ASSERT_TRUE(arena_ref_move_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest2->get(env).get(), java_string1));

  // Re-assign one of the "move dest" ArenaRef objects and verify that the other
  // move is unaffected.
  *arena_ref_move_dest1 = valid_arena_ref;
  ASSERT_TRUE(arena_ref_move_dest1->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest1->get(env).get(), java_string2));
  ASSERT_TRUE(arena_ref_move_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest2->get(env).get(), java_string1));

  // Delete the "move dest" ArenaRef object that was re-assigned and verify that
  // the other move is unaffected.
  arena_ref_move_dest1.reset();
  ASSERT_TRUE(arena_ref_move_dest2->is_valid());
  EXPECT_TRUE(env.get()->IsSameObject(arena_ref_move_dest2->get(env).get(), java_string1));
}

}  // namespace
