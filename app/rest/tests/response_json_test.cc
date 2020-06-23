/*
 * Copyright 2017 Google LLC
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

#include "app/rest/response_json.h"
#include <utility>
#include "app/rest/sample_generated.h"
#include "app/rest/sample_resource.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {

class ResponseSample : public ResponseJson<Sample, SampleT> {
 public:
  ResponseSample() : ResponseJson(sample_resource_data) {}
  ResponseSample(ResponseSample&& rhs) : ResponseJson(std::move(rhs)) {}

  std::string token() const {
    return application_data_ ? application_data_->token : std::string();
  }

  int number() const {
    return application_data_ ? application_data_->number : 0;
  }
};

// Test the creation.
TEST(ResponseJsonTest, Creation) {
  ResponseSample response;
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());
}

// Test move operation.
TEST(ResponseJsonTest, Move) {
  ResponseSample src;
  const char body[] =
      "{"
      "  \"token\": \"abc\","
      "  \"number\": 123"
      "}";
  src.ProcessBody(body, sizeof(body));
  src.MarkCompleted();
  const auto check_non_empty = [](const ResponseSample& response) {
    EXPECT_TRUE(response.header_completed());
    EXPECT_TRUE(response.body_completed());
    EXPECT_EQ("abc", response.token());
    EXPECT_EQ(123, response.number());
  };
  check_non_empty(src);

  ResponseSample dest = std::move(src);
  // src should now be moved-from and its parsed fields should be blank.
  // NOLINT disables ClangTidy checks that warn about access to moved-from
  // object. In this case, this is deliberate. The only data member that gets
  // accessed is application_data_, which is std::unique_ptr and has
  // well-defined state (equivalent to default-created).
  EXPECT_TRUE(src.token().empty());  // NOLINT
  EXPECT_EQ(0, src.number());        // NOLINT
  // dest should now contain everything src contained.
  check_non_empty(dest);
}

// Test the case server respond with just {}.
TEST(ResponseJsonTest, EmptyJsonResponse) {
  ResponseSample response;
  const char body[] =
      "{"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_TRUE(response.token().empty());
  EXPECT_EQ(0, response.number());
}

// Test the case server respond with non-empty standard JSON string.
TEST(ResponseJsonTest, StandardJsonResponse) {
  ResponseSample response;
  const char body[] =
      "{"
      "  \"token\": \"abc\","
      "  \"number\": 123"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_EQ("abc", response.token());
  EXPECT_EQ(123, response.number());
}

// Test the case server respond with non-empty JSON string.
TEST(ResponseJsonTest, NonStandardJsonResponse) {
  ResponseSample response;
  // JSON format has non-standard variations:
  //     quotation around field name or not;
  //     quotation around non-string field value or not;
  //     single quotes vs double quotes
  // Here we try some of the non-standard variations.
  const char body[] =
      "{"
      "  token: 'abc',"
      "  'number': '123'"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_EQ("abc", response.token());
  EXPECT_EQ(123, response.number());
}

}  // namespace rest
}  // namespace firebase
