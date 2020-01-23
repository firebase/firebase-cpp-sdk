#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIREBASE_FIRESTORE_EXCEPTION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIREBASE_FIRESTORE_EXCEPTION_ANDROID_H_

#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firebase-ios-sdk/Firestore/core/include/firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

class FirebaseFirestoreExceptionInternal {
 public:
  static Error ToErrorCode(JNIEnv* env, jobject exception);
  static std::string ToString(JNIEnv* env, jobject exception);
  static jthrowable ToException(JNIEnv* env, Error code, const char* message);
  static jthrowable ToException(JNIEnv* env, jthrowable exception);
  static bool IsInstance(JNIEnv* env, jobject exception);
  static bool IsFirestoreException(JNIEnv* env, jobject exception);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIREBASE_FIRESTORE_EXCEPTION_ANDROID_H_
