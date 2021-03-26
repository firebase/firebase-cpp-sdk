#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ARRAY_LIST_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ARRAY_LIST_H_

#include "firestore/src/jni/list.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `ArrayList`. */
class ArrayList : public List {
 public:
  using List::List;

  static void Initialize(Loader& loader);

  static Local<ArrayList> Create(Env& env);
  static Local<ArrayList> Create(Env& env, size_t size);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ARRAY_LIST_H_
