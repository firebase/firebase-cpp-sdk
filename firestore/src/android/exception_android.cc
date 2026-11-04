// Copyright 2020 Google LLC

#include "firestore/src/android/exception_android.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

#include "firestore/src/android/firestore_exceptions_android.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/throwable.h"

namespace firebase::firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticMethod;
using jni::String;
using jni::Throwable;

constexpr char kFirestoreExceptionClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreException";
Constructor<Throwable> kNewFirestoreException(
    "(Ljava/lang/String;"
    "Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;)V");
Method<Object> kGetCode(
    "getCode",
    "()Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;");
jclass g_firestore_exception_class = nullptr;

constexpr char kCodeClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreException$Code";
Method<int32_t> kValue("value", "()I");
StaticMethod<Object> kFromValue(
    "fromValue",
    "(I)Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;");

jclass g_illegal_argument_exception_class = nullptr;
jclass g_illegal_state_exception_class = nullptr;

bool IsIllegalArgumentException(jni::Env& env, const jni::Object& exception) {
  return env.IsInstanceOf(exception, g_illegal_argument_exception_class);
}

bool IsIllegalStateException(jni::Env& env, const jni::Object& exception) {
  return env.IsInstanceOf(exception, g_illegal_state_exception_class);
}

}  // namespace

void ExceptionInternal::Initialize(jni::Loader& loader) {
  g_firestore_exception_class = loader.LoadClass(
      kFirestoreExceptionClassName, kNewFirestoreException, kGetCode);
  loader.LoadClass(kCodeClassName, kValue, kFromValue);
  g_illegal_argument_exception_class =
      loader.LoadClass("java/lang/IllegalArgumentException");
  g_illegal_state_exception_class =
      loader.LoadClass("java/lang/IllegalStateException");
}

Error ExceptionInternal::GetErrorCode(Env& env, const Object& exception) {
  if (!exception) return Error::kErrorOk;
  if (IsIllegalArgumentException(env, exception)) {
    return Error::kErrorInvalidArgument;
  }
  if (IsIllegalStateException(env, exception)) {
    return Error::kErrorFailedPrecondition;
  }
  if (!IsFirestoreException(env, exception)) return Error::kErrorUnknown;

  Local<Object> java_code = env.Call(exception, kGetCode);
  if (!env.ok() || !java_code) return Error::kErrorUnknown;
  int32_t code = env.Call(java_code, kValue);
  return (env.ok() && code >= Error::kErrorOk &&
          code <= Error::kErrorUnauthenticated)
             ? static_cast<Error>(code)
             : Error::kErrorUnknown;
}

std::string ExceptionInternal::ToString(Env& env, const Object& exception) {
  return exception ? exception.CastTo<Throwable>().GetMessage(env) : "";
}

Local<Throwable> ExceptionInternal::Create(Env& env, Error code,
                                           const std::string& message) {
  if (code == Error::kErrorOk) return {};
  Local<String> java_message =
      env.NewStringUtf(message.empty() ? "Unknown Exception" : message);
  Local<Object> java_code = env.Call(kFromValue, static_cast<int32_t>(code));
  return env.New(kNewFirestoreException, java_message, java_code);
}

Local<Throwable> ExceptionInternal::Wrap(Env& env,
                                         Local<Throwable>&& exception) {
  return IsFirestoreException(env, exception)
             ? std::move(exception)
             : Create(env, GetErrorCode(env, exception),
                      ToString(env, exception));
}

bool ExceptionInternal::IsFirestoreException(Env& env,
                                             const Object& exception) {
  return env.IsInstanceOf(exception, g_firestore_exception_class);
}

bool ExceptionInternal::IsAnyExceptionThrownByFirestore(
    Env& env, const Object& exception) {
  return IsFirestoreException(env, exception) ||
         IsIllegalArgumentException(env, exception) ||
         IsIllegalStateException(env, exception);
}

void GlobalUnhandledExceptionHandler(jni::Env& env,
                                     jni::Local<jni::Throwable>&& exception,
                                     void*) {
#if FIRESTORE_HAVE_EXCEPTIONS
  std::string message = exception.GetMessage(env);
  env.ExceptionClear();
  if (IsIllegalArgumentException(env, exception)) {
    throw std::invalid_argument(message);
  } else if (IsIllegalStateException(env, exception)) {
    throw std::logic_error(message);
  } else if (ExceptionInternal::IsFirestoreException(env, exception)) {
    throw FirestoreException(message,
                             ExceptionInternal::GetErrorCode(env, exception));
  } else {
    throw FirestoreException(message, Error::kErrorInternal);
  }
#else
  env.ExceptionClear();
#endif
}

}  // namespace firebase::firestore
