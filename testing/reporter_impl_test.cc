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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/reporter.h"
#include "testing/reporter_impl_fake.h"

namespace firebase {
namespace testing {
namespace cppsdk {

class ReporterImplTest : public ::testing::Test {
 protected:
  void SetUp() override { reporter_.reset(); }

  void TearDown() override {
    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  Reporter reporter_;
};

TEST_F(ReporterImplTest, Test) {
  reporter_.addExpectation(
      "fake_function_name", "fake_function_result", kAny,
      {"fake_argument0", "fake_argument1", "fake_argument2"});
  fake::TestFunction();
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
