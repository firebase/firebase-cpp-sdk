#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_TASK_COMPLETION_SOURCE_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_TASK_COMPLETION_SOURCE_H_

#include <string>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/throwable.h"

namespace firebase {
namespace firestore {

/** A C++ proxy for a Java `TaskCompletionSource` from the Tasks API. */
class TaskCompletionSource : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  /** Creates a C++ proxy for a Java `TaskCompletionSource` object. */
  static jni::Local<TaskCompletionSource> Create(
      jni::Env& env, const Object& cancellation_token);

  /**
   * Invokes `getTask()` on the wrapped Java `TaskCompletionSource` object.
   */
  jni::Local<Object> GetTask(jni::Env& env);

  /**
   * Invokes `setException()` on the wrapped Java `TaskCompletionSource` object.
   */
  void SetException(jni::Env& env, const jni::Throwable& result);

  /**
   * Invokes `setResult()` on the wrapped Java `TaskCompletionSource` object.
   */
  void SetResult(jni::Env& env, const Object& result);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_TASK_COMPLETION_SOURCE_H_
