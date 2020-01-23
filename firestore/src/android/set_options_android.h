#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SET_OPTIONS_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SET_OPTIONS_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore/set_options.h"

namespace firebase {
namespace firestore {

/**
 * This helper class provides conversion between the C++ type and Java type. In
 * addition, we also need proper initializer and terminator for the Java class
 * cache/uncache.
 */
class SetOptionsInternal {
 public:
  using ApiType = SetOptions;

  /** Get a jobject for overwrite option. */
  static jobject Overwrite(JNIEnv* env);

  /** Get a jobject for merge all option. */
  static jobject Merge(JNIEnv* env);

  /** Convert a C++ SetOptions to a Java SetOptions. */
  static jobject ToJavaObject(JNIEnv* env, const SetOptions& set_options);

  // We do not need to convert Java SetOptions back to C++ SetOptions since
  // there is no public API that returns a SetOptions yet.

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SET_OPTIONS_ANDROID_H_
