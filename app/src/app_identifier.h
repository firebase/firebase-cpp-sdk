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

#ifndef FIREBASE_APP_SRC_APP_IDENTIFIER_H_
#define FIREBASE_APP_SRC_APP_IDENTIFIER_H_

#include <string>

#include "app/src/include/firebase/app.h"

namespace firebase {
namespace internal {

// Generate a unique identifier from the Firebase app options.
std::string CreateAppIdentifierFromOptions(const AppOptions& options);

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_IDENTIFIER_H_
