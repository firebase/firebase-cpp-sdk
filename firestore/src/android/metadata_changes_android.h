#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_METADATA_CHANGES_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_METADATA_CHANGES_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/include/firebase/firestore/metadata_changes.h"

namespace firebase {
namespace firestore {

class MetadataChangesInternal {
 public:
  using ApiType = MetadataChanges;

  static jobject ToJavaObject(JNIEnv* env, MetadataChanges metadata_changes);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static jobject exclude_;
  static jobject include_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_METADATA_CHANGES_ANDROID_H_
