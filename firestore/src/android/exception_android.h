#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EXCEPTION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EXCEPTION_ANDROID_H_

#include <string>

#if __cpp_exceptions
#include <exception>
#endif  // __cpp_exceptions

#include "app/src/include/firebase/app.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

// By default, google3 disables exceptions and so does the C++ SDK. Exceptions
// are enabled when building for Unity.
#if __cpp_exceptions

class FirestoreException : public std::exception {
 public:
  explicit FirestoreException(const std::string& message, Error code)
      : message_(message), code_(code) {}

  const char* what() const noexcept override { return message_.c_str(); }

  Error code() const { return code_; }

 private:
  std::string message_;
  Error code_;
};

#endif  // __cpp_exceptions

class ExceptionInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static Error GetErrorCode(jni::Env& env, const jni::Object& exception);
  static std::string ToString(jni::Env& env, const jni::Object& exception);

  static jni::Local<jni::Throwable> Create(jni::Env& env, Error code,
                                           const std::string& message);
  static jni::Local<jni::Throwable> Wrap(
      jni::Env& env, jni::Local<jni::Throwable>&& exception);

  /** Returns true if the given object is a FirestoreException. */
  static bool IsFirestoreException(jni::Env& env, const jni::Object& exception);

  /** Returns true if the given object is an IllegalStateException. */
  static bool IsIllegalStateException(jni::Env& env,
                                      const jni::Object& exception);

  /**
   * Returns true if the given object is a FirestoreException or any other type
   * of exception thrown by a Firestore API.
   */
  static bool IsAnyExceptionThrownByFirestore(jni::Env& env,
                                              const jni::Object& exception);
};

void GlobalUnhandledExceptionHandler(jni::Env& env,
                                     jni::Local<jni::Throwable>&& exception,
                                     void* context);
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EXCEPTION_ANDROID_H_
