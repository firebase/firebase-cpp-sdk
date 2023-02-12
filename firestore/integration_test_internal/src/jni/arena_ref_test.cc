/*
 * Copyright 2023 Google LLC
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
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/common/make_unique.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/long.h"

#include "firestore_integration_test.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kException[] = "java/lang/Exception";
Method<String> kConstructor("<init>", "(Ljava/lang/String;)V");

class ArenaRefTestAndroid : public FirestoreIntegrationTest {
 public:
  ArenaRefTestAndroid() : loader_(app()), env_(make_unique<Env>(GetEnv())) {
    loader_.LoadClass(kException);
    loader_.Load(kConstructor);
  }

  ~ArenaRefTestAndroid() override {
    // Ensure that after the test is done that any pending exception is cleared
    // to prevent spurious errors in the teardown of FirestoreIntegrationTest.
    env_->ExceptionClear();
  }

  Env& env() const { return *env_; }

  void throwException() {
    Local<Class> clazz = env().FindClass(kException);
    jmethodID ctor =
        env().GetMethodId(clazz, "<init>", "(Ljava/lang/String;)V");

    Local<String> message = env().NewStringUtf("Testing throw");
    Local<Throwable> exception = env().New<Throwable>(clazz, ctor, message);
    // After throwing, use EXPECT rather than ASSERT to ensure that the
    // exception is cleared.
    env().Throw(exception);
    EXPECT_FALSE(env().ok());
  }

  void clearExceptionOccurred() {
    Local<Throwable> thrown = env().ClearExceptionOccurred();
    EXPECT_EQ(thrown.GetMessage(env()), "Testing throw");
  }

 private:
  // Env is declared as having a `noexcept(false)` destructor, which causes the
  // compiler to propagate this into `EnvTest`'s destructor, but this conflicts
  // with the declaration of the parent class. Holding `Env` with a unique
  // pointer sidesteps this restriction.
  std::unique_ptr<Env> env_;
  Loader loader_;
};

TEST_F(ArenaRefTestAndroid, DefaultConstructor) {
  ArenaRef arena_ref;
  EXPECT_EQ(arena_ref.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, ConstructsFromNull) {
  Local<String> string;
  ArenaRef arena_ref(env(), string);
  EXPECT_EQ(arena_ref.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, ConstructsFromValid) {
  Local<String> string = env().NewStringUtf("hello world");
  ArenaRef arena_ref(env(), string);
  EXPECT_TRUE(env().IsSameObject(arena_ref.get(env()), string));
}

TEST_F(ArenaRefTestAndroid, CopyConstructsFromNull) {
  ArenaRef arena_ref1;
  ArenaRef arena_ref2(arena_ref1);
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, CopyConstructsFromValid) {
  Local<String> string = env().NewStringUtf("hello world");

  ArenaRef arena_ref1(env(), string);
  ArenaRef arena_ref2(arena_ref1);
  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string));
  EXPECT_TRUE(env().IsSameObject(arena_ref2.get(env()), string));
}

TEST_F(ArenaRefTestAndroid, CopyAssignsFromNullToNull) {
  ArenaRef arena_ref1, arena_ref2;
  arena_ref2 = arena_ref1;
  EXPECT_EQ(arena_ref1.get(env()).get(), nullptr);
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, CopyAssignsFromNullToValid) {
  Local<String> string = env().NewStringUtf("hello world");

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(env(), string);
  arena_ref2 = arena_ref1;
  EXPECT_EQ(arena_ref1.get(env()).get(), nullptr);
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, CopyAssignsFromValidToNull) {
  Local<String> string = env().NewStringUtf("hello world");

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(env(), string);
  arena_ref1 = arena_ref2;
  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string));
  EXPECT_TRUE(env().IsSameObject(arena_ref2.get(env()), string));
}

TEST_F(ArenaRefTestAndroid, CopyAssignsFromValidToValid) {
  Local<String> string1 = env().NewStringUtf("hello world");
  Local<String> string2 = env().NewStringUtf("hello earth");

  ArenaRef arena_ref1(env(), string1);
  ArenaRef arena_ref2(env(), string2);
  arena_ref1 = arena_ref2;

  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string2));
  EXPECT_TRUE(env().IsSameObject(arena_ref2.get(env()), string2));
}

TEST_F(ArenaRefTestAndroid, CopyAssignsFromNullObjectItself) {
  ArenaRef arena_ref1;
  arena_ref1 = arena_ref1;
  EXPECT_EQ(arena_ref1.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, CopyAssignsFromValidObjectItself) {
  Local<String> string1 = env().NewStringUtf("hello world");

  ArenaRef arena_ref1(env(), string1);
  arena_ref1 = arena_ref1;
  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string1));
}

TEST_F(ArenaRefTestAndroid, MoveConstructsFromNull) {
  ArenaRef arena_ref1;
  ArenaRef arena_ref2(std::move(arena_ref1));
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, MoveConstructsFromValid) {
  Local<String> string = env().NewStringUtf("hello world");

  ArenaRef arena_ref2(env(), string);
  ArenaRef arena_ref3(std::move(arena_ref2));
  EXPECT_TRUE(env().IsSameObject(arena_ref3.get(env()), string));
}

TEST_F(ArenaRefTestAndroid, MoveAssignsFromNullToNull) {
  ArenaRef arena_ref1, arena_ref2;
  arena_ref2 = std::move(arena_ref1);
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, MoveAssignsFromNullToValid) {
  Local<String> string = env().NewStringUtf("hello world");

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(env(), string);
  arena_ref2 = std::move(arena_ref1);
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, MoveAssignsFromValidToNull) {
  Local<String> string = env().NewStringUtf("hello world");

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(env(), string);
  arena_ref1 = std::move(arena_ref2);
  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string));
}

TEST_F(ArenaRefTestAndroid, MoveAssignsFromValidToValid) {
  Local<String> string1 = env().NewStringUtf("hello world");
  Local<String> string2 = env().NewStringUtf("hello earth");

  ArenaRef arena_ref1(env(), string1);
  ArenaRef arena_ref2(env(), string2);
  arena_ref1 = std::move(arena_ref2);
  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string2));
}

TEST_F(ArenaRefTestAndroid, MoveAssignsFromNullObjectItself) {
  ArenaRef arena_ref1;
  arena_ref1 = std::move(arena_ref1);
  EXPECT_EQ(arena_ref1.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, MoveAssignsFromValidObjectItself) {
  Local<String> string1 = env().NewStringUtf("hello world");

  ArenaRef arena_ref1(env(), string1);
  arena_ref1 = std::move(arena_ref1);
  EXPECT_TRUE(env().IsSameObject(arena_ref1.get(env()), string1));
}

TEST_F(ArenaRefTestAndroid, ThrowBeforeConstruct) {
  Local<String> string = env().NewStringUtf("hello world");
  EXPECT_EQ(string.ToString(env()).size(), 11U);
  throwException();
  ArenaRef arena_ref(env(), string);
  clearExceptionOccurred();
  EXPECT_EQ(arena_ref.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, ThrowBeforeCopyConstruct) {
  Local<String> string = env().NewStringUtf("hello world");
  ArenaRef arena_ref1(env(), string);
  EXPECT_EQ(arena_ref1.get(env()).ToString(env()).size(), 11U);
  throwException();
  ArenaRef arena_ref2(arena_ref1);
  clearExceptionOccurred();
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, ThrowBeforeCopyAssignment) {
  Local<String> string = env().NewStringUtf("hello world");
  ArenaRef arena_ref1(env(), string);
  ArenaRef arena_ref2;
  EXPECT_EQ(arena_ref1.get(env()).ToString(env()).size(), 11U);
  throwException();
  arena_ref2 = arena_ref1;
  clearExceptionOccurred();
  EXPECT_EQ(arena_ref2.get(env()).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, ThrowBeforeMoveConstruct) {
  Local<String> string = env().NewStringUtf("hello world");
  ArenaRef arena_ref1(env(), string);
  EXPECT_EQ(arena_ref1.get(env()).ToString(env()).size(), 11U);
  throwException();
  ArenaRef arena_ref2(std::move(arena_ref1));
  clearExceptionOccurred();
  EXPECT_TRUE(env().IsSameObject(arena_ref2.get(env()), string));
}

TEST_F(ArenaRefTestAndroid, ThrowBeforeMoveAssignment) {
  Local<String> string = env().NewStringUtf("hello world");
  ArenaRef arena_ref1(env(), string);
  ArenaRef arena_ref2;
  EXPECT_EQ(arena_ref1.get(env()).ToString(env()).size(), 11U);
  throwException();
  arena_ref2 = std::move(arena_ref1);
  clearExceptionOccurred();
  EXPECT_TRUE(env().IsSameObject(arena_ref2.get(env()), string));
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
