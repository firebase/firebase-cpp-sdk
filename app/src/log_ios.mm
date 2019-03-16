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

#include <stdarg.h>

namespace firebase {

// Key used in Info.plist to configure the log level before a load of code is executed.
static NSString* kLogLevelInfoPlistKey = @"FIRCPPLogLevel";

// Initialize the logging system.
void LogInitialize() {
  static bool read_log_level_from_plist = false;
  if (read_log_level_from_plist) return;
  read_log_level_from_plist = true;
  NSNumber* log_level_number = (NSNumber*)(
      [[NSBundle mainBundle] objectForInfoDictionaryKey:kLogLevelInfoPlistKey]);
  if (log_level_number) {
    int log_level_value = log_level_number.intValue;
    if (log_level_value >= kLogLevelVerbose && log_level_value <= kLogLevelAssert) {
      LogSetLevel(static_cast<LogLevel>(log_level_value));
    }
  }
}

// Log a firebase message.
void LogMessageV(LogLevel log_level, const char* format, va_list args) {
  switch (log_level) {
    case kLogLevelVerbose:
      NSLogv([@"VERBOSE: " stringByAppendingString:@(format)], args);
      break;
    case kLogLevelDebug:
      NSLogv([@"DEBUG: " stringByAppendingString:@(format)], args);
      break;
    case kLogLevelInfo:
      NSLogv(@(format), args);
      break;
    case kLogLevelWarning:
      NSLogv([@"WARNING: " stringByAppendingString:@(format)], args);
      break;
    case kLogLevelError:
      NSLogv([@"ERROR: " stringByAppendingString:@(format)], args);
      break;
    case kLogLevelAssert:
      NSLogv([@"ASSERT: " stringByAppendingString:@(format)], args);
      break;
  }
}


}  // namespace firebase
