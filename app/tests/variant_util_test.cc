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

#include "app/src/variant_util.h"

#include <cstring>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/tests/flexbuffer_matcher.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/json_util.h"

namespace {

using ::firebase::Variant;
using ::firebase::testing::cppsdk::EqualsJson;
using ::firebase::util::JsonToVariant;
using ::firebase::util::VariantToFlexbuffer;
using ::firebase::util::VariantToJson;
using ::flexbuffers::GetRoot;
using ::testing::Eq;
using ::testing::Not;
using ::testing::StrEq;

TEST(UtilDesktopTest, JsonToVariantNull) {
  EXPECT_THAT(JsonToVariant("null"), Eq(Variant::Null()));
}

TEST(UtilDesktopTest, JsonToVariantInt64) {
  EXPECT_THAT(JsonToVariant("0"), Eq(Variant(0)));
  EXPECT_THAT(JsonToVariant("100"), Eq(Variant(100)));
  EXPECT_THAT(JsonToVariant("8000000000"), Eq(Variant(int64_t(8000000000L))));
  EXPECT_THAT(JsonToVariant("-100"), Eq(Variant(-100)));
  EXPECT_THAT(JsonToVariant("-8000000000"), Eq(Variant(int64_t(-8000000000L))));
}

TEST(UtilDesktopTest, JsonToVariantDouble) {
  EXPECT_THAT(JsonToVariant("0.0"), Eq(Variant(0.0)));
  EXPECT_THAT(JsonToVariant("100.0"), Eq(Variant(100.0)));
  EXPECT_THAT(JsonToVariant("8000000000.0"), Eq(Variant(8000000000.0)));
  EXPECT_THAT(JsonToVariant("-100.0"), Eq(Variant(-100.0)));
  EXPECT_THAT(JsonToVariant("-8000000000.0"), Eq(Variant(-8000000000.0)));
}

TEST(UtilDesktopTest, JsonToVariantBool) {
  EXPECT_THAT(JsonToVariant("true"), Eq(Variant::True()));
  EXPECT_THAT(JsonToVariant("false"), Eq(Variant::False()));
}

TEST(UtilDesktopTest, JsonToVariantString) {
  EXPECT_THAT(JsonToVariant("\"Hello, World!\""), Eq(Variant("Hello, World!")));
  EXPECT_THAT(JsonToVariant("\"100\""), Eq(Variant("100")));
  EXPECT_THAT(JsonToVariant("\"false\""), Eq(Variant("false")));
}

TEST(UtilDesktopTest, JsonToVariantVector) {
  EXPECT_THAT(JsonToVariant("[]"), Eq(Variant::EmptyVector()));
  std::vector<Variant> int_vector{1, 2, 3, 4};
  EXPECT_THAT(JsonToVariant("[1, 2, 3, 4]"), Eq(Variant(int_vector)));
  std::vector<Variant> mixed_vector{1, true, 3.5, "hello"};
  EXPECT_THAT(JsonToVariant("[1, true, 3.5, \"hello\"]"), Eq(mixed_vector));
  std::vector<Variant> nested_vector{1, true, 3.5, "hello", int_vector};
  EXPECT_THAT(JsonToVariant("[1, true, 3.5, \"hello\", [1, 2, 3, 4]]"),
              Eq(nested_vector));
}

TEST(UtilDesktopTest, JsonToVariantMap) {
  EXPECT_THAT(JsonToVariant("{}"), Eq(Variant::EmptyMap()));
  std::map<Variant, Variant> int_map{
      std::make_pair("one_hundred", 100),
      std::make_pair("two_hundred", 200),
      std::make_pair("three_hundred", 300),
      std::make_pair("four_hundred", 400),
  };
  EXPECT_THAT(JsonToVariant("{"
                            "  \"one_hundred\": 100,"
                            "  \"two_hundred\": 200,"
                            "  \"three_hundred\": 300,"
                            "  \"four_hundred\": 400"
                            "}"),
              Eq(Variant(int_map)));
  std::map<Variant, Variant> mixed_map{
      std::make_pair("boolean_value", true),
      std::make_pair("int_value", 100),
      std::make_pair("double_value", 3.5),
      std::make_pair("string_value", "Good-bye, World!"),
  };
  EXPECT_THAT(JsonToVariant("{"
                            "  \"boolean_value\": true,"
                            "  \"int_value\": 100,"
                            "  \"double_value\": 3.5,"
                            "  \"string_value\": \"Good-bye, World!\""
                            "}"),
              Eq(mixed_map));
  std::map<Variant, Variant> nested_map{
      std::make_pair("int_map", int_map),
      std::make_pair("mixed_map", mixed_map),
  };
  EXPECT_THAT(JsonToVariant("{"
                            "  \"int_map\": {"
                            "    \"one_hundred\": 100,"
                            "    \"two_hundred\": 200,"
                            "    \"three_hundred\": 300,"
                            "    \"four_hundred\": 400"
                            "  },"
                            "  \"mixed_map\": {"
                            "    \"int_value\": 100,"
                            "    \"boolean_value\": true, "
                            "    \"double_value\": 3.5,"
                            "    \"string_value\": \"Good-bye, World!\""
                            "  }"
                            "}"),
              Eq(nested_map));
}

TEST(UtilDesktopTest, VariantToJsonNull) {
  EXPECT_THAT(VariantToJson(Variant::Null()), EqualsJson("null"));
}

TEST(UtilDesktopTest, VariantToJsonInt64) {
  EXPECT_THAT(VariantToJson(Variant(0)), EqualsJson("0"));
  EXPECT_THAT(VariantToJson(Variant(100)), EqualsJson("100"));
  EXPECT_THAT(VariantToJson(Variant(int64_t(8000000000L))),
              EqualsJson("8000000000"));
  EXPECT_THAT(VariantToJson(Variant(-100)), EqualsJson("-100"));
  EXPECT_THAT(VariantToJson(Variant(int64_t(-8000000000L))),
              EqualsJson("-8000000000"));
}

TEST(UtilDesktopTest, VariantToJsonDouble) {
  EXPECT_THAT(VariantToJson(Variant(0.0)), EqualsJson("0"));
  EXPECT_THAT(VariantToJson(Variant(100.0)), EqualsJson("100"));
  EXPECT_THAT(VariantToJson(Variant(-100.0)), EqualsJson("-100"));
}

TEST(UtilDesktopTest, VariantToJsonBool) {
  EXPECT_THAT(VariantToJson(Variant::True()), EqualsJson("true"));
  EXPECT_THAT(VariantToJson(Variant::False()), EqualsJson("false"));
}

TEST(UtilDesktopTest, VariantToJsonStaticString) {
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("Hello, World!")),
              EqualsJson("\"Hello, World!\""));
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("100")),
              EqualsJson("\"100\""));
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("false")),
              EqualsJson("\"false\""));
}

