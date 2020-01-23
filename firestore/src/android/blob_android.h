#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_BLOB_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_BLOB_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"

namespace firebase {
namespace firestore {

class BlobInternal {
 public:
  static jobject BlobToJavaBlob(JNIEnv* env, const uint8_t* value, size_t size);

  static jbyteArray JavaBlobToJbyteArray(JNIEnv* env, jobject obj);

  static jclass GetClass();

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_BLOB_ANDROID_H_
