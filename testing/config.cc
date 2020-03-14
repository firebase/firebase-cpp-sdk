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

#include "testing/config.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/testdata_config_resource.h"
#include "flatbuffers/idl.h"


namespace firebase {
namespace testing {
namespace cppsdk {

const char* kSchemaFilePath = "";

// Replace the current test data.
void ConfigSet(const char* test_data_in_json) {
  flatbuffers::Parser parser;
  const char* schema_str =
      reinterpret_cast<const char*>(testdata_config_resource_data);
  ASSERT_TRUE(parser.Parse(schema_str))
      << "Failed to parse schema:" << parser.error_;
  ASSERT_TRUE(parser.Parse(test_data_in_json))
      << "Invalid JSON:" << parser.error_;

  // Assign
  internal::ConfigSetImpl(parser.builder_.GetBufferPointer(),
                          parser.builder_.GetSize());
}

void ConfigReset() { internal::ConfigSetImpl(nullptr, 0); }

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
