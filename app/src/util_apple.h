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

#ifndef FIREBASE_APP_SRC_UTIL_APPLE_H_
#define FIREBASE_APP_SRC_UTIL_APPLE_H_

#include <string>

namespace firebase {
namespace util {

// Attempts to query the custom semaphore prefix from the application's
// Info.plist file. Returns an empty string if a custom semahpore prefix
// wasn't conifgured.
std::string GetSandboxModeSemaphorePrefix();

}  // namespace util
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_UTIL_APPLE_H_
