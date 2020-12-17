#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TIMESTAMP_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TIMESTAMP_ANDROID_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace firestore {

/** A C++ proxy for a Java `Timestamp`. */
class TimestampInternal : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  static jni::Class GetClass();

  /** Creates a C++ proxy for a Java `Timestamp` object. */
  static jni::Local<TimestampInternal> Create(jni::Env& env,
                                              const Timestamp& timestamp);

  /** Converts a Java `Timestamp` into a public C++ `Timestamp`. */
  Timestamp ToPublic(jni::Env& env) const;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TIMESTAMP_ANDROID_H_
