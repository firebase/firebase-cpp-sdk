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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <windows.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

#include "app/src/mutex.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Prefix for log messages at each level.
static const char* kLogLevelPrefix[] = {
    "VERBOSE: ",  // kLogLevelVerbose = 0,
    "DEBUG: ",    // kLogLevelDebug,
    "INFO: ",     // kLogLevelInfo,
    "WARNING: ",  // kLogLevelWarning,
    "ERROR: ",    // kLogLevelError,
    "ASSERT: ",   // kLogLevelAssert,
};

#if FIREBASE_PLATFORM_WINDOWS
// Guards the log buffer on Windows.
static Mutex g_log_mutex;  // NOLINT
#endif  // FIREBASE_PLATFORM_WINDOWS

// Initializes the logging module.
void LogInitialize() {}

// Set the platform specific SDK log level.
void LogSetPlatformLevel(LogLevel level) {}

// Log a firebase message.
void LogMessageV(LogLevel log_level, const char* format, va_list args) {
  assert(log_level < FIREBASE_ARRAYSIZE(kLogLevelPrefix));
  const char* prefix = kLogLevelPrefix[log_level];
  printf("%s", prefix);
  vprintf(format, args);
  printf("\n");
  // Platform specific logging.
#if FIREBASE_PLATFORM_WINDOWS
  {
    MutexLock lock(g_log_mutex);
    static char log_buffer[1024];
    size_t prefix_length = strlen(prefix);
    strcpy(log_buffer, prefix);  // // NOLINT
    vsnprintf(log_buffer + prefix_length,
              sizeof(log_buffer) - 1 - prefix_length, format, args);  // NOLINT
    log_buffer[sizeof(log_buffer) - 1] = '\0';
    OutputDebugString(log_buffer);
  }
#endif  // FIREBASE_PLATFORM_WINDOWS
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
