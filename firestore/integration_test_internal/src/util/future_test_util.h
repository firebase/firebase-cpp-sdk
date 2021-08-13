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

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FUTURE_TEST_UTIL_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FUTURE_TEST_UTIL_H_

#include <iosfwd>
#include <string>

#include "firebase/future.h"
#include "gtest/gtest.h"

namespace firebase {

// Prints a human-friendly representation of a Future to an ostream.
// This function is found dynamically by googletest to print a Future object
// in a test failure message.
void PrintTo(const Future<void>& future, std::ostream* os);

// Creates and returns a "matcher" for a Future's success. The matcher will wait
// for the Future to complete with a timeout. If the timeout is reached or the
// Future completes unsuccessfully then the matcher will fail; otherwise, it
// will succeed.
//
// Here is an example of how this function could be used:
// EXPECT_THAT(TestFirestore()->Terminate(), FutureSucceeds());
testing::Matcher<const Future<void>&> FutureSucceeds();

// Converts a `FutureStatus` value to its enumerator name, and returns it. For
// example, if `kFutureStatusComplete` is specified then "kFutureStatusComplete"
// will be returned. If the argument is invalid (i.e. not equal to any element
// from `FutureStatus`) then this function will gracefully return a "name" to
// indicate this.
std::string ToEnumeratorName(FutureStatus status);

}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FUTURE_TEST_UTIL_H_