TEST(UtilDesktopTest, VariantToJsonMutableString) {
  EXPECT_THAT(VariantToJson(Variant::FromMutableString("Hello, World!")),
              EqualsJson("\"Hello, World!\""));
  EXPECT_THAT(VariantToJson(Variant::FromMutableString("100")),
              EqualsJson("\"100\""));
  EXPECT_THAT(VariantToJson(Variant::FromMutableString("false")),
              EqualsJson("\"false\""));
}

TEST(UtilDesktopTest, VariantToJsonWithEscapeCharacters) {
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("Hello, \"World\"!")),
              EqualsJson("\"Hello, \\\"World\\\"!\""));
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("Hello, \\backslash\\!")),
              EqualsJson("\"Hello, \\\\backslash\\\\!\""));
  EXPECT_THAT(
      VariantToJson(Variant::FromStaticString("Hello, // forwardslash!")),
      EqualsJson("\"Hello, \\/\\/ forwardslash!\""));
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("Hello!\nHello again!")),
              EqualsJson("\"Hello!\\nHello again!\""));
  EXPECT_THAT(VariantToJson(Variant::FromStaticString("こんにちは")),
              EqualsJson("\"\\u3053\\u3093\\u306B\\u3061\\u306F\""));
}

TEST(UtilDesktopTest, VariantToJsonVector) {
  EXPECT_THAT(VariantToJson(Variant::EmptyVector()), EqualsJson("[]"));
  EXPECT_THAT(VariantToJson(Variant::EmptyVector(), true), EqualsJson("[]"));

  std::vector<Variant> int_vector{1, 2, 3, 4};
  EXPECT_THAT(VariantToJson(Variant(int_vector)), StrEq("[1,2,3,4]"));
  EXPECT_THAT(VariantToJson(Variant(int_vector), true),
              StrEq("[\n  1,\n  2,\n  3,\n  4\n]"));

  std::vector<Variant> mixed_vector{1, true, 3.5, "hello"};
  EXPECT_THAT(VariantToJson(Variant(mixed_vector)),
              StrEq("[1,true,3.5,\"hello\"]"));
  EXPECT_THAT(VariantToJson(Variant(mixed_vector), true),
              StrEq("[\n  1,\n  true,\n  3.5,\n  \"hello\"\n]"));

  std::vector<Variant> nested_vector{1, true, 3.5, "hello", int_vector};
  EXPECT_THAT(VariantToJson(nested_vector),
              StrEq("[1,true,3.5,\"hello\",[1,2,3,4]]"));
  EXPECT_THAT(VariantToJson(nested_vector, true),
              StrEq("[\n  1,\n  true,\n  3.5,\n  \"hello\",\n"
                    "  [\n    1,\n    2,\n    3,\n    4\n  ]\n]"));
}

