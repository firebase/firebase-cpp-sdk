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

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

#define STR_EXPAND(x) #x
#define STR(x) STR_EXPAND(x)

#include <stdarg.h>

namespace FIREBASE_NAMESPACE {

const char* kDefaultTag = STR(FIREBASE_NAMESPACE);

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

// Set the platform specific SDK log level.
void LogSetPlatformLevel(LogLevel level) {
  // This isn't available on Android, instead logs go through the framework's
  // android.util.Log.  Some modules, like Analytics and Realtime Database,
  // have their own custom logging which are enabled via system configuration
  // variables or module-specific API calls.
  (void)level;
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

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
