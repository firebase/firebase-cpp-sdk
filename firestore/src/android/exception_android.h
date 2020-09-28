#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EXCEPTION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EXCEPTION_ANDROID_H_

#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

class ExceptionInternal {
 public:
  static Error GetErrorCode(JNIEnv* env, jobject exception);
  static std::string ToString(JNIEnv* env, jobject exception);

  static jthrowable Create(JNIEnv* env, Error code, const char* message);
  static jthrowable Wrap(JNIEnv* env, jthrowable exception);

  /** Returns true if the given object is a FirestoreException. */
  static bool IsFirestoreException(JNIEnv* env, jobject exception);

  /**
   * Returns true if the given object is a FirestoreException or any other type
   * of exception thrown by a Firestore API.
   */
  static bool IsAnyExceptionThrownByFirestore(JNIEnv* env, jobject exception);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EXCEPTION_ANDROID_H_