TEST(UtilDesktopTest, VariantToJsonMapWithStringKeys) {
  EXPECT_THAT(VariantToJson(Variant::EmptyMap()), EqualsJson("{}"));
  std::map<Variant, Variant> int_map{
      std::make_pair("one_hundred", 100),
      std::make_pair("two_hundred", 200),
      std::make_pair("three_hundred", 300),
      std::make_pair("four_hundred", 400),
  };
  EXPECT_THAT(VariantToJson(Variant(int_map)),
              EqualsJson("{"
                         "  \"one_hundred\": 100,"
                         "  \"two_hundred\": 200,"
                         "  \"three_hundred\": 300,"
                         "  \"four_hundred\": 400"
                         "}"));
  std::map<Variant, Variant> mixed_map{
      std::make_pair("int_value", 100),
      std::make_pair("boolean_value", true),
      std::make_pair("double_value", 3.5),
      std::make_pair("string_value", "Good-bye, World!"),
  };
  EXPECT_THAT(VariantToJson(mixed_map),
              EqualsJson("{"
                         "  \"int_value\": 100,"
                         "  \"boolean_value\": true,"
                         "  \"double_value\": 3.5,"
                         "  \"string_value\": \"Good-bye, World!\""
                         "}"));
  std::map<Variant, Variant> nested_map{
      std::make_pair("int_map", int_map),
      std::make_pair("mixed_map", mixed_map),
  };
  EXPECT_THAT(VariantToJson(nested_map),
              EqualsJson("{"
                         "  \"int_map\":{"
                         "    \"one_hundred\": 100,"
                         "    \"two_hundred\": 200,"
                         "    \"three_hundred\": 300,"
                         "    \"four_hundred\": 400"
                         "  },"
                         "  \"mixed_map\":{"
                         "    \"int_value\": 100,"
                         "    \"boolean_value\": true,"
                         "    \"double_value\": 3.5,"
                         "    \"string_value\": \"Good-bye, World!\""
                         "  }"
                         "}"));

  // Test pretty printing with one key per map, since key order may vary.
  std::map<Variant, Variant> nested_one_key_map{
      std::make_pair("a",
                     std::vector<Variant>{
                         1, 2,
                         std::map<Variant, Variant>{
                             std::make_pair("b", std::vector<Variant>{3, 4}),
                         }}),
  };
  EXPECT_THAT(VariantToJson(Variant(nested_one_key_map), true),
              StrEq("{\n"
                    "  \"a\": [\n"
                    "    1,\n"
                    "    2,\n"
                    "    {\n"
                    "      \"b\": [\n"
                    "        3,\n"
                    "        4\n"
                    "      ]\n"
                    "    }\n"
                    "  ]\n"
                    "}"));
}

TEST(UtilDesktopTest, VariantToJsonMapLegalNonStringKeys) {
  // VariantToJson will convert fundamental types to strings.
  std::map<Variant, Variant> int_key_map{
      std::make_pair(100, "one_hundred"),
      std::make_pair(200, "two_hundred"),
      std::make_pair(300, "three_hundred"),
      std::make_pair(400, "four_hundred"),
  };
  EXPECT_THAT(VariantToJson(Variant(int_key_map)),
              EqualsJson("{"
                         "  \"100\": \"one_hundred\","
                         "  \"200\": \"two_hundred\","
                         "  \"300\": \"three_hundred\","
                         "  \"400\": \"four_hundred\""
                         "}"));
  std::map<Variant, Variant> mixed_key_map{
      std::make_pair(100, "int_value"),
      std::make_pair(3.5, "double_value"),
      std::make_pair(true, "boolean_value"),
      std::make_pair("Good-bye, World!", "string_value"),
  };
  EXPECT_THAT(VariantToJson(mixed_key_map),
              EqualsJson("{"
                         "  \"100\": \"int_value\","
                         "  \"3.5000000000000000\": \"double_value\","
                         "  \"true\": \"boolean_value\","
                         "  \"Good-bye, World!\": \"string_value\""
                         "}"));
}

