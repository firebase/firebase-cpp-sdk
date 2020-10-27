#include "firestore/src/android/exception_android.h"

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

// IllegalStateException
constexpr char kIllegalStateExceptionClassName[] =
    PROGUARD_KEEP_CLASS "java/lang/IllegalStateException";
Constructor<Throwable> kNewIllegalStateException("()V");

jclass g_illegal_state_exception_class = nullptr;

}  // namespace

/* static */
void ExceptionInternal::Initialize(jni::Loader& loader) {
  g_firestore_exception_class = loader.LoadClass(
      kFirestoreExceptionClassName, kNewFirestoreException, kGetCode);

  loader.LoadClass(kCodeClassName, kValue, kFromValue);

  g_illegal_state_exception_class = loader.LoadClass(
      kIllegalStateExceptionClassName, kNewIllegalStateException);
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

Local<Throwable> ExceptionInternal::Create(Env& env, Error code,
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

bool ExceptionInternal::IsIllegalStateException(Env& env,
                                                const Object& exception) {
  return env.IsInstanceOf(exception, g_illegal_state_exception_class);
}

bool ExceptionInternal::IsAnyExceptionThrownByFirestore(
    Env& env, const Object& exception) {
  return IsFirestoreException(env, exception) ||
         IsIllegalStateException(env, exception);
}

void GlobalUnhandledExceptionHandler(jni::Env& env,
                                     jni::Local<jni::Throwable>&& exception,
                                     void* context) {
#if __cpp_exceptions
  // TODO(b/149105903): Handle different underlying Java exceptions differently.
  env.ExceptionClear();
  throw FirestoreException(exception.GetMessage(env), Error::kErrorInternal);

#else
  // Just clear the pending exception. The exception was already logged when
  // first caught.
  env.ExceptionClear();
#endif
}

}  // namespace firestore
}  // namespace firebase
