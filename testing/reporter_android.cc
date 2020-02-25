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

#include <jni.h>
#include <string>
#include <vector>

#include "testing/reporter.h"
#include "testing/run_all_tests.h"
#include "testing/util_android.h"

namespace firebase {
namespace testing {
namespace cppsdk {

void Reporter::reset() {
  expectations_.clear();
  JNIEnv* env = GetTestJniEnv();
  jclass cls =
      env->FindClass("com/google/testing/FakeReporter");
  env->CallStaticVoidMethod(cls, env->GetStaticMethodID(cls, "reset", "()V"));
  util::CheckAndClearException(env);
  env->DeleteLocalRef(cls);
}

std::vector<std::string> Reporter::getAllFakes() {
  JNIEnv* env = GetTestJniEnv();
  jclass cls =
      env->FindClass("com/google/testing/FakeReporter");
  jobject fake_functions_list = env->CallStaticObjectMethod(
      cls, env->GetStaticMethodID(cls, "getAllFakes", "()Ljava/util/List;"));
  util::CheckAndClearException(env);
  std::vector<std::string> fake_functions =
      util::JavaStringListToStdStringVector(env, fake_functions_list);

  env->DeleteLocalRef(cls);
  env->DeleteLocalRef(fake_functions_list);
  return fake_functions;
}

std::vector<std::string> Reporter::getFakeArgs(const std::string& fake) {
  JNIEnv* env = GetTestJniEnv();
  jstring fake_string = env->NewStringUTF(fake.c_str());
  jclass cls =
      env->FindClass("com/google/testing/FakeReporter");
  jobject fake_args_list = env->CallStaticObjectMethod(
      cls,
      env->GetStaticMethodID(cls, "getFakeArgs",
                             "(Ljava/lang/String;)Ljava/util/List;"),
      fake_string);
  util::CheckAndClearException(env);
  std::vector<std::string> fake_args =
      util::JavaStringListToStdStringVector(env, fake_args_list);

  env->DeleteLocalRef(cls);
  env->DeleteLocalRef(fake_string);
  env->DeleteLocalRef(fake_args_list);
  return fake_args;
}

std::string Reporter::getFakeResult(const std::string& fake) {
  JNIEnv* env = GetTestJniEnv();
  jstring fake_string = env->NewStringUTF(fake.c_str());
  jclass cls =
      env->FindClass("com/google/testing/FakeReporter");
  jobject fake_result_string = env->CallStaticObjectMethod(
      cls,
      env->GetStaticMethodID(cls, "getFakeResult",
                             "(Ljava/lang/String;)Ljava/lang/String;"),
      fake_string);
  util::CheckAndClearException(env);
  std::string return_string =
      util::JavaStringToStdString(env, fake_result_string);

  env->DeleteLocalRef(cls);
  env->DeleteLocalRef(fake_string);
  env->DeleteLocalRef(fake_result_string);
  return return_string;
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
