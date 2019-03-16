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

#ifndef FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_DESKTOP_SERIALIZATION_H_
#define FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_DESKTOP_SERIALIZATION_H_

#include "app/src/include/firebase/variant.h"

namespace firebase {
namespace functions {
namespace internal {

// Wraps the given variants with type information for special types.
firebase::Variant Encode(const firebase::Variant& variant);

// Unwraps the given Variant, stripping the type information.
firebase::Variant Decode(const firebase::Variant& variant);

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_DESKTOP_SERIALIZATION_H_
