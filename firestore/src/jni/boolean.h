#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_BOOLEAN_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_BOOLEAN_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `Boolean`. */
class Boolean : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  static Class GetClass();

  static Local<Boolean> Create(Env& env, bool value);

  bool BooleanValue(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_BOOLEAN_H_
