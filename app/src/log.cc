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

#include <stdarg.h>
#include <stdio.h>

#include <cassert>
#include <cstdlib>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/no_destructor.h"

#if !defined(FIREBASE_LOG_DEBUG)
#define FIREBASE_LOG_DEBUG 0
#endif  // !defined(FIREBASE_LOG_DEBUG)
#if !defined(FIREBASE_LOG_TO_FILE)
#if FIREBASE_LOG_DEBUG
#define FIREBASE_LOG_TO_FILE 1
#else
#define FIREBASE_LOG_TO_FILE 0
#endif  // FIREBASE_LOG_DEBUG
#endif  // !defined(FIREBASE_LOG_TO_FILE)

namespace firebase {

// Default log callback.
static void DefaultLogCallback(LogLevel log_level, const char* message,
                               void* /*callback_data*/);

#if FIREBASE_LOG_DEBUG
const LogLevel kDefaultLogLevel = kLogLevelDebug;
#else
const LogLevel kDefaultLogLevel = kLogLevelInfo;
#endif  // FIREBASE_LOG_DEBUG

LogLevel g_log_level = kDefaultLogLevel;
LogCallback g_log_callback = DefaultLogCallback;
void* g_log_callback_data = nullptr;

namespace {

// Serializes formatting into the shared callback log buffer. Using a
// function-local NoDestructor<Mutex> guarantees thread-safe initialization on
// first log call (even from pre-main static constructors) and keeps the mutex
// alive for exit-time destructors that log warnings or debug messages.
Mutex& LogMutex() {
  static NoDestructor<Mutex> log_mutex;
  return *log_mutex;
}

}  // namespace

// Forward a log message to LogMessageV().
static void InternalLogMessage(LogLevel log_level, const char* message, ...) {
  va_list list;
  va_start(list, message);
  LogMessageV(log_level, "%s", list);
  va_end(list);
}

// Default log callback.  Halts the application after logging assert messages.
static void DefaultLogCallback(LogLevel log_level, const char* message,
                               void* /*callback_data*/) {
  InternalLogMessage(log_level, "%s", message);
  if (log_level == kLogLevelAssert) {
    abort();
  }
}

#if FIREBASE_LOG_TO_FILE
// Log a message to a log file.
static void LogToFile(LogLevel log_level, const char* format, va_list args) {
#define FIREBASE_LOG_FILENAME "firebase.log"
// Wide string version for Windows.
#define FIREBASE_LOG_FILENAME_W L"firebase.log"
  static FILE* log_file = nullptr;
  static bool attempted_to_open_log_file = false;
  static const char* kLogLevelToPrefixString[] = {
      "V",  // kLogLevelVerbose
      "D",  // kLogLevelDebug
      "I",  // kLogLevelInfo
      "W",  // kLogLevelWarning
      "E",  // kLogLevelError
      "A",  // kLogLevelAssert
  };
  if (attempted_to_open_log_file) {
    if (log_file) {
      fprintf(log_file,
              "%s: ", log_level ? kLogLevelToPrefixString[log_level] : "?");
      vfprintf(log_file, format, args);
      fprintf(log_file, "\n");
      // Since we could crash at some point (possibly why we have logging on),
      // flush to disk.
      fflush(log_file);
    }
  } else {
    MutexLock lock(LogMutex());
    if (!log_file) {
#if FIREBASE_PLATFORM_WINDOWS
      log_file = _wfopen(FIREBASE_LOG_FILENAME_W, L"wt");
#else
      log_file = fopen(FIREBASE_LOG_FILENAME, "wt");
#endif
      if (!log_file) {
        g_log_callback(kLogLevelError,
                       "Unable to open log file " FIREBASE_LOG_FILENAME,
                       g_log_callback_data);
      }
      attempted_to_open_log_file = true;
    }
  }
}
#endif  // FIREBASE_LOG_TO_FILE

// Log a firebase message (implemented by the platform specific logger).
void LogMessageWithCallbackV(LogLevel log_level, const char* format,
                             va_list args) {
  MutexLock lock(LogMutex());

  LogInitialize();
#if FIREBASE_LOG_TO_FILE
  va_list log_to_file_args;
  va_copy(log_to_file_args, args);
  LogToFile(log_level, format, log_to_file_args);
#endif  // FIREBASE_LOG_TO_FILE
  if (log_level < GetLogLevel()) return;

  static char log_buffer[512] = {0};
  vsnprintf(log_buffer, sizeof(log_buffer) - 1, format, args);
  g_log_callback(log_level, log_buffer, g_log_callback_data);
}

void SetLogLevel(LogLevel level) {
  g_log_level = level;
  LogSetPlatformLevel(level);
}

LogLevel GetLogLevel() { return g_log_level; }

void LogSetLevel(LogLevel level) { SetLogLevel(level); }

LogLevel LogGetLevel() { return GetLogLevel(); }

// Log a debug message to the system log.
void LogDebug(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageWithCallbackV(kLogLevelDebug, format, list);
  va_end(list);
}

// Log an info message to the system log.
void LogInfo(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageWithCallbackV(kLogLevelInfo, format, list);
  va_end(list);
}

// Log a warning to the system log.
void LogWarning(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageWithCallbackV(kLogLevelWarning, format, list);
  va_end(list);
}

// Log an error message to the system log.
void LogError(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageWithCallbackV(kLogLevelError, format, list);
  va_end(list);
}

// Log an assert message to the system log.
// NOTE: The default callback will stop the application.
void LogAssert(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageWithCallbackV(kLogLevelAssert, format, list);
  va_end(list);
}

// Log a firebase message via LogMessageV().
void LogMessage(LogLevel log_level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  LogMessageWithCallbackV(log_level, format, args);
  va_end(args);
}

// Set the log callback.
void LogSetCallback(LogCallback callback, void* callback_data) {
  g_log_callback = callback ? callback : DefaultLogCallback;
  g_log_callback_data = callback_data;
}

// Get the log callback.
LogCallback LogGetCallback(void** callback_data) {
  FIREBASE_ASSERT(callback_data);
  *callback_data = g_log_callback_data;
  return g_log_callback;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
