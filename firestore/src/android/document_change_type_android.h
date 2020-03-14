#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_TYPE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_TYPE_ANDROID_H_

#include <jni.h>

#include <map>

#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore/document_change.h"

namespace firebase {
namespace firestore {

class DocumentChangeTypeInternal {
 public:
  static DocumentChange::Type JavaDocumentChangeTypeToDocumentChangeType(
      JNIEnv* env, jobject type);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static std::map<DocumentChange::Type, jobject>* cpp_enum_to_java_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_TYPE_ANDROID_H_
