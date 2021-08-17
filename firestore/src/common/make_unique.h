/*
 * Copyright 2021 Google LLC
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

// Note: This is needed since std::make_unique is only available starting C++14.
// TODO(b/191981857): Remove this file when possible.

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_MAKE_UNIQUE_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_MAKE_UNIQUE_H_

#include <memory>
#include <utility>

namespace firebase {
namespace firestore {

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_MAKE_UNIQUE_H_
