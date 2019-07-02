#include "testing/ticker.h"

#include "testing/run_all_tests.h"
#include "testing/util_android.h"

namespace firebase {
namespace testing {
namespace cppsdk {

void TickerElapse() {
  JNIEnv* android_jni_env = GetTestJniEnv();
  jclass cls = android_jni_env->FindClass(
      "com/google/testing/TickerAndroid");
  android_jni_env->CallStaticVoidMethod(
      cls, android_jni_env->GetStaticMethodID(cls, "elapse", "()V"));
  util::CheckAndClearException(android_jni_env);
  android_jni_env->DeleteLocalRef(cls);
}

void TickerReset() {
  JNIEnv* android_jni_env = GetTestJniEnv();
  jclass cls = android_jni_env->FindClass(
      "com/google/testing/TickerAndroid");
  android_jni_env->CallStaticVoidMethod(
      cls, android_jni_env->GetStaticMethodID(cls, "reset", "()V"));
  util::CheckAndClearException(android_jni_env);
  android_jni_env->DeleteLocalRef(cls);
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
