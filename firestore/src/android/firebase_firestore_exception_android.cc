#include "firestore/src/android/firebase_firestore_exception_android.h"

#include <cstring>

#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define FIRESTORE_EXCEPTION_METHODS(X)                                      \
  X(Constructor, "<init>",                                                  \
    "(Ljava/lang/String;"                                                   \
    "Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;)V"),   \
  X(GetCode, "getCode",                                                     \
    "()Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;")
// clang-format on

METHOD_LOOKUP_DECLARATION(firestore_exception, FIRESTORE_EXCEPTION_METHODS)
METHOD_LOOKUP_DEFINITION(
    firestore_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreException",
    FIRESTORE_EXCEPTION_METHODS)

// clang-format off
#define FIRESTORE_EXCEPTION_CODE_METHODS(X)                                \
  X(Value, "value", "()I"),                                                \
  X(FromValue, "fromValue",                                                \
    "(I)Lcom/google/firebase/firestore/FirebaseFirestoreException$Code;",  \
    util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(firestore_exception_code,
                          FIRESTORE_EXCEPTION_CODE_METHODS)
METHOD_LOOKUP_DEFINITION(
    firestore_exception_code,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreException$Code",
    FIRESTORE_EXCEPTION_CODE_METHODS)

#define ILLEGAL_STATE_EXCEPTION_METHODS(X) X(Constructor, "<init>", "()V")

METHOD_LOOKUP_DECLARATION(illegal_state_exception,
                          ILLEGAL_STATE_EXCEPTION_METHODS)
METHOD_LOOKUP_DEFINITION(illegal_state_exception,
                         PROGUARD_KEEP_CLASS "java/lang/IllegalStateException",
                         ILLEGAL_STATE_EXCEPTION_METHODS)

/* static */
Error FirebaseFirestoreExceptionInternal::ToErrorCode(JNIEnv* env,
                                                      jobject exception) {
  if (exception == nullptr) {
    return Error::kOk;
  }

  // Some of the Precondition failure is thrown as IllegalStateException instead
  // of a FirebaseFirestoreException. So we convert them into a more meaningful
  // code.
  if (env->IsInstanceOf(exception, illegal_state_exception::GetClass())) {
    return Error::kFailedPrecondition;
  } else if (!IsInstance(env, exception)) {
    return Error::kUnknown;
  }

  jobject code = env->CallObjectMethod(
      exception,
      firestore_exception::GetMethodId(firestore_exception::kGetCode));
  jint code_value = env->CallIntMethod(
      code,
      firestore_exception_code::GetMethodId(firestore_exception_code::kValue));
  env->DeleteLocalRef(code);
  CheckAndClearJniExceptions(env);

  if (code_value > Error::kUnauthenticated || code_value < Error::kOk) {
    return Error::kUnknown;
  }
  return static_cast<Error>(code_value);
}

/* static */
std::string FirebaseFirestoreExceptionInternal::ToString(JNIEnv* env,
                                                         jobject exception) {
  return util::GetMessageFromException(env, exception);
}

/* static */
jthrowable FirebaseFirestoreExceptionInternal::ToException(
    JNIEnv* env, Error code, const char* message) {
  if (code == Error::kOk) {
    return nullptr;
  }
  // FirebaseFirestoreException requires message to be non-empty. If the caller
  // does not bother to give details, we assign an arbitrary message here.
  if (message == nullptr || strlen(message) == 0) {
    message = "Unknown Exception";
  }

  jstring exception_message = env->NewStringUTF(message);
  jobject exception_code =
      env->CallStaticObjectMethod(firestore_exception_code::GetClass(),
                                  firestore_exception_code::GetMethodId(
                                      firestore_exception_code::kFromValue),
                                  static_cast<jint>(code));
  jthrowable result = static_cast<jthrowable>(env->NewObject(
      firestore_exception::GetClass(),
      firestore_exception::GetMethodId(firestore_exception::kConstructor),
      exception_message, exception_code));
  env->DeleteLocalRef(exception_message);
  env->DeleteLocalRef(exception_code);
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jthrowable FirebaseFirestoreExceptionInternal::ToException(
    JNIEnv* env, jthrowable exception) {
  if (IsInstance(env, exception)) {
    return static_cast<jthrowable>(env->NewLocalRef(exception));
  } else {
    return ToException(env, ToErrorCode(env, exception),
                       ToString(env, exception).c_str());
  }
}

/* static */
bool FirebaseFirestoreExceptionInternal::IsInstance(JNIEnv* env,
                                                    jobject exception) {
  return env->IsInstanceOf(exception, firestore_exception::GetClass());
}

/* static */
bool FirebaseFirestoreExceptionInternal::IsFirestoreException(
    JNIEnv* env, jobject exception) {
  return IsInstance(env, exception) ||
         env->IsInstanceOf(exception, illegal_state_exception::GetClass());
}

/* static */
bool FirebaseFirestoreExceptionInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = firestore_exception::CacheMethodIds(env, activity) &&
                firestore_exception_code::CacheMethodIds(env, activity) &&
                illegal_state_exception::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void FirebaseFirestoreExceptionInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  firestore_exception::ReleaseClass(env);
  firestore_exception_code::ReleaseClass(env);
  illegal_state_exception::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
