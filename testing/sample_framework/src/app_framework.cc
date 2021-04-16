// Copyright 2019 Google Inc. All rights reserved.
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

#include "app_framework.h"  // NOLINT

#include <inttypes.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace app_framework {

// Base logging methods, implemented by platform-specific files.
void LogMessage(const char* format, ...);
void LogMessageV(bool filtered, const char* format, va_list list);

enum LogLevel g_log_level = kInfo;

void SetLogLevel(LogLevel log_level) { g_log_level = log_level; }

LogLevel GetLogLevel() { return g_log_level; }

void LogDebug(const char* format, ...) {
  va_list list;
  va_start(list, format);
  std::string format_str("DEBUG: ");
  format_str += format;
  app_framework::LogMessageV(g_log_level > kDebug, format_str.c_str(), list);
  va_end(list);
}

void LogInfo(const char* format, ...) {
  va_list list;
  va_start(list, format);
  std::string format_str("INFO: ");
  format_str += format;
  app_framework::LogMessageV(g_log_level > kInfo, format_str.c_str(), list);
  va_end(list);
}

void LogWarning(const char* format, ...) {
  va_list list;
  va_start(list, format);
  std::string format_str("WARNING: ");
  format_str += format;
  app_framework::LogMessageV(g_log_level > kWarning, format_str.c_str(), list);
  va_end(list);
}

void LogError(const char* format, ...) {
  va_list list;
  va_start(list, format);
  std::string format_str("ERROR: ");
  format_str += format;
  app_framework::LogMessageV(g_log_level > kError, format_str.c_str(), list);
  va_end(list);
}

#if !defined(_WIN32)  // Windows requires its own version of time-handling code.
int64_t GetCurrentTimeInMicroseconds() {
  struct timeval now;
  gettimeofday(&now, nullptr);
  return now.tv_sec * 1000000LL + now.tv_usec;
}
#endif  // !defined(_WIN32)

#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
void ChangeToFileDirectory(const char*) {}
#endif  // defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

#if defined(_WIN32)
#define stat _stat
#endif  // defined(_WIN32)

bool FileExists(const char* file_path) {
  struct stat s;
  return stat(file_path, &s) == 0;
}

}  // namespace app_framework
