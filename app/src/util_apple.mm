/*
 * Copyright 2022 Google LLC
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

#include "app/src/util_apple.h"

#import <Foundation/Foundation.h>

namespace firebase {
namespace util {

std::string GetCustomSemaphorePrefix() {
  NSBundle* mainBundle = [NSBundle mainBundle];
  if (mainBundle != nil) {
    NSDictionary<NSString*, id>* dictionary = [mainBundle infoDictionary];
    if (dictionary != nil) {
      NSString* customPrefix = [dictionary valueForKey:@"FirebaseSemaphorePrefix"];
      if (customPrefix != nil) {
        return std::string(customPrefix.UTF8String);
      }
    }
  }
  return std::string();
}

}  // namespace util
}  // namespace firebase
