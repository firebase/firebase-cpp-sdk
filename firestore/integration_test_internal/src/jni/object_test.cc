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

#include "firestore/src/jni/object.h"

#include <jni.h>

#include "firestore/src/jni/env.h"
#include "firestore_integration_test.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {

class ObjectTest : public FirestoreIntegrationTest {
 public:
  ObjectTest() : env_(app()->GetJNIEnv()) {}

 protected:
  JNIEnv* env_ = nullptr;
};

TEST_F(ObjectTest, ToString) {
  jclass string_class = env_->FindClass("java/lang/String");
  Object wrapper(string_class);

  Env env(env_);

  // java.lang.Class defines its toString output as having this form.
  EXPECT_EQ("class java.lang.String", wrapper.ToString(env));
  env_->DeleteLocalRef(string_class);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
