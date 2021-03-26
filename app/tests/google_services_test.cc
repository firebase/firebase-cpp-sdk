/*
 * Copyright 2019 Google LLC
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

#include <string>

#include "app/google_services_resource.h"
#include "app/src/log.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "absl/flags/flag.h"

namespace firebase {
namespace fbs {

// Helper function to parse config and return whether the config is valid.
bool Parse(const char* config) {
  flatbuffers::IDLOptions options;
  options.skip_unexpected_fields_in_json = true;
  flatbuffers::Parser parser(options);

  // Parse schema.
  const char* schema =
      reinterpret_cast<const char*>(google_services_resource_data);
  if (!parser.Parse(schema)) {
    ::firebase::LogError("Failed to parse schema: ", parser.error_.c_str());
    return false;
  }

  // Parse actual config.
  if (!parser.Parse(config)) {
    ::firebase::LogError("Invalid JSON: ", parser.error_.c_str());
    return false;
  }

  return true;
}

// Test the conformity of the provided .json file.
TEST(GoogleServicesTest, TestConformity) {
  // This is an actual .json, copied from Firebase auth sample app.
  std::string json_file =
      absl::GetFlag(FLAGS_test_srcdir) +
      "/google3/firebase/app/client/cpp/testdata/google-services.json";
  std::string json_str;
  EXPECT_TRUE(flatbuffers::LoadFile(json_file.c_str(), false, &json_str));
  EXPECT_FALSE(json_str.empty());
  EXPECT_TRUE(Parse(json_str.c_str()));
}

// Sanity check to parse a non-conform config.
TEST(GoogleServicesTest, TestNonConformity) {
  EXPECT_FALSE(Parse("{project_info:[1, 2, 3]}"));
}

// Test that extra field in .json is ok.
TEST(GoogleServicesTest, TestExtraField) {
  EXPECT_TRUE(Parse("{game_version:3.1415926}"));
}

}  // namespace fbs
}  // namespace firebase
