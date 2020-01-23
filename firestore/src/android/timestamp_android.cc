#include "firestore/src/android/timestamp_android.h"

#include <stdint.h>

#include "app/src/util_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define TIMESTAMP_METHODS(X)                                     \
  X(Constructor, "<init>", "(JI)V", util::kMethodTypeInstance),  \
  X(GetSeconds, "getSeconds", "()J"),                            \
  X(GetNanoseconds, "getNanoseconds", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(timestamp, TIMESTAMP_METHODS)
METHOD_LOOKUP_DEFINITION(timestamp,
                         PROGUARD_KEEP_CLASS "com/google/firebase/Timestamp",
                         TIMESTAMP_METHODS)

/* static */
jobject TimestampInternal::TimestampToJavaTimestamp(
    JNIEnv* env, const Timestamp& timestamp) {
  jobject result = env->NewObject(
      timestamp::GetClass(), timestamp::GetMethodId(timestamp::kConstructor),
      static_cast<jlong>(timestamp.seconds()),
      static_cast<jint>(timestamp.nanoseconds()));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
Timestamp TimestampInternal::JavaTimestampToTimestamp(JNIEnv* env,
                                                      jobject obj) {
  jlong seconds =
      env->CallLongMethod(obj, timestamp::GetMethodId(timestamp::kGetSeconds));
  jint nanoseconds = env->CallIntMethod(
      obj, timestamp::GetMethodId(timestamp::kGetNanoseconds));
  CheckAndClearJniExceptions(env);
  return Timestamp{static_cast<int64_t>(seconds),
                   static_cast<int32_t>(nanoseconds)};
}

/* static */
jclass TimestampInternal::GetClass() { return timestamp::GetClass(); }

/* static */
bool TimestampInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = timestamp::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void TimestampInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  timestamp::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
