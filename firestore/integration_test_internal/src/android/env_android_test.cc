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
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/jni/long.h"

#include "firestore_integration_test_android.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Method<Object> kGet("get", "(Ljava/lang/Object;)Ljava/lang/Object;");
Method<Object> kPut("put",
                    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

class EnvTestAndroid : public FirestoreAndroidIntegrationTest {
 public:
  void SetUp() override {
    FirestoreAndroidIntegrationTest::SetUp();
    jclass g_clazz = util::map::GetClass();
    loader().LoadFromExistingClass("java/util/HashMap", g_clazz, kGet, kPut);
    ASSERT_TRUE(loader().ok());
  }
};

TEST_F(EnvTestAndroid, EnvCallTakeArenaRefTest) {
  Env env = FirestoreInternal::GetEnv();

  ArenaRef hash_map(env, HashMap::Create(env));
  Local<Long> key = Long::Create(env, 1);
  Local<Long> val = Long::Create(env, 2);
  env.Call(hash_map, kPut, key, val);
  Local<Object> result = env.Call(hash_map, kGet, key);
  EXPECT_TRUE(result.Equals(env, val));
}

TEST_F(EnvTestAndroid, EnvIsInstanceOfTakeArenaRefTest) {
  Env env = FirestoreInternal::GetEnv();

  ArenaRef hash_map(env, HashMap::Create(env));
  EXPECT_TRUE(env.IsInstanceOf(hash_map, HashMap::GetClass()));
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
