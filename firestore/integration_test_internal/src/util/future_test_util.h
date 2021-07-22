// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FUTURE_TEST_UTIL_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FUTURE_TEST_UTIL_H_

#include <iosfwd>
#include <string>

#include "firebase/future.h"
#include "gtest/gtest.h"

namespace firebase {

// Prints a human-friendly representation of a Future to an ostream.
// This function is via ADL by Googletest to print a Future object in a test
// failure message. Note that the future type must match exactly, without any
// implicit conversions, for this mechanism to work.
template <typename T>
void PrintTo(const Future<T>& future, std::ostream* os);

// Creates and returns a "matcher" for a Future's success. The matcher will wait
// for the Future to complete with a timeout. If the timeout is reached or the
// Future completes unsuccessfully then the matcher will fail; otherwise, it
// will succeed.
//
// Here is an example of how this function could be used:
// EXPECT_THAT(TestFirestore()->Terminate(), FutureSucceeds());
testing::Matcher<const FutureBase&> FutureSucceeds();

// Converts a `FutureStatus` value to its enumerator name, and returns it. For
// example, if `kFutureStatusComplete` is specified then "kFutureStatusComplete"
// will be returned. If the argument is invalid (i.e. not equal to any element
// from `FutureStatus`) then this function will gracefully return a "name" to
// indicate this.
std::string ToEnumeratorName(FutureStatus status);

// Implementation details follow.

// Because this function's signature requires an implicit conversion to the base
// class, it cannot be found by Googletest -- instead, the template functions
// delegate to it.
void PrintTo(const FutureBase& future, std::ostream* os);

template <typename T>
void PrintTo(const Future<T>& future, std::ostream* os) {
  PrintTo(static_cast<const FutureBase&>(future), os);
}

}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FUTURE_TEST_UTIL_H_
