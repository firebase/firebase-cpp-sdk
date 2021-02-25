// Copyright 2019 Google LLC
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

#include "database/src/desktop/core/server_values.h"

#include <ctime>

#include "database/src/include/firebase/database/common.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

// We expect the result of GenerateServerValues to be pretty close to the
// current time. It might be off by a second or so, but much more than that
// might indicate an issue.
const int kEpsilonMs = 3000;

TEST(ServerValues, ServerTimestamp) {
  EXPECT_EQ(ServerTimestamp(), Variant(std::map<Variant, Variant>{
                                   std::make_pair(".sv", "timestamp"),
                               }));
}

TEST(ServerValues, GenerateServerValues) {
  int64_t current_time_ms = time(nullptr) * 1000;

  Variant result = GenerateServerValues(0);

  EXPECT_TRUE(result.is_map());
  EXPECT_EQ(result.map().size(), 1);
  EXPECT_NE(result.map().find("timestamp"), result.map().end());
  EXPECT_TRUE(result.map()["timestamp"].is_int64());
  EXPECT_NEAR(result.map()["timestamp"].int64_value(), current_time_ms,
              kEpsilonMs);
}

TEST(ServerValues, GenerateServerValuesWithTimeOffset) {
  int64_t current_time_ms = time(nullptr) * 1000;

  Variant result = GenerateServerValues(5000);

  EXPECT_TRUE(result.is_map());
  EXPECT_EQ(result.map().size(), 1);
  EXPECT_NE(result.map().find("timestamp"), result.map().end());
  EXPECT_TRUE(result.map()["timestamp"].is_int64());
  EXPECT_NEAR(result.map()["timestamp"].int64_value(), current_time_ms + 5000,
              kEpsilonMs);
}

TEST(ServerValues, ResolveDeferredValueNull) {
  Variant null_variant = Variant::Null();
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(null_variant, server_values);

  EXPECT_EQ(result, Variant::Null());
}

TEST(ServerValues, ResolveDeferredValueInt64) {
  Variant int_variant = Variant::FromInt64(12345);
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(int_variant, server_values);

  EXPECT_EQ(result, Variant::FromInt64(12345));
}

TEST(ServerValues, ResolveDeferredValueDouble) {
  Variant double_variant = Variant::FromDouble(3.14);
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(double_variant, server_values);

  EXPECT_EQ(result, Variant::FromDouble(3.14));
}

TEST(ServerValues, ResolveDeferredValueBool) {
  Variant bool_variant = Variant::FromBool(true);
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(bool_variant, server_values);

  EXPECT_EQ(result, Variant::FromBool(true));
}

TEST(ServerValues, ResolveDeferredValueStaticString) {
  Variant static_string_variant = Variant::FromStaticString("Test");
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(static_string_variant, server_values);

  EXPECT_EQ(result, Variant::FromStaticString("Test"));
}

TEST(ServerValues, ResolveDeferredValueMutableString) {
  Variant mutable_string_variant = Variant::FromMutableString("Test");
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(mutable_string_variant, server_values);

  EXPECT_EQ(result, Variant::FromMutableString("Test"));
}

TEST(ServerValues, ResolveDeferredValueVector) {
  Variant vector_variant = std::vector<Variant>{1, 2, 3, 4};
  Variant server_values = GenerateServerValues(0);
  Variant expected_vector_variant = vector_variant;

  Variant result = ResolveDeferredValueSnapshot(vector_variant, server_values);

  EXPECT_EQ(result, expected_vector_variant);
}

TEST(ServerValues, ResolveDeferredValueSimpleMap) {
  Variant simple_map_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  Variant server_values = GenerateServerValues(0);
  Variant expected_simple_map_variant = simple_map_variant;

  Variant result = ResolveDeferredValue(simple_map_variant, server_values);

  EXPECT_EQ(result, expected_simple_map_variant);
}

TEST(ServerValues, ResolveDeferredValueNestedMap) {
  Variant nested_map_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                         std::make_pair("fff", 500),
                     }),
  };
  Variant expected_nested_map_variant = nested_map_variant;
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(nested_map_variant, server_values);

  EXPECT_EQ(result, expected_nested_map_variant);
}

TEST(ServerValues, ResolveDeferredValueTimestamp) {
  int64_t current_time_ms = time(nullptr) * 1000;
  Variant timestamp = ServerTimestamp();
  Variant server_values = GenerateServerValues(0);

  Variant result = ResolveDeferredValue(timestamp, server_values);

  EXPECT_TRUE(result.is_int64());
  EXPECT_NEAR(result.int64_value(), current_time_ms, kEpsilonMs);
}

TEST(ServerValues, ResolveDeferredValueSnapshot) {
  int64_t current_time_ms = time(nullptr) * 1000;
  Variant nested_map_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                         std::make_pair("fff", ServerTimestamp()),
                     }),
  };
  Variant server_values = GenerateServerValues(0);

  Variant result =
      ResolveDeferredValueSnapshot(nested_map_variant, server_values);

  EXPECT_EQ(result.map()["aaa"].int64_value(), 100);
  EXPECT_EQ(result.map()["bbb"].int64_value(), 200);
  EXPECT_EQ(result.map()["ccc"].map()["ddd"].int64_value(), 300);
  EXPECT_EQ(result.map()["ccc"].map()["eee"].int64_value(), 400);
  EXPECT_NEAR(result.map()["ccc"].map()["fff"].int64_value(), current_time_ms,
              kEpsilonMs);
}

TEST(ServerValues, ResolveDeferredValueMerge) {
  int64_t current_time_ms = time(nullptr) * 1000;
  Variant merge(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc/ddd", 300),
      std::make_pair("ccc/eee", ServerTimestamp()),
  });
  CompoundWrite write = CompoundWrite::FromVariantMerge(merge);
  Variant server_values = GenerateServerValues(0);

  CompoundWrite result = ResolveDeferredValueMerge(write, server_values);

  EXPECT_EQ(*result.write_tree().GetValueAt(Path("aaa")), 100);
  EXPECT_EQ(*result.write_tree().GetValueAt(Path("bbb")), 200);
  EXPECT_EQ(*result.write_tree().GetValueAt(Path("ccc/ddd")), 300);
  EXPECT_NEAR(result.write_tree().GetValueAt(Path("ccc/eee"))->int64_value(),
              current_time_ms, kEpsilonMs);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
