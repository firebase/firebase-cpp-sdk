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

#ifndef FIREBASE_APP_SRC_LOG_H_
#define FIREBASE_APP_SRC_LOG_H_

#include <stdarg.h>

#include <chrono>
#include <sstream>
#include <string>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/log.h"

namespace firebase {

extern const LogLevel kDefaultLogLevel;

// Common log methods.

// All messages at or above the specified log level value are displayed.
void SetLogLevel(LogLevel level);
// Get the currently set log level.
LogLevel GetLogLevel();
// Use firebase::SetLogLevel() instead.
FIREBASE_DEPRECATED void LogSetLevel(LogLevel level);
// Use firebase::GetLogLevel() instead.
FIREBASE_DEPRECATED LogLevel LogGetLevel();
// Set the platform specific SDK log level.
// This is called internally by LogSetLevel().
void LogSetPlatformLevel(LogLevel level);
// Log a debug message to the system log.
void LogDebug(const char* format, ...);
// Log an info message to the system log.
void LogInfo(const char* format, ...);
// Log a warning to the system log.
void LogWarning(const char* format, ...);
// Log an error to the system log.
void LogError(const char* format, ...);
// Log an assert and stop the application.
void LogAssert(const char* format, ...);
// Log a firebase message (implemented by the platform specific logger).
void LogMessageV(LogLevel log_level, const char* format, va_list args);
// Log a firebase message via LogMessageWithCallbackV().
void LogMessage(LogLevel log_level, const char* format, ...);
// Log a firebase message through log callback.
void LogMessageWithCallbackV(LogLevel log_level, const char* format,
                             va_list args);

// Callback which can be used to override message logging.
typedef void (*LogCallback)(LogLevel log_level, const char* log_message,
                            void* callback_data);
// Set the log callback.
void LogSetCallback(LogCallback callback, void* callback_data);
// Get the log callback.
LogCallback LogGetCallback(void** callback_data);

// Initializes the logging module (implemented by the platform specific logger).
void LogInitialize();

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

namespace UnityIssue1154TestAppCpp {

std::string FormattedTimestamp();
std::string FormattedElapsedTime(
    std::chrono::time_point<std::chrono::steady_clock> start);

inline void Log0(std::ostringstream& ss) {
  std::string message = ss.str();
  firebase::LogInfo(message.c_str());
}

template <typename T>
void Log0(std::ostringstream& ss, T message) {
  ss << message;
  Log0(ss);
}

template <typename T, typename... U>
void Log0(std::ostringstream& ss, T message, U... rest) {
  ss << message;
  Log0(ss, rest...);
}

template <typename... T>
std::chrono::time_point<std::chrono::steady_clock> Log(T... messages) {
  std::ostringstream ss;
  Log0(ss, messages...);
  return std::chrono::steady_clock::now();
}

template <typename... T>
void Log(std::chrono::time_point<std::chrono::steady_clock> start_time,
         T... messages) {
  std::string suffix = " (elapsed time: ";
  suffix += FormattedElapsedTime(start_time);
  suffix += ")";
  Log(messages..., suffix);
}

}  // namespace UnityIssue1154TestAppCpp

#endif  // FIREBASE_APP_SRC_LOG_H_
