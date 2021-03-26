#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_TYPE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_TYPE_ANDROID_H_

#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

class DocumentChangeTypeInternal : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  DocumentChange::Type GetType(jni::Env& env) const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_TYPE_ANDROID_H_
