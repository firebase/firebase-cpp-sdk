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

// Use the following Macros definition to control compiling.
//     TEST_CLASS_NAME: The Java class name.
//     INIT_GOOGLE: Whether to initialize Google test or not.
//     OUTPUT_TO_INFO: Whether to dump stdout and stderr through LOG(INFO).
#include "testing/run_all_tests.h"

#include <jni.h>
#include <stdio.h>

#include "base/logging.h"
#include "gtest/gtest.h"

namespace firebase {
namespace testing {
namespace cppsdk {

JNIEnv* g_android_jni_env;
jobject g_android_activity;

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#ifndef TEST_CLASS_NAME
#define TEST_CLASS_NAME FirebaseTestActivity
#endif  // TEST_CLASS_NAME

#define JNI_FUNCTION_NAME2(X) Java_com_google_firebase_test_##X##_##runAllTests
#define JNI_FUNCTION_NAME1(X) JNI_FUNCTION_NAME2(X)
#define JNI_FUNCTION_NAME JNI_FUNCTION_NAME1(TEST_CLASS_NAME)

#define STRINGIFY2(X) #X
#define STRINGIFY1(X) STRINGIFY2(X)
#define JNI_FUNCTION_NAME_STR STRINGIFY1(JNI_FUNCTION_NAME)

extern "C" JNIEXPORT jint JNICALL JNI_FUNCTION_NAME
    // Java_com_google_firebase_test_RobolectrictestWrapperActivity_runAllTests
    (JNIEnv* env, jobject obj, jstring j_log_path, jobject activity) {
  // Initializes the global variables, which is required by test case.
  firebase::testing::cppsdk::g_android_jni_env = env;
  if (activity != nullptr) {
    firebase::testing::cppsdk::g_android_activity = activity;
  } else {
    firebase::testing::cppsdk::g_android_activity = obj;
  }

#ifdef OUTPUT_TO_INFO
  // Redirect test output.
  FILE* fp = nullptr;
  const char* log_path = j_log_path == nullptr
                             ? nullptr
                             : env->GetStringUTFChars(j_log_path, nullptr);
  if (log_path != nullptr) {
    fp = freopen(log_path, "w+", stdout);
    dup2(STDOUT_FILENO, STDERR_FILENO);
  } else {
    LOG(WARNING) << "log path is empty";
  }
#endif  // OUTPUT_TO_INFO

// Run all tests.
#ifdef INIT_GOOGLE
  int argc = 1;
  const char* argv[] = {JNI_FUNCTION_NAME_STR};
  testing::InitGoogleTest(&argc, const_cast<char**>(argv));
#endif  // INIT_GOOGLE
  int result = RUN_ALL_TESTS();

  // Log test summary.
  auto unit_test = testing::UnitTest::GetInstance();
  LOG(INFO) << "Tests finished.";
  LOG(INFO) << "  passed tests: " << unit_test->successful_test_count();
  LOG(INFO) << "  failed tests: " << unit_test->failed_test_count();
  LOG(INFO) << "  disabled tests: " << unit_test->disabled_test_count();
  LOG(INFO) << "  total tests: " << unit_test->total_test_count();
#ifdef OUTPUT_TO_INFO
  if (fp != nullptr) {
    LOG(INFO) << "Native test logs:";
    fseek(fp, 0, SEEK_SET);
    char log[256];
    while (!feof(fp)) {
      if (fgets(log, 256, fp)) {
        log[strlen(log) - 1] = '\0';  // Remove newline.
        LOG(INFO) << log;
      }
    }
    fclose(fp);
  } else {
    LOG(WARNING) << "Native test logs are not dumped";
  }
  if (log_path != nullptr) {
    env->ReleaseStringUTFChars(j_log_path, log_path);
  }
#endif  // OUTPUT_TO_INFO

  // Run test could succeed trivially if the test case is not linked.
  if (unit_test->total_test_count() == 0) {
    LOG(ERROR) << "Looks like the test case isn't linked properly.";
    result = 1;
  }

  return result;
}
