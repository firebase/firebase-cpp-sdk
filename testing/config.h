// Copyright 2020 Google
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
