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

#include "testing/config_ios.h"
#include "testing/config.h"

#include <cstdint>

#include "testing/testdata_config_generated.h"
#include "testing/testdata_config_resource.h"
#include "flatbuffers/idl.h"

#import <Foundation/Foundation.h>

namespace firebase {
namespace testing {
namespace cppsdk {

// We keep this raw data in order to be able to return it back for merge.
static uint8_t* g_test_data_config = nullptr;

// Replace the current test data.
void ConfigSet(const char* test_data_in_json) {
  flatbuffers::Parser parser;
  const char* schema_str = reinterpret_cast<const char*>(testdata_config_resource_data);
  NSCAssert(parser.Parse(schema_str), @"Failed to parse schema:%s", parser.error_.c_str());
  NSCAssert(parser.Parse(test_data_in_json), @"Invalid JSON:%s", parser.error_.c_str());

  // Assign
  internal::ConfigSetImpl(parser.builder_.GetBufferPointer(),
                          parser.builder_.GetSize());
}

void ConfigReset() { internal::ConfigSetImpl(nullptr, 0); }

const ConfigRow* ConfigGet(const char* fake) {
  NSCAssert(g_test_data_config != nullptr, @"No test data at all");
  const TestDataConfig* config = GetTestDataConfig(g_test_data_config);
  // LookupByKey() does not work because the data passed in may not conform. So
  // we just iterate over the test data.
  if (config->config()) {
    for (const ConfigRow* row : *(config->config())) {
      if (row->fake() && strcmp(row->fake()->c_str(), fake) == 0) {
        return row;
      }
    }
  }
  return nullptr;
}

namespace internal {
void ConfigSetImpl(const uint8_t* test_data_binary,
                   flatbuffers::uoffset_t size) {
  delete[] g_test_data_config;
  if (test_data_binary != nullptr && size > 0) {
    g_test_data_config = new uint8_t[size];
    memcpy(g_test_data_config, test_data_binary, size);
  } else {
    g_test_data_config = nullptr;
  }
}
}  // namespace internal

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
