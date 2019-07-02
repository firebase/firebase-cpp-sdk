#ifndef FIREBASE_TESTING_CPPSDK_CONFIG_DESKTOP_H_
#define FIREBASE_TESTING_CPPSDK_CONFIG_DESKTOP_H_

#include "testing/testdata_config_generated.h"

namespace firebase {
namespace testing {
namespace cppsdk {

// Returns the test data for the specific fake.
const ConfigRow* ConfigGet(const char* fake);

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_CONFIG_DESKTOP_H_
