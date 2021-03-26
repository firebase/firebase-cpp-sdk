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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MATCHERS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MATCHERS_H_

#include <utility>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {

// Check a smart pointer with a raw pointer for equality. Ideally we would just
// do:
//
//   Pointwise(Property(&UniquePtr<T>::get, Eq())),
//
// but Property can't handle tuple matchers.
MATCHER(SmartPtrRawPtrEq, "CheckSmartPtrRawPtrEq") {
  return std::get<0>(arg).get() == std::get<1>(arg);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MATCHERS_H_
