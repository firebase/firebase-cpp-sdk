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

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_ANDROID_FIRESTORE_INTEGRATION_TEST_ANDROID_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_ANDROID_FIRESTORE_INTEGRATION_TEST_ANDROID_H_

#include <iosfwd>
#include <string>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#include "firestore/src/jni/throwable.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"

namespace firebase {
namespace firestore {

/** Converts a Java object to a descriptive string, suitable for debugging. */
std::string ToDebugString(const jni::Object& object);

namespace jni {

// These `PrintTo()` functions are called by the test framework when generating
// messages about Java objects. They must exist in the `jni` namespace in order
// to be found by the framework.
void PrintTo(const Object& object, std::ostream* os);

template <typename T>
void PrintTo(const Local<T>& object, std::ostream* os) {
  PrintTo(static_cast<const Object&>(object), os);
}

template <typename T>
void PrintTo(const Global<T>& object, std::ostream* os) {
  PrintTo(static_cast<const Object&>(object), os);
}

}  // namespace jni

/**
 * A gmock matcher that compares two Java objects for equality using the Java
 * .equals() method.
 *
 * Example:
 *
 * jni::Env env;
 * jni::Local<jni::String> object1 = env.NewStringUtf("string");
 * jni::Local<jni::String> object2 = env.NewStringUtf("string");
 * EXPECT_THAT(object1, JavaEq(object2));
 */
MATCHER_P(JavaEq,
          object,
          std::string("compares ") + (negation ? "unequal" : "equal") +
              " using .equals() to a " + ToDebugString(object)) {
  jni::Env env;
  jni::ExceptionClearGuard block(env);
  return jni::Object::Equals(env, object, arg);
}

/**
 * A gmock matcher that compares two Java objects for equality using the ==
 * operator; that is, that they both refer to the _same_ Java object.
 *
 * Example:
 *
 * jni::Env env;
 * jni::Local<jni::String> object1 = env.NewStringUtf("string");
 * jni::Local<jni::String> object2 = object1;
 * EXPECT_THAT(object1, RefersToSameJavaObjectAs(object2));
 */
MATCHER_P(RefersToSameJavaObjectAs,
          object,
          std::string("is ") + (negation ? "not " : "") +
              " referring to the same object as " + ToDebugString(object)) {
  jni::Env env;
  jni::ExceptionClearGuard block(env);
  return env.IsSameObject(arg, object);
}

/** Adds Android-specific functionality to `FirestoreIntegrationTest`. */
class FirestoreAndroidIntegrationTest : public FirestoreIntegrationTest {
 public:
  FirestoreAndroidIntegrationTest();

 protected:
  void SetUp() override;
  void TearDown() noexcept(false) override;

  jni::Loader& loader() { return loader_; }

  /** Creates and returns a new Java `Exception` with a default message. */
  static jni::Local<jni::Throwable> CreateException();
  /** Creates and returns a new Java `Exception` with the given message. */
  static jni::Local<jni::Throwable> CreateException(const std::string& message);

  /** Throws a Java `Exception` object with a default message. */
  jni::Local<jni::Throwable> ThrowException();
  /** Throws a Java `Exception` object with the given message. */
  jni::Local<jni::Throwable> ThrowException(const std::string& message);

  // Bring definitions of `Await()` from the superclass into this class so that
  // the definition below *overloads* instead of *hides* them.
  using FirestoreIntegrationTest::Await;

  /** Blocks until the given `Task` has completed or times out. */
  static void Await(const jni::Task& task);

  /** Returns an Env object for the calling thread, creating it if necessary. */
  static jni::Env& env();

 private:
  void FailTestIfPendingException();

  jni::Loader loader_;
  jni::Global<jni::Throwable> last_thrown_exception_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_ANDROID_FIRESTORE_INTEGRATION_TEST_ANDROID_H_
