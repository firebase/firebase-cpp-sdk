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
#include <mutex>
#include <condition_variable>
#include <stdio.h>

#include <atomic>
#include <cassert>
#include <cstdlib>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"

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

static std::atomic<LogLevel> g_log_level{kDefaultLogLevel};
LogCallback g_log_callback = DefaultLogCallback;
void* g_log_callback_data = nullptr;

// Returns the mutex which controls access to log state and log file output.
static std::mutex& GetCbMutex() {
  static std::mutex* m = new std::mutex();
  return *m;
}
static std::condition_variable& GetCbCv() {
  static std::condition_variable* cv = new std::condition_variable();
  return *cv;
}
static int g_cb_in_flight = 0;
static thread_local int g_local_in_flight = 0;

static Mutex* GetLogMutex() {
  static Mutex* mutex = new Mutex();
  return mutex;
}

// Forward a log message to LogMessageV().
static void InternalLogMessage(LogLevel log_level, const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageV(log_level, format, list);
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
#define FIREBASE_LOG_FILENAME "firebase.log"
// Wide string version for Windows.
#define FIREBASE_LOG_FILENAME_W L"firebase.log"
// Log a message to a log file.
// Must be called while holding *GetLogMutex().
// Returns true if opening the log file was attempted for the first time and
// failed, so the caller can report the error outside the lock.
static bool LogToFile(LogLevel log_level, const char* format, va_list args) {
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
  bool failed_to_open = false;
  if (!attempted_to_open_log_file) {
    attempted_to_open_log_file = true;
#if FIREBASE_PLATFORM_WINDOWS
    log_file = _wfopen(FIREBASE_LOG_FILENAME_W, L"wt");
#else
    log_file = fopen(FIREBASE_LOG_FILENAME, "wt");
#endif
    if (!log_file) {
      failed_to_open = true;
    }
  }
  if (log_file) {
    const char* prefix =
        (log_level >= kLogLevelVerbose &&
         static_cast<size_t>(log_level) <
             FIREBASE_ARRAYSIZE(kLogLevelToPrefixString))
            ? kLogLevelToPrefixString[log_level]
            : "?";
    fprintf(log_file, "%s: ", prefix);
    vfprintf(log_file, format, args);
    fprintf(log_file, "\n");
    // Since we could crash at some point (possibly why we have logging on),
    // flush to disk.
    fflush(log_file);
  }
  return failed_to_open;
}
#endif  // FIREBASE_LOG_TO_FILE

// Log a firebase message (implemented by the platform specific logger).
void LogMessageWithCallbackV(LogLevel log_level, const char* format,
                             va_list args) {
  if (!format) return;
  LogCallback callback = nullptr;
  void* callback_data = nullptr;
#if FIREBASE_LOG_TO_FILE
  bool failed_to_open_log_file = false;
#endif
  struct InFlightTracker {
    bool active = false;
    ~InFlightTracker() {
      if (active) {
        std::unique_lock<std::mutex> lock(GetCbMutex());
        g_cb_in_flight--;
        g_local_in_flight--;
        GetCbCv().notify_all();
      }
    }
  } tracker;
  {
    MutexLock lock(*GetLogMutex());

    LogInitialize();
#if FIREBASE_LOG_TO_FILE
    va_list log_to_file_args;
    va_copy(log_to_file_args, args);
    failed_to_open_log_file = LogToFile(log_level, format, log_to_file_args);
    va_end(log_to_file_args);
#endif  // FIREBASE_LOG_TO_FILE
    if (log_level < GetLogLevel()
#if FIREBASE_LOG_TO_FILE
        && !failed_to_open_log_file
#endif  // FIREBASE_LOG_TO_FILE
    ) {
      return;
    }
    callback = g_log_callback;
    callback_data = g_log_callback_data;
    if (callback) {
      std::unique_lock<std::mutex> cb_lock(GetCbMutex());
      g_cb_in_flight++;
      g_local_in_flight++;
      tracker.active = true;
    }
  }

  if (callback) {

#if FIREBASE_LOG_TO_FILE
    if (failed_to_open_log_file && kLogLevelError >= GetLogLevel()) {
      callback(kLogLevelError,
               "Unable to open log file " FIREBASE_LOG_FILENAME,
               callback_data);
    }
#endif  // FIREBASE_LOG_TO_FILE
    if (log_level >= GetLogLevel()) {
      char log_buffer[512];
      vsnprintf(log_buffer, sizeof(log_buffer), format, args);
      log_buffer[sizeof(log_buffer) - 1] = '\0';
      callback(log_level, log_buffer, callback_data);
    }
  }
}

void SetLogLevel(LogLevel level) {
  g_log_level.store(level, std::memory_order_relaxed);
  LogSetPlatformLevel(level);
}

LogLevel GetLogLevel() { return g_log_level.load(std::memory_order_relaxed); }

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
  {
    MutexLock lock(*GetLogMutex());
    g_log_callback = callback ? callback : DefaultLogCallback;
    g_log_callback_data = callback_data;
  }
  std::unique_lock<std::mutex> lk(GetCbMutex());
  GetCbCv().wait(lk, []() {
    return g_cb_in_flight == g_local_in_flight;
  });
}

// Get the log callback.
LogCallback LogGetCallback(void** callback_data) {
  FIREBASE_ASSERT_RETURN(DefaultLogCallback, callback_data != nullptr);
  MutexLock lock(*GetLogMutex());
  *callback_data = g_log_callback_data;
  return g_log_callback;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
