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
