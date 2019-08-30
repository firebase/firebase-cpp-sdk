/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <jni.h>

#include <string>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"
#include "app/src/util_android.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Called from com.google.firebase.app.internal.cpp.Log.
extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_app_internal_cpp_Log_nativeLog(
    JNIEnv* env, jobject instance, jint priority, jstring tag, jstring msg) {
  std::string ctag = util::JStringToString(env, tag);
  std::string cmsg = util::JStringToString(env, msg);
  static const LogLevel kLogPriorityToLogLevel[] = {
      kLogLevelDebug,    // 0 = undocumented
      kLogLevelDebug,    // 1 = undocumented
      kLogLevelVerbose,  // 2 = android.util.Log.VERBOSE
      kLogLevelDebug,    // 3 = android.util.Log.DEBUG
      kLogLevelInfo,     // 4 = android.util.Log.INFO
      kLogLevelWarning,  // 5 = android.util.Log.WARN
      kLogLevelError,    // 6 = android.util.Log.ERROR
      kLogLevelAssert,   // 7 = android.util.Log.ASSERT
  };
  assert(priority < FIREBASE_ARRAYSIZE(kLogPriorityToLogLevel));
  assert(priority >= 0);
  LogMessage(kLogPriorityToLogLevel[priority], "(%s) %s", ctag.c_str(),
             cmsg.c_str());
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
