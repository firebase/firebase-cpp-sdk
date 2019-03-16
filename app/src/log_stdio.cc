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

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Initializes the logging module.
void LogInitialize() {}

// Log a firebase message.
void LogMessageV(LogLevel log_level, const char* format, va_list args) {
  switch (log_level) {
    case kLogLevelVerbose:
      printf("VERBOSE: ");
      break;
    case kLogLevelDebug:
      printf("DEBUG: ");
      break;
    case kLogLevelInfo:
      break;
    case kLogLevelWarning:
      printf("WARNING: ");
      break;
    case kLogLevelError:
      printf("ERROR: ");
      break;
    case kLogLevelAssert:
      printf("ASSERT: ");
      break;
  }
  vprintf(format, args);
  printf("\n");
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
