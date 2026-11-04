// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_APP_SRC_OPTIONAL_H_
#define FIREBASE_APP_SRC_OPTIONAL_H_

#include <optional>
#include <utility>

#include "app/src/assert.h"

namespace firebase {

template <typename T>
class Optional : public std::optional<T> {
 public:
  using std::optional<T>::optional;
  using std::optional<T>::operator=;
  T& value() {
    FIREBASE_ASSERT(this->has_value());
    return **this;
  }
  const T& value() const {
    FIREBASE_ASSERT(this->has_value());
    return **this;
  }
};

template <typename ValueType>
Optional<ValueType> OptionalFromPointer(const ValueType* pointer) {
  return pointer ? Optional<ValueType>(*pointer) : Optional<ValueType>();
}

namespace internal {
using firebase::Optional;
using firebase::OptionalFromPointer;
}  // namespace internal

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_OPTIONAL_H_
