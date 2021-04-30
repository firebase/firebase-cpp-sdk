#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_BLOB_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_BLOB_ANDROID_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

class BlobInternal : public jni::Object {
 public:
  using Object::Object;

  static void Initialize(jni::Loader& loader);

  static jni::Class GetClass();

  static jni::Local<BlobInternal> Create(jni::Env& env, const uint8_t* value,
                                         size_t size);

  jni::Local<jni::Array<uint8_t>> ToBytes(jni::Env& env) const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_BLOB_ANDROID_H_
