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

#include "app/src/jni/string.h"
#include <assert.h>
#include <jni.h>

#include <string>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"

namespace firebase {

// Called from com.google.firebase.app.internal.cpp.Log.
extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_app_internal_cpp_Log_nativeLog(
    JNIEnv* env, jobject instance, jint priority, jstring tag, jstring msg) {
  std::string ctag = jni::JStringToString(env, tag);
  std::string cmsg = jni::JStringToString(env, msg);
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
  const int kNumPriorities =
      static_cast<int>(FIREBASE_ARRAYSIZE(kLogPriorityToLogLevel));
  LogLevel level = (priority >= 0 && priority < kNumPriorities)
                       ? kLogPriorityToLogLevel[priority]
                       : kLogLevelDebug;
  LogMessage(level, "(%s) %s", ctag.c_str(), cmsg.c_str());
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
