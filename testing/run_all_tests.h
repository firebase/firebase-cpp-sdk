// C++ unittest wrapper and helper functions to get JNI environment in test
// cases.

#ifndef FIREBASE_TESTING_CPPSDK_RUN_ALL_TESTS_H_
#define FIREBASE_TESTING_CPPSDK_RUN_ALL_TESTS_H_

#include <jni.h>

namespace firebase {
namespace testing {
namespace cppsdk {

extern JNIEnv* g_android_jni_env;
extern jobject g_android_activity;

inline JNIEnv* GetTestJniEnv() {
  return g_android_jni_env;
}

inline jobject GetTestActivity() {
  return g_android_activity;
}

// Wrapper functions should not be exposed to test-case writer.

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_RUN_ALL_TESTS_H_
