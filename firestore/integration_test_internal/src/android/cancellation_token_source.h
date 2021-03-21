#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_CANCELLATION_TOKEN_SOURCE_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_CANCELLATION_TOKEN_SOURCE_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

/** A C++ proxy for a Java `CancellationTokenSource` from the Tasks API. */
class CancellationTokenSource : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  /** Creates a C++ proxy for a Java `CancellationTokenSource` object. */
  static jni::Local<CancellationTokenSource> Create(jni::Env& env);

  /**
   * Invokes `getToken()` on the wrapped Java `CancellationTokenSource` object.
   */
  jni::Local<Object> GetToken(jni::Env& env);

  /**
   * Invokes `cancel()` on the wrapped Java `CancellationTokenSource` object.
   */
  void Cancel(jni::Env& env);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_ANDROID_CANCELLATION_TOKEN_SOURCE_H_
