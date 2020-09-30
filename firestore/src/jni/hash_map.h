#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_HASH_MAP_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_HASH_MAP_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/map.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `HashMap`. */
class HashMap : public Map {
 public:
  using Map::Map;

  static void Initialize(Loader& loader);

  static Local<HashMap> Create(Env& env);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_HASH_MAP_H_
