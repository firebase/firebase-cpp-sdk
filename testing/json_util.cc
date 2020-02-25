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

#include "testing/json_util.h"

#include <ostream>
#include <string>

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

namespace firebase {
namespace testing {
namespace cppsdk {
namespace internal {

EqualsJsonMatcher::EqualsJsonMatcher(const std::string& expected_json)
    : expected_json_(expected_json) {}

bool EqualsJsonMatcher::MatchAndExplain(
    const std::string& actual, ::testing::MatchResultListener* listener) const {
  flatbuffers::Parser parser_actual;
  flatbuffers::Parser parser_expected;

  flexbuffers::Builder builder_actual;
  flexbuffers::Builder builder_expected;

  if (actual.empty() || !parser_actual.ParseFlexBuffer(actual.c_str(), nullptr,
                                                       &builder_actual)) {
    *listener << "Unable to parse actual value (" << parser_actual.error_
              << ").\n";
    return false;
  }
  if (expected_json_.empty() ||
      !parser_expected.ParseFlexBuffer(expected_json_.c_str(), nullptr,
                                       &builder_expected)) {
    *listener << "Unable to parse expected value (" << parser_expected.error_
              << ").\n";
    return false;
  }

  std::vector<uint8_t> buffer_actual = builder_actual.GetBuffer();
  std::vector<uint8_t> buffer_expected = builder_expected.GetBuffer();

  auto root_actual = flexbuffers::GetRoot(buffer_actual);
  auto root_expected = flexbuffers::GetRoot(buffer_expected);
  return CompareFlexbufferReference(root_actual, root_expected, "root",
                                    listener);
}

void EqualsJsonMatcher::DescribeTo(std::ostream* os) const {
  *os << "equals JSON: \n" << expected_json_;
}

void EqualsJsonMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "doesn't equal JSON: \n" << expected_json_;
}

bool EqualsJsonMatcher::CompareFlexbufferReference(
    flexbuffers::Reference reference_actual,
    flexbuffers::Reference reference_expected, std::string key_name,
    ::testing::MatchResultListener* listener) const {

  auto type_actual = reference_actual.GetType();
  auto type_expected = reference_expected.GetType();
  if (type_actual != type_expected) {
    *listener << "Type Mismatch (" << key_name << ")\n";
    return false;
  } else if (type_actual == flexbuffers::FBT_MAP) {
    flexbuffers::Map map_actual = reference_actual.AsMap();
    flexbuffers::Map map_expected = reference_expected.AsMap();

    auto keys_actual = map_actual.Keys();
    auto keys_expected = map_expected.Keys();
    if (keys_actual.size() != keys_expected.size()) {
      *listener << "Size of " << key_name
                << " does not match.  Expected: " << keys_expected.size()
                << " Actual: " << keys_actual.size() << "\n";
      return false;
    }

    bool map_matches = true;
    for (int i = 0; i < keys_actual.size(); i++) {
      auto key_actual = keys_actual[i];
      auto key_expected = keys_expected[i];

      if (key_actual.ToString() != key_expected.ToString()) {
        *listener << "Key mismatch in " << key_name
                  << " Expected: " << key_expected.ToString()
                  << " Actual: " << key_actual.ToString() << "\n";
        map_matches = false;
        continue;
      }

      auto val_actual = map_actual.Values()[i];
      auto val_expected = map_expected.Values()[i];
      if (!CompareFlexbufferReference(
              val_actual, val_expected,
              key_name + "[" + key_actual.ToString() + "]", listener)) {
        map_matches = false;
      }
    }
    return map_matches;
  } else if (type_actual == flexbuffers::FBT_VECTOR) {
    flexbuffers::Vector vec_actual = reference_actual.AsVector();
    flexbuffers::Vector vec_expected = reference_expected.AsVector();

    if (vec_actual.size() != vec_expected.size()) {
      *listener << "Size of " << key_name
                << " does not match.  Expected: " << vec_expected.size()
                << " Actual: " << vec_actual.size() << "\n";
      return false;
    }

    bool vectors_match = true;
    for (int i = 0; i < vec_actual.size(); i++) {
      auto vec_element_actual = vec_actual[i];
      auto vec_element_expected = vec_expected[i];
      if (!CompareFlexbufferReference(vec_element_actual, vec_element_expected,
                                      key_name + "[" + std::to_string(i) + "]",
                                      listener)) {
        vectors_match = false;
      }
    }
    return vectors_match;
  } else {
    auto str_actual = reference_actual.ToString();
    auto str_expected = reference_expected.ToString();

    if (str_actual != str_expected) {
      *listener << "Values for " << key_name << " do not match.\n"
                << "Expected: " << str_expected << "\n"
                << "Actual: " << str_actual << "\n";
      return false;
    }
    return true;
  }
}

}  // namespace internal

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
