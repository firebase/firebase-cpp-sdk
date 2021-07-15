// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_EXCEPTION_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_EXCEPTION_ANDROID_H_

#include <string>

#if __cpp_exceptions
#include <exception>
#endif  // __cpp_exceptions

#include "app/src/include/firebase/app.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class ExceptionInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static Error GetErrorCode(jni::Env& env, const jni::Object& exception);
  static std::string ToString(jni::Env& env, const jni::Object& exception);

  static jni::Local<jni::Throwable> Create(jni::Env& env,
                                           Error code,
                                           const std::string& message);
  static jni::Local<jni::Throwable> Wrap(
      jni::Env& env, jni::Local<jni::Throwable>&& exception);

  /** Returns true if the given object is a FirestoreException. */
  static bool IsFirestoreException(jni::Env& env, const jni::Object& exception);

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

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_EXCEPTION_ANDROID_H_
