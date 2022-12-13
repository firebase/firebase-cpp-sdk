/*
 * Copyright 2022 Google LLC
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
#include "firestore/src/jni/boolean.h"

#include "firestore_integration_test.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

using ArenaRefTest = FirestoreIntegrationTest;

TEST_F(ArenaRefTest, ConstructorTest) {
  Env env(GetEnv());
  auto boxed_ptr = Boolean::Create(env, true);
  ArenaRef ref1(boxed_ptr);
  EXPECT_TRUE(ref1.is_valid());

  ArenaRef ref2(ref1);
  EXPECT_TRUE(ref1.is_valid());
  EXPECT_TRUE(ref2.is_valid());

  ArenaRef ref3(std::move(ref2));
  EXPECT_TRUE(ref3.is_valid());
}

TEST_F(ArenaRefTest, AssignmentOperatorTest) {
  Env env(GetEnv());
  auto boxed_ptr = Boolean::Create(env, true);
  ArenaRef ref1(boxed_ptr);
  EXPECT_TRUE(ref1.is_valid());

  ArenaRef ref2, ref3;

  ref2 = ref1;
  EXPECT_TRUE(ref1.is_valid());
  EXPECT_TRUE(ref2.is_valid());

  ref3 = std::move(ref2);
  EXPECT_TRUE(ref3.is_valid());
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
