#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_UTIL_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_UTIL_ANDROID_H_

#include <jni.h>
#include <string>
#include "app/src/util_android.h"

#if __cpp_exceptions
#include <exception>
#endif  // __cpp_exceptions

namespace firebase {
namespace firestore {

// By default, google3 disables exception and so does in the release. We enable
// exception only in the test build to test the abort cases. DO NOT enable
// exception for non-test build, which is against the style guide.
#if __cpp_exceptions

class FirestoreException : public std::exception {
 public:
  explicit FirestoreException(const std::string& message) : message_(message) {}

  const char* what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;
};

inline bool CheckAndClearJniExceptions(JNIEnv* env) {
  jobject java_exception = env->ExceptionOccurred();
  if (java_exception == nullptr) {
    return false;
  }

  util::CheckAndClearJniExceptions(env);

  FirestoreException cc_exception{
      util::GetMessageFromException(env, java_exception)};
  env->DeleteLocalRef(java_exception);
  throw cc_exception;
}

#else  // __cpp_exceptions

inline bool CheckAndClearJniExceptions(JNIEnv* env) {
  return util::CheckAndClearJniExceptions(env);
}

#endif  // __cpp_exceptions

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_UTIL_ANDROID_H_
