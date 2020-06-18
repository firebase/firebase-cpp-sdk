// Copyright 2020 Google LLC
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

#include "app/tests/flexbuffer_matcher.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flatbuffers/flexbuffers.h"

using ::testing::Not;

namespace {

class FlexbufferMatcherTest : public ::testing::Test {
 protected:
  FlexbufferMatcherTest() : fbb_(512) {}

  void SetUp() override {
    // Null type.
    fbb_.Null();
    fbb_.Finish();
    null_flexbuffer_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Bool type.
    fbb_.Bool(false);
    fbb_.Finish();
    bool_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Bool(true);
    fbb_.Finish();
    bool_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Int type.
    fbb_.Int(5);
    fbb_.Finish();
    int_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Int(10);
    fbb_.Finish();
    int_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    // UInt type.
    fbb_.UInt(100);
    fbb_.Finish();
    uint_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.UInt(500);
    fbb_.Finish();
    uint_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Float type.
    fbb_.Float(12.5);
    fbb_.Finish();
    float_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Float(100.625);
    fbb_.Finish();
    float_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    // String type.
    fbb_.String("A sailor went to sea sea sea");
    fbb_.Finish();
    string_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.String("To see what he could see see see");
    fbb_.Finish();
    string_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Key type.
    fbb_.Key("But all that he could see see see");
    fbb_.Finish();
    key_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Key("Was the bottom of the deep blue sea sea sea");
    fbb_.Finish();
    key_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Map type.
    fbb_.Map([&]() {
      fbb_.Add("lorem", "ipsum");
      fbb_.Add("dolor", "sit");
    });
    fbb_.Finish();
    map_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Map([&]() {
      fbb_.Add("amet", "consectetur");
      fbb_.Add("adipiscing", "elit");
    });
    fbb_.Finish();
    map_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Map([&]() {
      fbb_.Add("sed", "do");
      fbb_.Add("eiusmod", "tempor");
      fbb_.Add("incididunt", "ut");
    });
    fbb_.Finish();
    map_flexbuffer_c_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Vector types.
    fbb_.Vector([&]() {
      fbb_ += "labore";
      fbb_ += "et";
    });
    fbb_.Finish();
    vector_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Vector([&]() {
      fbb_ += "dolore";
      fbb_ += "magna";
    });
    fbb_.Finish();
    vector_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Vector([&]() {
      fbb_ += "aliqua";
      fbb_ += "ut";
      fbb_ += "enim";
    });
    fbb_.Finish();
    vector_flexbuffer_c_ = fbb_.GetBuffer();
    fbb_.Clear();

    // Blob types
    fbb_.Blob("abcde", 5);
    fbb_.Finish();
    blob_flexbuffer_a_ = fbb_.GetBuffer();
    fbb_.Clear();

    fbb_.Blob("fghij", 5);
    fbb_.Finish();
    blob_flexbuffer_b_ = fbb_.GetBuffer();
    fbb_.Clear();
  }

