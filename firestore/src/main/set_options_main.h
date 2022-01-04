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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_SET_OPTIONS_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_SET_OPTIONS_MAIN_H_

#include <unordered_set>
#include <utility>

#include "firestore/src/include/firebase/firestore/set_options.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

/**
 * A wrapper over the public `SetOptions` class that allows getting access to
 * the values stored (the public class only allows setting options, not
 * retrieving them).
 */
class SetOptionsInternal {
 public:
  using Type = SetOptions::Type;

  explicit SetOptionsInternal(SetOptions options)
      : options_{std::move(options)} {}

  Type type() const { return options_.type_; }
  const std::unordered_set<FieldPath>& field_mask() const {
    return options_.fields_;
  }

 private:
  SetOptions options_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_SET_OPTIONS_MAIN_H_
