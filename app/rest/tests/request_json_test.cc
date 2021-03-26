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

#include "app/rest/request_json.h"
#include "app/rest/sample_generated.h"
#include "app/rest/sample_resource.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {

class RequestSample : public RequestJson<Sample, SampleT> {
 public:
  RequestSample() : RequestJson(sample_resource_data) {}

  void set_token(const char* token) {
    application_data_->token = token;
    UpdatePostFields();
  }

  void set_number(int number) {
    application_data_->number = number;
    UpdatePostFields();
  }

  void UpdatePostFieldForTest() {
    UpdatePostFields();
  }
};

// Test the creation.
TEST(RequestJsonTest, Creation) {
  RequestSample request;
  EXPECT_TRUE(request.options().post_fields.empty());
}

// Test the case where no field is set.
TEST(RequestJsonTest, UpdatePostFieldsEmpty) {
  RequestSample request;
  request.UpdatePostFieldForTest();
  EXPECT_EQ("{\n"
            "}\n", request.options().post_fields);
}

// Test with fields set.
TEST(RequestJsonTest, UpdatePostFields) {
  RequestSample request;
  request.set_number(123);
  request.set_token("abc");
  EXPECT_EQ("{\n"
            "  token: \"abc\",\n"
            "  number: 123\n"
            "}\n", request.options().post_fields);
}

}  // namespace rest
}  // namespace firebase
