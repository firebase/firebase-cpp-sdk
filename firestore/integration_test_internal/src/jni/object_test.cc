// Copyright 2021 Google LLC

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
