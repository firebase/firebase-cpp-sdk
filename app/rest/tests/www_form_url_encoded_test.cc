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

#include "app/rest/www_form_url_encoded.h"

#include "app/rest/util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {

class WwwFormUrlEncodedTest : public ::testing::Test {
 protected:
  void SetUp() override { util::Initialize(); }

  void TearDown() override { util::Terminate(); }
};

TEST_F(WwwFormUrlEncodedTest, Initialize) {
  std::string initial("something");
  WwwFormUrlEncoded form(&initial);
  EXPECT_EQ(initial, form.form_data());
}

TEST_F(WwwFormUrlEncodedTest, AddFields) {
  std::string form_data;
  WwwFormUrlEncoded form(&form_data);
  form.Add("foo", "bar");
  form.Add("bash", "bish bosh");
  form.Add("h:&=l\nlo", "g@@db=\r\tye&\xfe");
  form.Add(WwwFormUrlEncoded::Item("hip", "hop"));
  EXPECT_EQ("foo=bar&bash=bish%20bosh&"
            "h%3A%26%3Dl%0Alo=g%40%40db%3D%0D%09ye%26%FE&"
            "hip=hop",
            form.form_data());
}

TEST_F(WwwFormUrlEncodedTest, ParseEmpty) {
  auto items = WwwFormUrlEncoded::Parse("");
  EXPECT_EQ(0, items.size());
}

TEST_F(WwwFormUrlEncodedTest, ParseForm) {
  WwwFormUrlEncoded::Item expected_items[] = {
      WwwFormUrlEncoded::Item("h:llo", "g@@dbye&"),
      WwwFormUrlEncoded::Item("bash", "bish bosh"),
  };
  auto items = WwwFormUrlEncoded::Parse(
      "h%3Allo=g%40%40dbye%26&"
      "bash=bish%20bosh");
  EXPECT_EQ(sizeof(expected_items) / sizeof(expected_items[0]), items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    EXPECT_EQ(expected_items[i].key, items[i].key) << "Key " << i;
    EXPECT_EQ(expected_items[i].value, items[i].value) << "Value " << i;
  }
}

TEST_F(WwwFormUrlEncodedTest, ParseFormWithOtherSeparators) {
  WwwFormUrlEncoded::Item expected_items[] = {
      WwwFormUrlEncoded::Item("h:llo", "g@@dbye&"),
      WwwFormUrlEncoded::Item("bash", "bish bosh"),
      WwwFormUrlEncoded::Item("hello", "you"),
  };
  auto items = WwwFormUrlEncoded::Parse(
      "h%3Allo=g%40%40dbye%26&\r "
      "bash=bish%20bosh\n&\t&\nhello=you");
  EXPECT_EQ(sizeof(expected_items) / sizeof(expected_items[0]), items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    EXPECT_EQ(expected_items[i].key, items[i].key) << "Key " << i;
    EXPECT_EQ(expected_items[i].value, items[i].value) << "Value " << i;
  }
}

TEST_F(WwwFormUrlEncodedTest, ParseFormWithInvalidFields) {
  WwwFormUrlEncoded::Item expected_items[] = {
      WwwFormUrlEncoded::Item("h:llo", "g@@dbye&"),
      WwwFormUrlEncoded::Item("bash", "bish bosh"),
  };
  auto items = WwwFormUrlEncoded::Parse(
      "h%3Allo=g%40%40dbye%26&"
      "invalidfield0&"
      "bash=bish%20bosh&"
      "moreinvaliddata&"
      "ignorethisaswell");
  EXPECT_EQ(sizeof(expected_items) / sizeof(expected_items[0]), items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    EXPECT_EQ(expected_items[i].key, items[i].key) << "Key " << i;
    EXPECT_EQ(expected_items[i].value, items[i].value) << "Value " << i;
  }
}

}  // namespace rest
}  // namespace firebase