TEST(UtilDesktopTest, VariantToJsonMapWithBadKeys) {
  // JSON only supports strings for keys (and this implmentation will coerce
  // fundamental types to string keys. Anything else (containers, blobs)
  // should fail, which is represented by an empty string. Also, the empty
  // string is not valid JSON, so we must test with StrEq instead of
  // JsonEquals.

  // Vector as a key.
  std::vector<Variant> int_vector{1, 2, 3, 4};
  std::map<Variant, Variant> map_with_vector_key{
      std::make_pair(int_vector, "pairs of numbers!"),
  };
  EXPECT_THAT(VariantToJson(Variant(map_with_vector_key)), StrEq(""));

  // Map as a key.
  std::map<Variant, Variant> int_map{
      std::make_pair(1, 1),
      std::make_pair(2, 3),
      std::make_pair(5, 8),
      std::make_pair(13, 21),
  };
  std::map<Variant, Variant> map_with_map_key{
      std::make_pair(int_map, "pairs of numbers!"),
  };
  EXPECT_THAT(VariantToJson(Variant(map_with_map_key)), StrEq(""));

  std::string blob_data = "abcdefghijklmnopqrstuvwxyz";

  // Static blob as a key.
  Variant static_blob =
      Variant::FromStaticBlob(blob_data.c_str(), blob_data.size());
  std::map<Variant, Variant> map_with_static_blob_key{
      std::make_pair(static_blob, "blobby blob blob"),
  };
  EXPECT_THAT(VariantToJson(Variant(map_with_static_blob_key)), StrEq(""));

  // Mutable blob as a key.
  Variant mutable_blob =
      Variant::FromMutableBlob(blob_data.c_str(), blob_data.size());
  std::map<Variant, Variant> map_with_mutable_blob_key{
      std::make_pair(static_blob, "blorby blorb blorb"),
  };
  EXPECT_THAT(VariantToJson(Variant(map_with_mutable_blob_key)), StrEq(""));

  // Legal top level map with illegal nested values.
  std::map<Variant, Variant> map_with_legal_key{
      std::make_pair("totes legal", map_with_map_key)};
  EXPECT_THAT(VariantToJson(Variant(map_with_legal_key)), StrEq(""));
}

TEST(UtilDesktopTest, VariantToJsonWithStaticBlob) {
  // Static blobs are not supported, so we expect these to fail, which is
  // represented by an empty string.
  std::string blob_data = "abcdefghijklmnopqrstuvwxyz";
  Variant blob = Variant::FromStaticBlob(blob_data.c_str(), blob_data.size());
  EXPECT_THAT(VariantToJson(blob), StrEq(""));
  std::vector<Variant> blob_vector{1, true, 3.5, "hello", blob};
  EXPECT_THAT(VariantToJson(blob_vector), StrEq(""));
  std::map<Variant, Variant> blob_map{
      std::make_pair("int_value", 100),
      std::make_pair("boolean_value", true),
      std::make_pair("double_value", 3.5),
      std::make_pair("string_value", "Good-bye, World!"),
      std::make_pair("blob_value", blob),
  };
  EXPECT_THAT(VariantToJson(blob_map), StrEq(""));
}

TEST(UtilDesktopTest, VariantToJsonWithMutableBlob) {
  // Mutable blobs are not supported, so we expect these to fail, which is
  // represented by an empty string.
  std::string blob_data = "abcdefghijklmnopqrstuvwxyz";
  Variant blob = Variant::FromMutableBlob(blob_data.c_str(), blob_data.size());
  EXPECT_THAT(VariantToJson(blob), StrEq(""));
  std::vector<Variant> blob_vector{1, true, 3.5, "hello", blob};
  EXPECT_THAT(VariantToJson(blob_vector), StrEq(""));
  std::map<Variant, Variant> blob_map{
      std::make_pair("int_value", 100),
      std::make_pair("boolean_value", true),
      std::make_pair("double_value", 3.5),
      std::make_pair("string_value", "Good-bye, World!"),
      std::make_pair("blob_value", blob),
  };
  EXPECT_THAT(VariantToJson(blob_map), StrEq(""));
}

TEST(UtilDesktopTest, VariantToFlexbufferNull) {
  EXPECT_TRUE(GetRoot(VariantToFlexbuffer(Variant::Null())).IsNull());
}

