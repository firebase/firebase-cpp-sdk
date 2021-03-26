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

#include "testing/util_android.h"

#include <jni.h>

#include <string>
#include <vector>

namespace firebase {
namespace testing {
namespace cppsdk {
namespace util {

bool CheckAndClearException(JNIEnv *env) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
  }
  return false;
}

// Converts a `java.util.List<String>` to a `std::vector<std::string>`.
std::vector<std::string> JavaStringListToStdStringVector(JNIEnv* env,
                                                         jobject list) {
  jclass cls = env->FindClass("java/util/List");
  int size = env->CallIntMethod(list, env->GetMethodID(cls, "size", "()I"));
  env->DeleteLocalRef(cls);
  if (CheckAndClearException(env)) size = 0;
  std::vector<std::string> vector(size);
  for (int i = 0; i < size; i++) {
    cls = env->FindClass("java/util/List");
    jobject element =
        env->CallObjectMethod(list,
                              env->GetMethodID(cls, "get",
                                               "(I)Ljava/lang/Object;"),
                              i);
    bool failed = CheckAndClearException(env);
    env->DeleteLocalRef(cls);
    if (failed) break;
    vector[i] = JavaStringToStdString(env, element);
    env->DeleteLocalRef(element);
  }
  return vector;
}

// Convert a `jstring` to a `std::string`.
std::string JavaStringToStdString(JNIEnv* env, jobject string_object) {
  if (string_object == nullptr) return "";
  const char* string_buffer =
      env->GetStringUTFChars(static_cast<jstring>(string_object), nullptr);
  std::string return_string(string_buffer);
  env->ReleaseStringUTFChars(static_cast<jstring>(string_object),
                             string_buffer);
  return return_string;
}

}  // namespace util
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
