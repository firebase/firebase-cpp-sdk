#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_PATH_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_PATH_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore/field_path.h"

namespace firebase {
namespace firestore {

// This helper class provides conversion between the C++ type and Java type. In
// addition, we also need proper initializer and terminator for the Java class
// cache/uncache.
class FieldPathConverter {
 public:
  using ApiType = FieldPath;

  // Convert a C++ FieldPath to a Java FieldPath.
  static jobject ToJavaObject(JNIEnv* env, const FieldPath& path);

  // We do not need to convert Java FieldPath back to C++ FieldPath since there
  // is no public API that returns a FieldPath yet.

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_PATH_ANDROID_H_
