#ifndef FIREBASE_TESTING_CPPSDK_REPORTER_IMPL_FAKE_H_
#define FIREBASE_TESTING_CPPSDK_REPORTER_IMPL_FAKE_H_

#include "testing/reporter_impl.h"

namespace firebase {
namespace testing {
namespace cppsdk {
namespace fake {

// Stub Fake function for testing.
//
// Just add some report with result and arguments. Then we will check all
// reports in *_test.cc file.
void TestFunction();

}  // namespace fake
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_REPORTER_IMPL_FAKE_H_