  flexbuffers::Builder fbb_;
  std::vector<uint8_t> null_flexbuffer_;
  std::vector<uint8_t> bool_flexbuffer_a_;
  std::vector<uint8_t> bool_flexbuffer_b_;
  std::vector<uint8_t> int_flexbuffer_a_;
  std::vector<uint8_t> int_flexbuffer_b_;
  std::vector<uint8_t> uint_flexbuffer_a_;
  std::vector<uint8_t> uint_flexbuffer_b_;
  std::vector<uint8_t> float_flexbuffer_a_;
  std::vector<uint8_t> float_flexbuffer_b_;
  std::vector<uint8_t> string_flexbuffer_a_;
  std::vector<uint8_t> string_flexbuffer_b_;
  std::vector<uint8_t> key_flexbuffer_a_;
  std::vector<uint8_t> key_flexbuffer_b_;
  std::vector<uint8_t> map_flexbuffer_a_;
  std::vector<uint8_t> map_flexbuffer_b_;
  std::vector<uint8_t> map_flexbuffer_c_;
  std::vector<uint8_t> vector_flexbuffer_a_;
  std::vector<uint8_t> vector_flexbuffer_b_;
  std::vector<uint8_t> vector_flexbuffer_c_;
  std::vector<uint8_t> blob_flexbuffer_a_;
  std::vector<uint8_t> blob_flexbuffer_b_;
};

// TODO(73494146): These tests should be moved to to the Flatbuffers repo whent
// the matcher itself is.
TEST_F(FlexbufferMatcherTest, IdentityChecking) {
  EXPECT_THAT(null_flexbuffer_, EqualsFlexbuffer(null_flexbuffer_));
  EXPECT_THAT(bool_flexbuffer_a_, EqualsFlexbuffer(bool_flexbuffer_a_));
  EXPECT_THAT(int_flexbuffer_a_, EqualsFlexbuffer(int_flexbuffer_a_));
  EXPECT_THAT(uint_flexbuffer_a_, EqualsFlexbuffer(uint_flexbuffer_a_));
  EXPECT_THAT(float_flexbuffer_a_, EqualsFlexbuffer(float_flexbuffer_a_));
  EXPECT_THAT(string_flexbuffer_a_, EqualsFlexbuffer(string_flexbuffer_a_));
  EXPECT_THAT(key_flexbuffer_a_, EqualsFlexbuffer(key_flexbuffer_a_));
  EXPECT_THAT(map_flexbuffer_a_, EqualsFlexbuffer(map_flexbuffer_a_));
  EXPECT_THAT(vector_flexbuffer_a_, EqualsFlexbuffer(vector_flexbuffer_a_));
  EXPECT_THAT(blob_flexbuffer_a_, EqualsFlexbuffer(blob_flexbuffer_a_));
}

TEST_F(FlexbufferMatcherTest, TypeMismatch) {
  EXPECT_THAT(null_flexbuffer_, Not(EqualsFlexbuffer(int_flexbuffer_b_)));
  EXPECT_THAT(int_flexbuffer_a_, Not(EqualsFlexbuffer(uint_flexbuffer_b_)));
  EXPECT_THAT(float_flexbuffer_a_, Not(EqualsFlexbuffer(bool_flexbuffer_b_)));
  EXPECT_THAT(key_flexbuffer_a_, Not(EqualsFlexbuffer(string_flexbuffer_b_)));
  EXPECT_THAT(map_flexbuffer_a_, Not(EqualsFlexbuffer(vector_flexbuffer_b_)));
}

TEST_F(FlexbufferMatcherTest, ValueMismatch) {
  EXPECT_THAT(bool_flexbuffer_a_, Not(EqualsFlexbuffer(bool_flexbuffer_b_)));
  EXPECT_THAT(int_flexbuffer_a_, Not(EqualsFlexbuffer(int_flexbuffer_b_)));
  EXPECT_THAT(uint_flexbuffer_a_, Not(EqualsFlexbuffer(uint_flexbuffer_b_)));
  EXPECT_THAT(float_flexbuffer_a_, Not(EqualsFlexbuffer(float_flexbuffer_b_)));
  EXPECT_THAT(string_flexbuffer_a_,
              Not(EqualsFlexbuffer(string_flexbuffer_b_)));
  EXPECT_THAT(key_flexbuffer_a_, Not(EqualsFlexbuffer(key_flexbuffer_b_)));
  EXPECT_THAT(map_flexbuffer_a_, Not(EqualsFlexbuffer(map_flexbuffer_b_)));
  EXPECT_THAT(vector_flexbuffer_a_,
              Not(EqualsFlexbuffer(vector_flexbuffer_b_)));
  EXPECT_THAT(blob_flexbuffer_a_, Not(EqualsFlexbuffer(blob_flexbuffer_b_)));
}

TEST_F(FlexbufferMatcherTest, SizeMismatch) {
  EXPECT_THAT(map_flexbuffer_a_, Not(EqualsFlexbuffer(map_flexbuffer_c_)));
  EXPECT_THAT(map_flexbuffer_a_, Not(EqualsFlexbuffer(map_flexbuffer_c_)));
  EXPECT_THAT(vector_flexbuffer_a_,
              Not(EqualsFlexbuffer(vector_flexbuffer_c_)));
  EXPECT_THAT(vector_flexbuffer_a_,
              Not(EqualsFlexbuffer(vector_flexbuffer_c_)));
}

}  // namespace
