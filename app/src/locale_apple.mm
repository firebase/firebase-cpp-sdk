/*
 * Copyright 2019 Google LLC
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

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/locale.h"

#if !FIREBASE_PLATFORM_OSX && !FIREBASE_PLATFORM_IOS && !FIREBASE_PLATFORM_TVOS
#error "This file only supports Apple platforms, use locale.cc for other platforms."
#endif

#import <Foundation/Foundation.h>

namespace firebase {
namespace internal {

std::string GetTimezone() {
  const char* tz = NSTimeZone.localTimeZone.name.UTF8String;
  return tz ? tz : "";
}

std::string GetLocale() {
  const char* loc = [NSLocale currentLocale].localeIdentifier.UTF8String;
  return loc ? loc : "";
}

}  // namespace internal
}  // namespace firebase
