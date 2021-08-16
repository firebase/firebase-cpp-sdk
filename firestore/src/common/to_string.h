/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_TO_STRING_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_TO_STRING_H_

#include <iosfwd>
#include <string>

#include "firestore/src/include/firebase/firestore/map_field_value.h"

// This file contains string converters that aren't exposed publicly but are
// used by more than one of the public string converters in their
// implementation.

namespace firebase {
namespace firestore {

// Returns a string representation of the given `MapFieldValue` for
// logging/debugging purposes.
//
// Note: the exact string representation is unspecified and subject to change;
// don't rely on the format of the string.
std::string ToString(const MapFieldValue& value);

// Outputs the string representation of the given `MapFieldValue` to the given
// stream.
//
// See `ToString` above for comments on the representation format.
//
std::ostream& operator<<(std::ostream& out, const MapFieldValue& value);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_TO_STRING_H_
