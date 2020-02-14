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

#ifndef FIREBASE_TESTING_CPPSDK_JSON_UTIL_H_
#define FIREBASE_TESTING_CPPSDK_JSON_UTIL_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flatbuffers/flexbuffers.h"

namespace firebase {
namespace testing {
namespace cppsdk {

namespace internal {

class EqualsJsonMatcher {
 public:
  explicit EqualsJsonMatcher(const std::string& expected_json);
  bool MatchAndExplain(const std::string& actual,
                       ::testing::MatchResultListener* listener) const;
  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  std::string expected_json_;

  bool CompareFlexbufferReference(
      flexbuffers::Reference reference_actual,
      flexbuffers::Reference reference_expected, std::string key_name,
      ::testing::MatchResultListener* listener) const;
};

}  // namespace internal

inline ::testing::PolymorphicMatcher<internal::EqualsJsonMatcher> EqualsJson(
    const std::string& expected_json) {
  return ::testing::MakePolymorphicMatcher(
      internal::EqualsJsonMatcher(expected_json));
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_JSON_UTIL_H_
