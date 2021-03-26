#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_MAP_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_MAP_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

class Set;

/** A C++ proxy for a Java `Map`. */
class Map : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  static Class GetClass();

  size_t Size(Env& env) const;
  Local<Object> Get(Env& env, const Object& key);
  Local<Object> Put(Env& env, const Object& key, const Object& value);
  Local<Object> Remove(Env& env, const Object& key);

  Local<Set> KeySet(Env& env);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_MAP_H_
