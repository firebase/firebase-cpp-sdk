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

#include "app/src/util_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/long.h"

#include "firestore_integration_test_android.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

using ArenaRefTestAndroid = FirestoreIntegrationTest;

TEST_F(ArenaRefTestAndroid, DefaultConstructorCreatesReferenceToNull) {
  Env env = FirestoreInternal::GetEnv();
  ArenaRef arena_ref;
  EXPECT_EQ(arena_ref.get(env).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, ConstructFromEnvAndObject) {
  Env env = FirestoreInternal::GetEnv();
  Local<String> string = env.NewStringUtf("hello world");

  ArenaRef arena_ref(env, string);
  EXPECT_TRUE(arena_ref.get(env).Equals(env, string));
}

TEST_F(ArenaRefTestAndroid, CopysReferenceToNull) {
  Env env = FirestoreInternal::GetEnv();

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(arena_ref1);
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, CopysReferenceToValidObject) {
  Env env = FirestoreInternal::GetEnv();

  Local<String> string = env.NewStringUtf("hello world");

  ArenaRef arena_ref1(env, string);
  ArenaRef arena_ref2(arena_ref1);

  EXPECT_TRUE(arena_ref1.get(env).Equals(env, string));
  EXPECT_TRUE(arena_ref2.get(env).Equals(env, string));
}

TEST_F(ArenaRefTestAndroid, CopyAssignsReferenceToNull) {
  Env env = FirestoreInternal::GetEnv();

  ArenaRef arena_ref1, arena_ref2;
  arena_ref2 = arena_ref1;
  EXPECT_EQ(arena_ref1.get(env).get(), nullptr);
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, CopyAssignsReferenceToValidObject) {
  Env env = FirestoreInternal::GetEnv();

  Local<String> string1 = env.NewStringUtf("hello world");
  Local<String> string2 = env.NewStringUtf("hello earth");

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(env, string1);
  ArenaRef arena_ref3(env, string2);
  arena_ref3 = arena_ref2;
  arena_ref2 = arena_ref2;

  EXPECT_TRUE(arena_ref3.get(env).Equals(env, string1));
  EXPECT_TRUE(arena_ref2.get(env).Equals(env, string1));

  arena_ref2 = arena_ref1;
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
  EXPECT_TRUE(arena_ref3.get(env).Equals(env, string1));
}

TEST_F(ArenaRefTestAndroid, MovesReferenceToNull) {
  Env env = FirestoreInternal::GetEnv();

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(std::move(arena_ref1));
  EXPECT_EQ(arena_ref1.get(env).get(), nullptr);
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, MovesReferenceToValidObject) {
  Env env = FirestoreInternal::GetEnv();

  Local<String> string = env.NewStringUtf("hello world");

  ArenaRef arena_ref2(env, string);
  ArenaRef arena_ref3(std::move(arena_ref2));
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
  EXPECT_TRUE(arena_ref3.get(env).Equals(env, string));
}

TEST_F(ArenaRefTestAndroid, MoveAssignsReferenceToNull) {
  Env env = FirestoreInternal::GetEnv();

  ArenaRef arena_ref1, arena_ref2;
  arena_ref2 = std::move(arena_ref1);
  EXPECT_EQ(arena_ref1.get(env).get(), nullptr);
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
}

TEST_F(ArenaRefTestAndroid, MoveAssignsReferenceToValidObject) {
  Env env = FirestoreInternal::GetEnv();

  Local<String> string1 = env.NewStringUtf("hello world");
  Local<String> string2 = env.NewStringUtf("hello earth");

  ArenaRef arena_ref1;
  ArenaRef arena_ref2(env, string1);
  arena_ref2 = std::move(arena_ref2);
  EXPECT_TRUE(arena_ref2.get(env).Equals(env, string1));

  ArenaRef arena_ref3(env, string2);
  arena_ref3 = std::move(arena_ref2);
  EXPECT_EQ(arena_ref2.get(env).get(), nullptr);
  EXPECT_TRUE(arena_ref3.get(env).Equals(env, string1));

  arena_ref3 = std::move(arena_ref1);
  EXPECT_EQ(arena_ref3.get(env).get(), nullptr);
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
