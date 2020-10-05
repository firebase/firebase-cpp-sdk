#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_INTEGER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_INTEGER_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `Integer`. */
class Integer : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  static Class GetClass();

  static Local<Integer> Create(Env& env, int32_t value);

  int32_t IntValue(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_INTEGER_H_
