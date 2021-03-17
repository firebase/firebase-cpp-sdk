// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file defines a stand-alone no-op native Android app. The app deals with
// app events and do nothing interesting. This app is the one under Android
// instrumented test.
//
// In addition, this file also contains an Android native function to be called
// in the Android instrumented test directly. The native function will run all
// gtest tests and update information on passed tests.

#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <pthread.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>

// non-google3 code should include "firebase/app.h" instead.
#include "app/src/include/firebase/app.h"
#include "app/src/assert.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/android/firestore_android.h"
// non-google3 code should include "gtest/gtest.h" instead.
#include "gtest/gtest.h"
// For GTEST_FLAG(filter).
#include "third_party/googletest/googletest/src/gtest-internal-inl.h"

#ifndef NATIVE_FUNCTION_NAME
#define NATIVE_FUNCTION_NAME Java_com_google_firebase_test_MyTest_RunAllTest
#endif  // NATIVE_FUNCTION_NAME

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

// Preserve the JNIEnv and activity that are required by the Firestore client.
static JNIEnv* g_env = nullptr;
static jobject g_activity = nullptr;
static int g_stdout_pipe_file[2];
static int g_stderr_pipe_file[2];
static pthread_t g_pipe_stdout_thread;
static pthread_t g_pipe_stderr_thread;

// This implementation is derived from http://github.com/google/fplutil
static struct android_app* g_app_state = nullptr;
static bool g_destroy_requested = false;
static bool g_started = false;
static bool g_restarted = false;
static pthread_mutex_t g_started_mutex;

static void* pipe_thread(void* pipe) {
  int* pipe_file = static_cast<int*>(pipe);
  char buffer[256];
  const char* tag = pipe_file == g_stdout_pipe_file ? "stdout" : "stderr";
  size_t read_size = read(pipe_file[0], buffer, sizeof(buffer) - 1);
  while (read_size > 0) {
    // Remove trailing \n.
    if (buffer[read_size - 1] == '\n') buffer[read_size - 1] = '\0';
    __android_log_print(ANDROID_LOG_INFO, tag, "%s", buffer);
    read_size = read(pipe_file[0], buffer, sizeof(buffer) - 1);
  }
  return nullptr;
}

namespace firebase {
namespace firestore {

App* GetApp(const char* name) {
  if (name == nullptr || std::string{name} == kDefaultAppName) {
    return App::Create(g_env, g_activity);
  } else {
    App* default_app = App::GetInstance();
    FIREBASE_ASSERT_MESSAGE(default_app,
                "Cannot create a named app before the default app");
    return App::Create(default_app->options(), name, g_env, g_activity);
  }
}

App* GetApp() { return GetApp(nullptr); }

// Process events pending on the main thread.
// Returns true when the app receives an event requesting exit.
bool ProcessEvents(int msec) {
  android_poll_source* source = nullptr;
  int events;
  int looperId = ALooper_pollAll(msec, /*outFd=*/nullptr, &events,
                                 reinterpret_cast<void**>(&source));
  if (looperId >= 0 && source) {
    source->process(g_app_state, source);
  }
  return g_destroy_requested | g_restarted;
}

FirestoreInternal* CreateTestFirestoreInternal(App* app) {
  return new FirestoreInternal(app);
}

void InitializeFirestore(Firestore*) {
  // No extra initialization necessary
}

}  // namespace firestore
}  // namespace firebase

