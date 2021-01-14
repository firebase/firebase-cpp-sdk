#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_FIRESTORE_INTEGRATION_TEST_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_FIRESTORE_INTEGRATION_TEST_ANDROID_H_

#include <string>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#include "firestore/src/jni/throwable.h"
#include "firestore/src/tests/firestore_integration_test.h"

namespace firebase {
namespace firestore {

/** Adds Android-specific functionality to `FirestoreIntegrationTest`. */
class FirestoreAndroidIntegrationTest : public FirestoreIntegrationTest {
 public:
  FirestoreAndroidIntegrationTest();

 protected:
  jni::Loader& loader() { return loader_; }

  /** Creates and returns a new Java `Exception` object with a message. */
  jni::Local<jni::Throwable> CreateException(jni::Env& env,
                                             const std::string& message);

  // Bring definitions of `Await()` from the superclass into this class so that
  // the definition below *overloads* instead of *hides* them.
  using FirestoreIntegrationTest::Await;

  /** Blocks until the given `Task` has completed or times out. */
  static void Await(jni::Env& env, const jni::Task& task);

 private:
  jni::Loader loader_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_FIRESTORE_INTEGRATION_TEST_ANDROID_H_
