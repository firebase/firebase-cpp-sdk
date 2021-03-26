#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_PATH_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_PATH_ANDROID_H_

#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

// This helper class provides conversion between the C++ type and Java type. In
// addition, we also need proper initializer and terminator for the Java class
// cache/uncache.
class FieldPathConverter {
 public:
  static void Initialize(jni::Loader& loader);

  /** Creates a Java FieldPath from  a C++ FieldPath. */
  static jni::Local<jni::Object> Create(jni::Env& env, const FieldPath& path);

  // We do not need to convert Java FieldPath back to C++ FieldPath since there
  // is no public API that returns a FieldPath yet.
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_PATH_ANDROID_H_
