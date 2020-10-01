#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ITERATOR_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ITERATOR_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `Iterator`. */
class Iterator : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  bool HasNext(Env& env) const;
  Local<Object> Next(Env& env);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ITERATOR_H_
