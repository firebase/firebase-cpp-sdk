#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SET_OPTIONS_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SET_OPTIONS_ANDROID_H_

#include "firestore/src/include/firebase/firestore/set_options.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

/**
 * This helper class provides conversion between the C++ type and Java type. In
 * addition, we also need proper initializer and terminator for the Java class
 * cache/uncache.
 */
class SetOptionsInternal {
 public:
  static void Initialize(jni::Loader& loader);

  /** Convert a C++ SetOptions to a Java SetOptions. */
  static jni::Local<jni::Object> Create(jni::Env& env,
                                        const SetOptions& set_options);

  // We do not need to convert Java SetOptions back to C++ SetOptions since
  // there is no public API that returns a SetOptions yet.
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SET_OPTIONS_ANDROID_H_
