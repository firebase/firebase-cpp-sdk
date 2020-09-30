#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LONG_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LONG_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `Long`. */
class Long : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  static Class GetClass();

  static Local<Long> Create(Env& env, int64_t value);

  int64_t LongValue(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LONG_H_
