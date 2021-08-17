/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/android/exception_android.h"

#include <stdexcept>

#include "firestore/src/android/firestore_exceptions_android.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/throwable.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticMethod;
using jni::String;
using jni::Throwable;

// FirebaseFirestoreException
constexpr char kFirestoreExceptionClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreException";

Constructor<Throwable> kNewFirestoreException(
    "(Ljava/lang/String;"
    "Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;)V");
Method<Object> kGetCode(
    "getCode",
    "()Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;");

jclass g_firestore_exception_class = nullptr;

// FirebaseFirestoreException$Code
constexpr char kCodeClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreException$Code";

Method<int32_t> kValue("value", "()I");
StaticMethod<Object> kFromValue(
    "fromValue",
    "(I)Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;");

jclass g_illegal_argument_exception_class = nullptr;
jclass g_illegal_state_exception_class = nullptr;

/** Returns true if the given object is an IllegalArgumentException. */
bool IsIllegalArgumentException(jni::Env& env, const jni::Object& exception) {
  return env.IsInstanceOf(exception, g_illegal_argument_exception_class);
}

/** Returns true if the given object is an IllegalStateException. */
bool IsIllegalStateException(jni::Env& env, const jni::Object& exception) {
  return env.IsInstanceOf(exception, g_illegal_state_exception_class);
}

}  // namespace

/* static */
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
  if (!exception) {
    return Error::kErrorOk;
  }

  if (IsIllegalStateException(env, exception)) {
    // Some of the Precondition failure is thrown as IllegalStateException
    // instead of a FirebaseFirestoreException. Convert those into a more
    // meaningful code.
    return Error::kErrorFailedPrecondition;
  } else if (!IsFirestoreException(env, exception)) {
    return Error::kErrorUnknown;
  }

  Local<Object> java_code = env.Call(exception, kGetCode);
  int32_t code = env.Call(java_code, kValue);

  if (code > Error::kErrorUnauthenticated || code < Error::kErrorOk) {
    return Error::kErrorUnknown;
  }
  return static_cast<Error>(code);
}

std::string ExceptionInternal::ToString(Env& env, const Object& exception) {
  return exception.CastTo<Throwable>().GetMessage(env);
}

Local<Throwable> ExceptionInternal::Create(Env& env,
                                           Error code,
                                           const std::string& message) {
  if (code == Error::kErrorOk) {
    return {};
  }

  Local<String> java_message;
  if (message.empty()) {
    // FirebaseFirestoreException requires message to be non-empty. If the
    // caller does not bother to give details, we assign an arbitrary message
    // here.
    java_message = env.NewStringUtf("Unknown Exception");
  } else {
    java_message = env.NewStringUtf(message);
  }

  Local<Object> java_code = env.Call(kFromValue, static_cast<int32_t>(code));
  return env.New(kNewFirestoreException, java_message, java_code);
}

Local<Throwable> ExceptionInternal::Wrap(Env& env,
                                         Local<Throwable>&& exception) {
  if (IsFirestoreException(env, exception)) {
    return Move(exception);
  } else {
    return Create(env, GetErrorCode(env, exception),
                  ToString(env, exception).c_str());
  }
}

bool ExceptionInternal::IsFirestoreException(Env& env,
                                             const Object& exception) {
  return env.IsInstanceOf(exception, g_firestore_exception_class);
}

bool ExceptionInternal::IsAnyExceptionThrownByFirestore(
    Env& env, const Object& exception) {
  return IsFirestoreException(env, exception) ||
         IsIllegalStateException(env, exception);
}

void GlobalUnhandledExceptionHandler(jni::Env& env,
                                     jni::Local<jni::Throwable>&& exception,
                                     void* context) {
#if FIRESTORE_HAVE_EXCEPTIONS
  std::string message = exception.GetMessage(env);

  env.ExceptionClear();
  if (IsIllegalArgumentException(env, exception)) {
    throw std::invalid_argument(message);
  } else if (IsIllegalStateException(env, exception)) {
    throw std::logic_error(message);
  } else if (ExceptionInternal::IsFirestoreException(env, exception)) {
    Error code = ExceptionInternal::GetErrorCode(env, exception);
    throw FirestoreException(message, code);
  } else {
    // All other exceptions are internal errors.
    //
    // This includes NullPointerException which would normally indicate that a
    // user has passed a null argument to a Java method that didn't allow it. In
    // C++, arguments are taken by value or const reference and can't end up as
    // a null Java reference unless there's an error in the argument conversion.
    throw FirestoreException(exception.GetMessage(env), Error::kErrorInternal);
  }

#else
  // Just clear the pending exception. The exception was already logged when
  // first caught.
  env.ExceptionClear();
#endif  // FIRESTORE_HAVE_EXCEPTIONS
}

}  // namespace firestore
}  // namespace firebase
