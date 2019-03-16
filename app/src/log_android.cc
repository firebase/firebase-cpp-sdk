/*
 * Copyright 2016 Google LLC
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

#include "app/src/log.h"

#if !defined(FIREBASE_ANDROID_FOR_DESKTOP)
#include <android/log.h>
#endif  // !defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include <assert.h>
#include <jni.h>
#include <stdarg.h>
#include <stdio.h>

#include "app/src/util_android.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

const char* kDefaultTag = "firebase";

// TODO(zxu): Instead of linking on log_stdio, mock the __android_log_vprint
// and use the logic here.
#if !defined(FIREBASE_ANDROID_FOR_DESKTOP)
// Initializes the logging module.
void LogInitialize() {}

// Log a firebase message.
void AndroidLogMessageV(int priority, const char* tag, const char* format,
                        va_list args) {
  __android_log_vprint(priority, tag, format, args);
}

// Log a firebase message.
void LogMessageV(LogLevel log_level, const char* format, va_list args) {
  switch (log_level) {
    case kLogLevelVerbose:
      AndroidLogMessageV(ANDROID_LOG_VERBOSE, kDefaultTag, format, args);
      break;
    case kLogLevelDebug:
      AndroidLogMessageV(ANDROID_LOG_DEBUG, kDefaultTag, format, args);
      break;
    case kLogLevelInfo:
      AndroidLogMessageV(ANDROID_LOG_INFO, kDefaultTag, format, args);
      break;
    case kLogLevelWarning:
      AndroidLogMessageV(ANDROID_LOG_WARN, kDefaultTag, format, args);
      break;
    case kLogLevelError:
      AndroidLogMessageV(ANDROID_LOG_ERROR, kDefaultTag, format, args);
      break;
    case kLogLevelAssert:
      AndroidLogMessageV(ANDROID_LOG_FATAL, kDefaultTag, format, args);
      break;
  }
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

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
  assert(priority <
         sizeof(kLogPriorityToLogLevel) / sizeof(kLogPriorityToLogLevel[0]));
  assert(priority >= 0);
  LogMessage(kLogPriorityToLogLevel[priority], "(%s) %s", ctag.c_str(),
             cmsg.c_str());
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
