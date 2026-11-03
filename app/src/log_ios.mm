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

#import <Foundation/Foundation.h>

#include <atomic>
#include <vector>
#include <iterator>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#import "FIRConfiguration.h"

#include "app/src/app_ios.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {

// Key used in Info.plist to configure the log level before a load of code is executed.
static NSString* const kLogLevelInfoPlistKey = @"FIRCPPLogLevel";
static std::atomic<bool> g_platform_log_level_set{false};
static std::atomic<bool> g_read_log_level_from_plist{false};

static Mutex& GetIosLogMutex() {
  static Mutex mutex;
  return mutex;
}

// Maps C++ to iOS SDK log levels.
static const FIRLoggerLevel kCppToIOSLogLevel[] = {
    FIRLoggerLevelDebug,  // kLogLevelVerbose = 0,
    // Mapping C++ debug to Obj-C info is intentional.
    // Info messages are intended to be shown only for debugging scenarios.
    FIRLoggerLevelInfo,     // kLogLevelDebug,
    FIRLoggerLevelNotice,   // kLogLevelInfo,
    FIRLoggerLevelWarning,  // kLogLevelWarning,
    FIRLoggerLevelError,    // kLogLevelError,
    FIRLoggerLevelError,    // kLogLevelAssert,
};

// Initialize the logging system.
void LogInitialize() {
  if (g_read_log_level_from_plist.load(std::memory_order_acquire)) return;
  MutexLock lock(GetIosLogMutex());
  if (g_read_log_level_from_plist.load(std::memory_order_relaxed)) return;
  @autoreleasepool {
    if (!g_platform_log_level_set.load(std::memory_order_acquire)) {
      id log_level_obj = [[NSBundle mainBundle]
          objectForInfoDictionaryKey:kLogLevelInfoPlistKey];
      bool has_valid_value = false;
      long log_level_value = 0;
      if ([log_level_obj isKindOfClass:[NSNumber class]]) {
        log_level_value = [log_level_obj integerValue];
        has_valid_value = true;
      } else if ([log_level_obj isKindOfClass:[NSString class]]) {
        const char* str = [(NSString*)log_level_obj UTF8String];
        if (str && str[0] != '\0') {
          char* endptr = nullptr;
          log_level_value = std::strtol(str, &endptr, 10);
          if (endptr != str) {
            while (std::isspace(static_cast<unsigned char>(*endptr))) endptr++;
            if (*endptr == '\0') {
              has_valid_value = true;
            }
          }
        }
      }
      if (has_valid_value && log_level_value >= kLogLevelVerbose &&
          log_level_value <= kLogLevelAssert) {
        SetLogLevel(static_cast<LogLevel>(log_level_value));
      }
    }
    // Synchronize the platform logger with the C++ log level.
    LogSetPlatformLevelLocked(GetLogLevel());
  }
  g_read_log_level_from_plist.store(true, std::memory_order_release);
}

static void LogSetPlatformLevelLocked(LogLevel level) {
  g_platform_log_level_set.store(true, std::memory_order_relaxed);
  const size_t kNumLevels = std::size(kCppToIOSLogLevel);
  size_t index = (level >= kLogLevelVerbose &&
                  static_cast<size_t>(level) < kNumLevels)
                     ? static_cast<size_t>(level)
                     : static_cast<size_t>(kLogLevelInfo);
  firebase::internal::SetFirConfigurationLoggerLevel(kCppToIOSLogLevel[index]);
}

// Set the platform specific SDK log level.
void LogSetPlatformLevel(LogLevel level) {
  MutexLock lock(GetIosLogMutex());
  LogSetPlatformLevelLocked(level);
}

// Log a firebase message.
void LogMessageV(LogLevel log_level, const char* format, va_list args) {
  if (!format) return;
  va_list args_copy;
  va_copy(args_copy, args);
  char message_buffer[1024];
  int result = vsnprintf(message_buffer, sizeof(message_buffer), format, args);
  
  std::vector<char> dynamic_buffer;
  char* final_buffer = message_buffer;
  
  if (result < 0) {
    va_end(args_copy);
    return;
  }
  if (static_cast<size_t>(result) >= sizeof(message_buffer)) {
    dynamic_buffer.resize(result + 1);
    final_buffer = dynamic_buffer.data();
    vsnprintf(final_buffer, result + 1, format, args_copy);
  }
  va_end(args_copy);
  @autoreleasepool {
    NSString* message = @(final_buffer);
    if (!message) {
      message = [[NSString alloc] initWithBytes:final_buffer
                                         length:std::strlen(final_buffer)
                                       encoding:NSISOLatin1StringEncoding];
#if !__has_feature(objc_arc)
      [message autorelease];
#endif
    }
    switch (log_level) {
      case kLogLevelVerbose:
        NSLog(@"VERBOSE: %@", message);
        break;
      case kLogLevelDebug:
        NSLog(@"DEBUG: %@", message);
        break;
      case kLogLevelInfo:
        NSLog(@"%@", message);
        break;
      case kLogLevelWarning:
        NSLog(@"WARNING: %@", message);
        break;
      case kLogLevelError:
        NSLog(@"ERROR: %@", message);
        break;
      case kLogLevelAssert:
        NSLog(@"ASSERT: %@", message);
        break;
      default:
        NSLog(@"UNKNOWN: %@", message);
        break;
    }
  }
}

}  // namespace firebase
