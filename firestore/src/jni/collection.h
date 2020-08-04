#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_COLLECTION_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_COLLECTION_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

class Iterator;

/** A C++ proxy for a Java `Collection`. */
class Collection : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  bool Add(Env& env, const Object& object);
  Local<class Iterator> Iterator(Env& env);
  size_t Size(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_COLLECTION_H_