extern "C" JNIEXPORT jboolean JNICALL NATIVE_FUNCTION_NAME(JNIEnv* env,
                                                           jobject thiz,
                                                           jobject activity,
                                                           jstring filter) {
  if (env == nullptr || thiz == nullptr || activity == nullptr ||
      filter == nullptr) {
    __android_log_print(ANDROID_LOG_INFO, __func__, "Invalid parameters.");
    return static_cast<jboolean>(false);
  }

  // Preparation work before run all tests.
  if (g_env == nullptr) {
    g_env = env;
  }
  if (g_activity == nullptr) {
    g_activity = activity;
  }
  // Anything that goes into std output will be gone. So we re-direct it into
  // android logcat.
  pipe(g_stdout_pipe_file);
  dup2(g_stdout_pipe_file[1], STDOUT_FILENO);
  if (pthread_create(&g_pipe_stdout_thread, nullptr, pipe_thread,
                     g_stdout_pipe_file) == -1) {
    __android_log_print(ANDROID_LOG_INFO, __func__,
                        "Failed to re-direct stdout to logcat");
  } else {
    __android_log_print(ANDROID_LOG_INFO, __func__, "Dump stdout to logcat");
    pthread_detach(g_pipe_stdout_thread);
  }
  pipe(g_stderr_pipe_file);
  dup2(g_stderr_pipe_file[1], STDERR_FILENO);
  if (pthread_create(&g_pipe_stderr_thread, nullptr, pipe_thread,
                     g_stderr_pipe_file) == -1) {
    __android_log_print(ANDROID_LOG_INFO, __func__,
                        "Failed to re-direct stderr to logcat");
  } else {
    __android_log_print(ANDROID_LOG_INFO, __func__, "Dump stderr to logcat");
    pthread_detach(g_pipe_stderr_thread);
  }
  // Required by gtest. Although this is a no-op. TODO(zxu): potentially pass
  // through the Java test flag and translate the JUnit test flag into an
  // equivalent gtest flag.
  int argc = 1;
  const char* argv[] = {STRINGIFY(NATIVE_FUNCTION_NAME)};
  testing::InitGoogleTest(&argc, const_cast<char**>(argv));
  testing::internal::GTestFlagSaver flag_saver;
  const char* filter_c_str = env->GetStringUTFChars(filter, /*isCopy=*/nullptr);
  testing::GTEST_FLAG(filter) = filter_c_str;
  env->ReleaseStringUTFChars(filter, filter_c_str);

  // Now run all tests.
  __android_log_print(ANDROID_LOG_INFO, __func__, "Start to run test %s",
                      testing::GTEST_FLAG(filter).c_str());
  auto unit_test = testing::UnitTest::GetInstance();
  bool result = unit_test->Run() == 0;
  __android_log_print(
      ANDROID_LOG_INFO, __func__,
      "Tests finished.\n"
      "  passed tests: %d\n"
      "  skipped tests: %d\n"
      "  failed tests: %d\n"
      "  disabled tests: %d\n"
      "  total tests: %d\n",
      unit_test->successful_test_count(), unit_test->skipped_test_count(),
      unit_test->failed_test_count(), unit_test->disabled_test_count(),
      unit_test->total_test_count());
  return static_cast<jboolean>(result);
}

// Handle state changes from via native app glue.
static void OnAppCmd(struct android_app* app, int32_t cmd) {
  g_destroy_requested |= cmd == APP_CMD_DESTROY;
}

// A dummy android_main() that flushes pending events and finishes the activity.
// Modified from the Firebase Game's testapp.
extern "C" void android_main(struct android_app* state) {
  // native_app_glue spawns a new thread, calling android_main() when the
  // activity onStart() or onRestart() methods are called.  This code handles
  // the case where we're re-entering this method on a different thread by
  // signalling the existing thread to exit, waiting for it to complete before
  // reinitializing the application.
  if (g_started) {
    g_restarted = true;
    // Wait for the existing thread to exit.
    pthread_mutex_lock(&g_started_mutex);
    pthread_mutex_unlock(&g_started_mutex);
  } else {
    g_started_mutex = PTHREAD_MUTEX_INITIALIZER;
  }
  pthread_mutex_lock(&g_started_mutex);
  g_started = true;

  // Save native app glue state and setup a callback to track the state.
  g_destroy_requested = false;
  g_app_state = state;
  g_app_state->onAppCmd = OnAppCmd;

  // Wait until the user wants to quit the app.
  __android_log_print(ANDROID_LOG_INFO, __func__,
                      "started. Waiting for events.");
  while (!firebase::firestore::ProcessEvents(1000)) {
  }

  // Finish the activity.
  __android_log_print(ANDROID_LOG_INFO, __func__, "quitting.");
  if (!g_restarted) ANativeActivity_finish(state->activity);

  g_app_state->activity->vm->DetachCurrentThread();
  g_started = false;
  g_restarted = false;
  pthread_mutex_unlock(&g_started_mutex);
}