TEST(UtilDesktopTest, VariantToFlexbufferInt64) {
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(0)).AsInt32(), Eq(0));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(100)).AsInt32(), Eq(100));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(int64_t(8000000000L))).AsInt64(),
              Eq(8000000000));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(-100)).AsInt32(), Eq(-100));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(int64_t(-8000000000L))).AsInt64(),
              Eq(-8000000000));
}

TEST(UtilDesktopTest, VariantToFlexbufferDouble) {
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(0.0)).AsDouble(), Eq(0.0));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(100.0)).AsDouble(), Eq(100.0));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer(-100.0)).AsDouble(), Eq(-100.0));
}

TEST(UtilDesktopTest, VariantToFlexbufferBool) {
  EXPECT_TRUE(GetRoot(VariantToFlexbuffer(Variant::True())).AsBool());
  EXPECT_FALSE(GetRoot(VariantToFlexbuffer(Variant::False())).AsBool());
}

TEST(UtilDesktopTest, VariantToFlexbufferString) {
  EXPECT_THAT(GetRoot(VariantToFlexbuffer("Hello, World!")).AsString().c_str(),
              StrEq("Hello, World!"));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer("100")).AsString().c_str(),
              StrEq("100"));
  EXPECT_THAT(GetRoot(VariantToFlexbuffer("false")).AsString().c_str(),
              StrEq("false"));
}

TEST(UtilDesktopTest, VariantToFlexbufferVector) {
  flexbuffers::Builder fbb(512);
  fbb.Vector([&]() {});
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(Variant::EmptyVector()),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();

  std::vector<Variant> int_vector{1, 2, 3, 4};
  fbb.Vector([&]() {
    fbb += 1;
    fbb += 2;
    fbb += 3;
    fbb += 4;
  });
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(int_vector),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();

  std::vector<Variant> mixed_vector{1, true, 3.5, "hello"};
  fbb.Vector([&]() {
    fbb += 1;
    fbb += true;
    fbb += 3.5;
    fbb += "hello";
  });
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(mixed_vector),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();

  std::vector<Variant> nested_vector{1, true, 3.5, "hello", int_vector};
  fbb.Vector([&]() {
    fbb += 1;
    fbb += true;
    fbb += 3.5;
    fbb += "hello";
    fbb.Vector([&]() {
      fbb += 1;
      fbb += 2;
      fbb += 3;
      fbb += 4;
    });
  });
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(nested_vector),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();
}

TEST(UtilDesktopTest, VariantToFlexbufferMapWithStringKeys) {
  flexbuffers::Builder fbb(512);
  fbb.Map([&]() {});
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(Variant::EmptyMap()),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();

  std::map<Variant, Variant> int_map{
      std::make_pair("one_hundred", 100),
      std::make_pair("two_hundred", 200),
      std::make_pair("three_hundred", 300),
      std::make_pair("four_hundred", 400),
  };
  fbb.Map([&]() {
    fbb.Add("one_hundred", 100);
    fbb.Add("two_hundred", 200);
    fbb.Add("three_hundred", 300);
    fbb.Add("four_hundred", 400);
  });
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(int_map), EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();

  std::map<Variant, Variant> mixed_map{
      std::make_pair("int_value", 100),
      std::make_pair("boolean_value", true),
      std::make_pair("double_value", 3.5),
      std::make_pair("string_value", "Good-bye, World!"),
  };
  fbb.Map([&]() {
    fbb.Add("int_value", 100);
    fbb.Add("boolean_value", true);
    fbb.Add("double_value", 3.5);
    fbb.Add("string_value", "Good-bye, World!");
  });
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(mixed_map),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();

  std::map<Variant, Variant> nested_map{
      std::make_pair("int_map", int_map),
      std::make_pair("mixed_map", mixed_map),
  };
  fbb.Map([&]() {
    fbb.Map("int_map", [&]() {
      fbb.Add("one_hundred", 100);
      fbb.Add("two_hundred", 200);
      fbb.Add("three_hundred", 300);
      fbb.Add("four_hundred", 400);
    });
    fbb.Map("mixed_map", [&]() {
      fbb.Add("int_value", 100);
      fbb.Add("boolean_value", true);
      fbb.Add("double_value", 3.5);
      fbb.Add("string_value", "Good-bye, World!");
    });
  });
  fbb.Finish();
  EXPECT_THAT(VariantToFlexbuffer(nested_map),
              EqualsFlexbuffer(fbb.GetBuffer()));
  fbb.Clear();
}

}  // namespace
