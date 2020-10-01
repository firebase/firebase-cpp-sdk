#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LIST_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LIST_H_

#include "firestore/src/jni/collection.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `List`. */
class List : public Collection {
 public:
  using Collection::Collection;

  static void Initialize(Loader& loader);

  static Class GetClass();

  Local<Object> Get(Env& env, size_t i) const;
  Local<Object> Set(Env& env, size_t i, const Object& object);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LIST_H_
