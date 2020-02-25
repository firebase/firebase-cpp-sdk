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

#ifndef FIREBASE_TESTING_CPPSDK_UTIL_ANDROID_H_
#define FIREBASE_TESTING_CPPSDK_UTIL_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

namespace firebase {
namespace testing {
namespace cppsdk {
namespace util {

// Check for JNI exceptions, print them to the log (if any were raised) and
// clear the exception state returning whether an exception was raised.
bool CheckAndClearException(JNIEnv *env);

// Converts a `java.util.List<String>` to a `std::vector<std::string>`.
std::vector<std::string> JavaStringListToStdStringVector(JNIEnv* env,
                                                         jobject list);

// Convert a `jstring` to a `std::string`.
std::string JavaStringToStdString(JNIEnv* env, jobject string_object);

}  // namespace util
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_UTIL_ANDROID_H_
