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

#include <string>
#include <vector>

#include "testing/reporter.h"
#include "testing/run_all_tests.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace testing {
namespace cppsdk {

TEST(ReportRowTest, TestGetFake) {
  ReportRow r("fake", "result", kAny, {"1", "2", "3"});
  EXPECT_EQ(r.getFake(), "fake");
}

TEST(ReportRowTest, TestGetResult) {
  ReportRow r("fake", "result", kAny, {"1", "2", "3"});
  EXPECT_EQ(r.getResult(), "result");
}

TEST(ReportRowTest, TestGetArgs) {
  ReportRow r("fake", "result", kAny, {"1", "2", "3"});
  EXPECT_THAT(r.getArgs(),
              ::testing::Eq(std::vector<std::string>{"1", "2", "3"}));
}

TEST(ReportRowTest, TestGetPlatform) {
  ReportRow r("fake", "result", kAny, {"1", "2", "3"});
  EXPECT_EQ(r.getPlatform(), kAny);

  r = ReportRow("fake", "result", kAndroid, {"1", "2", "3"});
  EXPECT_EQ(r.getPlatform(), kAndroid);

  r = ReportRow("fake", "result", kIos, {"1", "2", "3"});
  EXPECT_EQ(r.getPlatform(), kIos);

  r = ReportRow("fake", "result", {"1", "2", "3"});
  EXPECT_EQ(r.getPlatform(), kAny);
}

TEST(ReportRowTest, TestGetPlatformString) {
  ReportRow r("fake", "result", kAny, {"1", "2", "3"});
  EXPECT_EQ(r.getPlatformString(), "any");

  r = ReportRow("fake", "result", kAndroid, {"1", "2", "3"});
  EXPECT_EQ(r.getPlatformString(), "android");

  r = ReportRow("fake", "result", kIos, {"1", "2", "3"});
  EXPECT_EQ(r.getPlatformString(), "ios");

  r = ReportRow("fake", "result", {"1", "2", "3"});
  EXPECT_EQ(r.getPlatformString(), "any");
}

TEST(ReportRowTest, TestToString) {
  ReportRow r("fake", "result", kAny, {"1", "2", "3"});
  EXPECT_EQ(r.toString(), "fake result any [1 2 3]");

  r = ReportRow("", "", kAny, {});
  EXPECT_EQ(r.toString(), "  any []");
}

// Compare only fake_ values
TEST(ReportRowTest, TestLessThanOperator) {
  ReportRow r1("abc", "9876", kAny, {"a", "a", "a"});
  ReportRow r2("xyz", "5555", kAny, {"x", "x", "x"});

  EXPECT_TRUE(r1 < r2);
  EXPECT_FALSE(r2 < r1);

  EXPECT_FALSE(r1 < r1);
  EXPECT_FALSE(r2 < r2);
}

TEST(ReportRowTest, TestEqualOperator) {
  ReportRow r1("abc", "9876", kAny, {"a", "a", "a"});
  ReportRow r2("xyz", "5555", kAny, {"x", "x", "x"});
  ReportRow r3("xyz", "4444", kAny, {"z", "z", "z"});

  EXPECT_FALSE(r1 == r2);
  EXPECT_FALSE(r2 == r1);

  EXPECT_TRUE(r1 == r1);
  EXPECT_TRUE(r2 == r2);

  EXPECT_FALSE(r2 == r3);
}

TEST(ReportRowTest, TestNotEqualOperator) {
  ReportRow r1("abc", "9876", kAny, {"a", "a", "a"});
  ReportRow r2("xyz", "5555", kAny, {"x", "x", "x"});
  ReportRow r3("xyz", "4444", kAny, {"z", "z", "z"});

  EXPECT_TRUE(r1 != r2);
  EXPECT_TRUE(r2 != r1);

  EXPECT_FALSE(r1 != r1);
  EXPECT_FALSE(r2 != r2);

  EXPECT_TRUE(r2 != r3);
}

class ReporterTest : public ::testing::Test {
 protected:
  Reporter r_;
};

TEST_F(ReporterTest, TestGetExpectations) {
  r_.addExpectation("fake1", "result1", kAny, {"one", "two"});
  r_.addExpectation("fake2", "result2", kAny, {"one", "two"});
  r_.addExpectation(ReportRow("fake3", "result3", kAny, {"one", "two"}));

  EXPECT_THAT(r_.getExpectations(),
              ::testing::Eq(std::vector<ReportRow>{
                  ReportRow("fake1", "result1", kAny, {"one", "two"}),
                  ReportRow("fake2", "result2", kAny, {"one", "two"}),
                  ReportRow("fake3", "result3", kAny, {"one", "two"})}));
}

TEST_F(ReporterTest, TestGetExpectationsSortedByKey) {
  r_.addExpectation(ReportRow("fake3", "result3", kAny, {"one", "two"}));
  r_.addExpectation("fake2", "result2", kAny, {"one", "two"});
  r_.addExpectation("fake1", "result1", kAny, {"one", "two"});

  EXPECT_THAT(r_.getExpectations(),
              ::testing::Eq(std::vector<ReportRow>{
                  ReportRow("fake1", "result1", kAny, {"one", "two"}),
                  ReportRow("fake2", "result2", kAny, {"one", "two"}),
                  ReportRow("fake3", "result3", kAny, {"one", "two"})}));
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || defined(__ANDROID__)
TEST_F(ReporterTest, TestGetExpectationsAndroid) {
  r_.addExpectation("fake1", "result1", kAny, {"one", "two"});
  r_.addExpectation("fake2", "result2", kAndroid, {"one", "two"});
  r_.addExpectation(ReportRow("fake3", "result3", kIos, {"one", "two"}));

  EXPECT_THAT(r_.getExpectations(),
              ::testing::Eq(std::vector<ReportRow>{
                  ReportRow("fake1", "result1", kAny, {"one", "two"}),
                  ReportRow("fake2", "result2", kAndroid, {"one", "two"})}));
}

TEST_F(ReporterTest, TestResetAndroid) {
  r_.addExpectation("fake1", "result1", kAny, {"one", "two"});

  EXPECT_THAT(r_.getExpectations(),
              ::testing::Eq(std::vector<ReportRow>{
                  ReportRow("fake1", "result1", kAny, {"one", "two"})}));
  r_.reset();
  EXPECT_THAT(r_.getExpectations(), ::testing::Eq(std::vector<ReportRow>{}));
}

TEST_F(ReporterTest, TestGetFakeReportsAndroid) {
  EXPECT_THAT(r_.getFakeReports(), ::testing::Eq(std::vector<ReportRow>{}));
}

TEST_F(ReporterTest, TestGetAllFakesAndroid) {
  EXPECT_THAT(r_.getAllFakes(), ::testing::Eq(std::vector<std::string>{}));
}

TEST_F(ReporterTest, TestGetFakeArgsAndroid) {
  EXPECT_THAT(r_.getFakeArgs("some_fake"),
              ::testing::Eq(std::vector<std::string>{}));
}

TEST_F(ReporterTest, TestGetFakeResultAndroid) {
  EXPECT_EQ(r_.getFakeResult("some_fake"), "");
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || defined(__ANDROID__)

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
