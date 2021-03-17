#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_FUTURE_TEST_UTIL_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_FUTURE_TEST_UTIL_H_

#include <iosfwd>
#include <string>

#include "app/src/include/firebase/future.h"
#include "third_party/googletest/googletest/include/gtest/gtest-matchers.h"

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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_FUTURE_TEST_UTIL_H_
