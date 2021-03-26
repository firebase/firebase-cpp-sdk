// Copyright 2020 Google
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// C++ unittest wrapper and helper functions to get JNI environment in test
// cases.

#ifndef FIREBASE_TESTING_CPPSDK_RUN_ALL_TESTS_H_
#define FIREBASE_TESTING_CPPSDK_RUN_ALL_TESTS_H_

#if defined(__ANDROID__) || defined(FIREBASE_ANDROID_FOR_DESKTOP)

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

#endif  // defined(__ANDROID__) || defined(FIREBASE_ANDROID_FOR_DESKTOP)

#endif  // FIREBASE_TESTING_CPPSDK_RUN_ALL_TESTS_H_
