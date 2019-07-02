// Provide a way for C++ unit test to send test data to fakes. The fakes will
// behave as specified in order to mimic different real world scenarios. See
// goto/firebasecppunittest for more details.
#ifndef FIREBASE_TESTING_CPPSDK_CONFIG_H_
#define FIREBASE_TESTING_CPPSDK_CONFIG_H_

#include <cstdint>
#include "testing/testdata_config_generated.h"

namespace firebase {
namespace testing {
namespace cppsdk {

// Replace the current test data.
void ConfigSet(const char* test_data_in_json);

// Reset (free up) the current test data.
void ConfigReset();

namespace internal {
// Platform-specific function to send the test data.
void ConfigSetImpl(const uint8_t* test_data_binary,
                   flatbuffers::uoffset_t size);
}  // namespace internal

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_CONFIG_H_
