// Copyright 2018 Google LLC
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

#include <fstream>
#include <sstream>

#include "app/memory/unique_ptr.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/util_desktop.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;
using ::testing::Ne;

namespace firebase {
namespace database {
namespace internal {

typedef std::vector<std::pair<std::string, std::string>> TestList;

// Hardcoded Json string for test uses \' instead of \" for readability.
// This utility function converts \' into \"
std::string& ConvertQuote(std::string* in) {
  std::replace(in->begin(), in->end(), '\'', '\"');
  return *in;
}

// Test for ConvertQuote
TEST(IndexedVariantHelperFunction, ConvertQuote) {
  {
    std::string test_string = "";
    EXPECT_THAT(ConvertQuote(&test_string), Eq(""));
  }
  {
    std::string test_string = "'";
    EXPECT_THAT(ConvertQuote(&test_string), Eq("\""));
  }
  {
    std::string test_string = "\"";
    EXPECT_THAT(ConvertQuote(&test_string), Eq("\""));
  }
  {
    std::string test_string = "''";
    EXPECT_THAT(ConvertQuote(&test_string), Eq("\"\""));
  }
  {
    std::string test_string = "{'A':'a'}";
    EXPECT_THAT(ConvertQuote(&test_string), Eq("{\"A\":\"a\"}"));
  }
}

std::string QueryParamsToString(const QueryParams& params) {
  std::stringstream ss;

  ss << "{ order_by=";
  switch (params.order_by) {
    case QueryParams::kOrderByPriority:
      ss << "kOrderByPriority";
      break;
    case QueryParams::kOrderByKey:
      ss << "kOrderByKey";
      break;
    case QueryParams::kOrderByValue:
      ss << "kOrderByValue";
      break;
    case QueryParams::kOrderByChild:
      ss << "kOrderByChild(" << params.order_by_child << ")";
      break;
  }

  if (!params.equal_to_value.is_null()) {
    ss << ", equal_to_value=" << util::VariantToJson(params.equal_to_value);
  }
  if (!params.equal_to_child_key.empty()) {
    ss << ", equal_to_child_key=" << params.equal_to_child_key;
  }
  if (!params.start_at_value.is_null()) {
    ss << ", start_at_value=" << util::VariantToJson(params.start_at_value);
  }
  if (!params.start_at_child_key.empty()) {
    ss << ", start_at_child_key=" << params.start_at_child_key;
  }
  if (!params.end_at_value.is_null()) {
    ss << ", end_at_value=" << util::VariantToJson(params.end_at_value);
  }
  if (!params.end_at_child_key.empty()) {
    ss << ", end_at_child_key=" << params.end_at_child_key;
  }
  if (params.limit_first != 0) {
    ss << ", limit_first=" << params.limit_first;
  }
  if (params.limit_last != 0) {
    ss << ", limit_last=" << params.limit_last;
  }
  ss << " }";

  return ss.str();
}

// Validate the index created by IndexedVariant and its order
void VerifyIndex(const Variant* input_variant,
                 const QueryParams* input_query_params, TestList* expected) {
  // IndexedVariant supports 4 types of constructor:
  //   IndexedVariant() - both input_variant and input_query_params are null
  //   IndexedVariant(Variant) - only input_query_params is null
  //   IndexedVariant(Variant, QueryParams) - both are NOT null
  // Additionally, we test the copy constructor in all cases
  //   IndexedVariant(IndexedVariant) - A copy of an existing IndexedVariant
  UniquePtr<IndexedVariant> index_variant;
  if (input_variant == nullptr && input_query_params == nullptr) {
    index_variant = MakeUnique<IndexedVariant>();
  } else if (input_variant != nullptr && input_query_params == nullptr) {
    index_variant = MakeUnique<IndexedVariant>(*input_variant);
  } else if (input_variant != nullptr && input_query_params != nullptr) {
    index_variant =
        MakeUnique<IndexedVariant>(*input_variant, *input_query_params);
  }

  // assert if input_variant is null but input_query_params is not null
  assert(index_variant);
  IndexedVariant copied_index_variant(*index_variant);

  const IndexedVariant::Index* indexes[] = {
      &index_variant->index(),
      &copied_index_variant.index(),
  };
  for (const auto& index : indexes) {
    // Convert TestList::index() into TestList for comparison
    TestList actual_list;
    for (auto& it : *index) {
      actual_list.push_back(
          {it.first.AsString().string_value(), util::VariantToJson(it.second)});
    }

    for (auto& it : *expected) {
      // Make sure Json string is formatted in the same way since we are doing
      // string comparison.
      ConvertQuote(&it.second);
      it.second = util::VariantToJson(util::JsonToVariant(it.second.c_str()));
    }

    EXPECT_THAT(actual_list, Eq(*expected))
        << "Test Variant: " << util::VariantToJson(*input_variant) << std::endl
        << "Test QueryParams: "
        << (input_query_params ? QueryParamsToString(*input_query_params)
                               : "null");
  }
}

// Default IndexedVariant
TEST(IndexedVariant, ConstructorTestBasic) {
  TestList expected_result = {};
  VerifyIndex(nullptr, nullptr, &expected_result);
}

TEST(IndexedVariant, ConstructorTestDefaultQueryParamsNoPriority) {
  {
    Variant test_input = Variant::Null();
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    Variant test_input(123);
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    Variant test_input(123.456);
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    Variant test_input(true);
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    Variant test_input(false);
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    std::string variant_string = "[1,2,3]";
    Variant test_input =
        util::JsonToVariant(ConvertQuote(&variant_string).c_str());
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    std::string variant_string = "{}";
    Variant test_input =
        util::JsonToVariant(ConvertQuote(&variant_string).c_str());
    TestList expected_result = {};
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    std::string variant_string =
        "{"
        "  'A': 1,"
        "  'B': 'b',"
        "  'C':true"
        "}";
    Variant test_input =
        util::JsonToVariant(ConvertQuote(&variant_string).c_str());
    TestList expected_result = {
        {"A", "1"},
        {"B", "'b'"},
        {"C", "true"},
    };
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    std::string variant_string =
        "{"
        "  'A': 1,"
        "  'B': { '.value': 'b', '.priority': 100 },"
        "  'C': true"
        "}";
    Variant test_input =
        util::JsonToVariant(ConvertQuote(&variant_string).c_str());
    TestList expected_result = {
        {"A", "1"},
        {"C", "true"},
        {"B", "{ '.value': 'b', '.priority': 100 }"},
    };
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
  {
    std::string variant_string =
        "{"
        "  'A': { '.value': 1, '.priority': 300 },"
        "  'B': { '.value': 'b', '.priority': 100 },"
        "  'C': { '.value': true, '.priority': 200 }"
        "}";
    Variant test_input =
        util::JsonToVariant(ConvertQuote(&variant_string).c_str());
    TestList expected_result = {
        {"B", "{ '.value': 'b', '.priority': 100 }"},
        {"C", "{ '.value': true, '.priority': 200 }"},
        {"A", "{ '.value': 1, '.priority': 300 }"},
    };
    VerifyIndex(&test_input, nullptr, &expected_result);
  }
}

// Used to run individual test for GetOrderByVariantTest
// Need to access private function IndexedVariant::GetOrderByVariant().
// Therefore this class is friended by IndexedVariant
class IndexedVariantGetOrderByVariantTest : public ::testing::Test {
 protected:
  void RunTest(const QueryParams& params, const Variant& key,
               const TestList& value_result_list, const char* test_name) {
    IndexedVariant indexed_variant(Variant::Null(), params);

    for (auto& test : value_result_list) {
      std::string value_string = test.first;
      Variant value = util::JsonToVariant(ConvertQuote(&value_string).c_str());
      bool expected_null = test.second.empty();
      std::string expected_string = test.second;
      Variant expected =
          util::JsonToVariant(ConvertQuote(&expected_string).c_str());

      auto* result = indexed_variant.GetOrderByVariant(key, value);
      EXPECT_THAT(!result || result->is_null(), Eq(expected_null))
          << test_name << " (" << key.AsString().string_value() << ", "
          << value_string << ") ";
      if (!expected_null && result != nullptr) {
        EXPECT_THAT(*result, Eq(expected))
            << test_name << " (" << key.AsString().string_value() << ", "
            << value_string << ") ";
      }
    }
  }
};

TEST_F(IndexedVariantGetOrderByVariantTest, GetOrderByVariantTest) {
  // Test order by priority
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByPriority;

    const Variant key("A");
    // List of test: { value, expected}
    TestList value_result_list = {
        {"1", ""},
        {"{'.value': 1, '.priority': 100}", "100"},
        {"{'B': 1,'.priority': 100}", "100"},
        {"{'B': {'.value': 1, '.priority': 200} ,'.priority': 100}", "100"},
        {"{'B': {'C': 1, '.priority': 200} ,'.priority': 100}", "100"},
    };

    RunTest(params, key, value_result_list, "OrderByPriority");
  }

  // Test order by key
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;

    const Variant key("A");
    // List of test: { value, expected}
    TestList value_result_list = {
        {"1", "'A'"},
        {"{'.value': 1, '.priority': 100}", "'A'"},
        {"{'B': 1,'.priority': 100}", "'A'"},
        {"{'B': {'.value': 1, '.priority': 200} ,'.priority': 100}", "'A'"},
        {"{'B': {'C': 1, '.priority': 200} ,'.priority': 100}", "'A'"},
    };

    RunTest(params, key, value_result_list, "OrderByKey");
  }

  // Test order by value
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    IndexedVariant indexed_variant(Variant::Null(), params);
    const Variant key("A");
    // List of test: { value, expected}
    TestList value_result_list = {
        {"1", "1"},
        {"{'.value': 1, '.priority': 100}", "1"},
    };

    RunTest(params, key, value_result_list, "OrderByValue");
  }

  // Test order by child
  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.order_by_child = "B";
    IndexedVariant indexed_variant(Variant::Null(), params);
    const Variant key("A");
    // List of test: { value, expected}
    TestList value_result_list = {
        {"1", ""},
        {"{'.value': 1, '.priority': 100}", ""},
        {"{'B': 1,'.priority': 100}", "1"},
        {"{'B': {'.value': 1, '.priority': 200} ,'.priority': 100}", "1"},
    };

    RunTest(params, key, value_result_list, "OrderByChild");
  }
}

TEST(IndexedVariant, FindTest) {
  std::string test_data =
      "{"
      "  'A': 1,"
      "  'B': 'b',"
      "  'C': true"
      "}";
  Variant variant = util::JsonToVariant(ConvertQuote(&test_data).c_str());

  IndexedVariant indexed_variant(variant);

  // List of test: { key, expected}
  TestList test_list = {
      {"A", "A"},
      {"B", "B"},
      {"C", "C"},
      {"D", ""},
  };

  for (auto& test : test_list) {
    auto it = indexed_variant.Find(Variant(test.first));

    bool expected_found = !test.second.empty();
    EXPECT_THAT(it != indexed_variant.index().end(), Eq(expected_found))
        << "Find(" << test.first << ")";

    if (expected_found && it != indexed_variant.index().end()) {
      EXPECT_THAT(it->first, Eq(Variant(test.second)))
          << "Find(" << test.first << ")";
    }
  }
}

TEST(IndexedVariant, GetPredecessorChildNameTest) {
  std::string test_data =
      "{"
      "    'A': { '.value': 1, '.priority': 300 },"
      "    'B': { '.value': 'b', '.priority': 100 },"
      "    'C': { '.value': true, '.priority': 200 },"
      "    'D': { 'E': {'.value': 'e', '.priority': 200}, '.priority': 100 }"
      "}";

  // Expected Order (Order by priority by default)
  //   ["B", { ".value": "b", ".priority": 100 } ],
  //   ["D", { "E" : {".value": "e", ".priority": 200 }, ".priority": 100 } ],
  //   ["C", { ".value": true, ".priority": 200 } ],
  //   ["A", { ".value": 1, ".priority": 300 } ]
  Variant variant = util::JsonToVariant(ConvertQuote(&test_data).c_str());

  // Use default QueryParams which uses OrderByPriority
  IndexedVariant indexed_variant(variant);

  struct TestCase {
    // Input key string
    std::string key;

    // Input value variant, structured in Json string, with \" replaced for
    // readibility.
    std::string value;

    // Expected return value from GetPredecessorChildName().  If it is empty
    // string (""), then the expected return value is nullptr
    std::string expected_result;
  };

  std::vector<TestCase> test_list = {
      {"A", "{ '.value': 1, '.priority': 300 }", "C"},
      // The first element, no predecessor
      {"B", "{ '.value': 'b', '.priority': 100 }", ""},
      {"C", "{ '.value': true, '.priority': 200 }", "D"},
      {"D", "{ 'E': {'.value': 'e', '.priority': 200}, '.priority': 100 }",
       "B"},
      // Pair not found
      {"E", "'e'", ""},
      // EXCEPTION: Not found due to missing priority.
      {"A", "1", ""},
      {"B", "'b'", ""},
      {"C", "true", ""},
      {"D", "{ 'E': {'.value': 'e', '.priority': 200}}", ""},
      {"D", "{ 'E': 'e'}}", ""},
      // EXCEPTION: Not found because priority is different
      {"A", "{ '.value': 1, '.priority': 1000 }", ""},
      // EXCEPTION: Found because, even though the value is different, the
      // priority is the same.
      {"A", "{ '.value': 'a', '.priority': 300 }", "C"},
  };

  for (auto& test : test_list) {
    std::string& key = test.key;
    Variant value = util::JsonToVariant(ConvertQuote(&test.value).c_str());
    const char* child_name =
        indexed_variant.GetPredecessorChildName(key, value);

    std::string& expected = test.expected_result;
    bool expected_found = !expected.empty();
    EXPECT_THAT(child_name != nullptr, Eq(expected_found))
        << "GetPredecessorChildNameTest(" << key << ", " << test.value << ")";

    if (expected_found && child_name != nullptr) {
      EXPECT_THAT(std::string(child_name), Eq(expected))
          << "GetPredecessorChildNameTest(" << key << ", " << test.value << ")";
    }
  }
}

TEST(IndexedVariant, Variant) {
  Variant variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
  };
  QueryParams params;
  IndexedVariant indexed_variant(variant, params);
  Variant expected = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
  };
  EXPECT_EQ(indexed_variant.variant(), expected);
}

TEST(IndexedVariant, UpdateChildTest) {
  Variant variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
  };

  IndexedVariant indexed_variant(variant);

  // Add new element.
  IndexedVariant result1 = indexed_variant.UpdateChild("eee", 500);
  // Change existing element.
  IndexedVariant result2 = indexed_variant.UpdateChild("ccc", 600);
  // Remove existing element.
  IndexedVariant result3 = indexed_variant.UpdateChild("bbb", Variant::Null());

  Variant expected1 = std::map<Variant, Variant>{
      std::make_pair("aaa", 100), std::make_pair("bbb", 200),
      std::make_pair("ccc", 300), std::make_pair("ddd", 400),
      std::make_pair("eee", 500),
  };
  Variant expected2 = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 600),
      std::make_pair("ddd", 400),
  };
  Variant expected3 = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("ccc", 300),
      std::make_pair("ddd", 400),
  };
  EXPECT_EQ(result1.variant(), expected1);
  EXPECT_EQ(result2.variant(), expected2);
  EXPECT_EQ(result3.variant(), expected3);
}

TEST(IndexedVariant, UpdatePriorityTest) {
  Variant variant = 100;
  IndexedVariant indexed_variant(variant);

  IndexedVariant result = indexed_variant.UpdatePriority(1234);
  Variant expected = std::map<Variant, Variant>{
      std::make_pair(".value", 100),
      std::make_pair(".priority", 1234),
  };

  EXPECT_EQ(result.variant(), expected);
}

TEST(IndexedVariant, GetFirstAndLastChildByPriority) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByPriority;
  Variant variant = std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{std::make_pair(".priority", 3),
                                                std::make_pair(".value", 100)}),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{std::make_pair(".priority", 4),
                                                std::make_pair(".value", 200)}),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{std::make_pair(".priority", 1),
                                                std::make_pair(".value", 300)}),
      std::make_pair("ddd",
                     std::map<Variant, Variant>{std::make_pair(".priority", 2),
                                                std::make_pair(".value", 400)}),
  };
  IndexedVariant indexed_variant(variant, params);
  Optional<std::pair<Variant, Variant>> expected_first(std::make_pair(
      "ccc", std::map<Variant, Variant>{std::make_pair(".priority", 1),
                                        std::make_pair(".value", 300)}));
  Optional<std::pair<Variant, Variant>> expected_last(std::make_pair(
      "bbb", std::map<Variant, Variant>{std::make_pair(".priority", 4),
                                        std::make_pair(".value", 200)}));
  EXPECT_EQ(indexed_variant.GetFirstChild(), expected_first);
  EXPECT_EQ(indexed_variant.GetLastChild(), expected_last);
}

TEST(IndexedVariant, GetFirstAndLastChildByChild) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  params.order_by_child = "zzz";
  Variant variant = std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{std::make_pair("zzz", 2)}),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{std::make_pair("zzz", 1)}),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{std::make_pair("zzz", 4)}),
      std::make_pair("ddd",
                     std::map<Variant, Variant>{std::make_pair("zzz", 3)}),
  };
  IndexedVariant indexed_variant(variant, params);
  Optional<std::pair<Variant, Variant>> expected_first(std::make_pair(
      "bbb", std::map<Variant, Variant>{std::make_pair("zzz", 1)}));
  Optional<std::pair<Variant, Variant>> expected_last(std::make_pair(
      "ccc", std::map<Variant, Variant>{std::make_pair("zzz", 4)}));
  EXPECT_EQ(indexed_variant.GetFirstChild(), expected_first);
  EXPECT_EQ(indexed_variant.GetLastChild(), expected_last);
}

TEST(IndexedVariant, GetFirstAndLastChildByKey) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  Variant variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 400),
      std::make_pair("bbb", 300),
      std::make_pair("ccc", 200),
      std::make_pair("ddd", 100),
  };
  IndexedVariant indexed_variant(variant, params);
  Optional<std::pair<Variant, Variant>> expected_first(
      std::make_pair("aaa", 400));
  Optional<std::pair<Variant, Variant>> expected_last(
      std::make_pair("ddd", 100));
  EXPECT_EQ(indexed_variant.GetFirstChild(), expected_first);
  EXPECT_EQ(indexed_variant.GetLastChild(), expected_last);
}

TEST(IndexedVariant, GetFirstAndLastChildByValue) {
  // Value
  QueryParams params;
  params.order_by = QueryParams::kOrderByValue;
  Variant variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 400),
      std::make_pair("bbb", 300),
      std::make_pair("ccc", 200),
      std::make_pair("ddd", 100),
  };
  IndexedVariant indexed_variant(variant, params);
  Optional<std::pair<Variant, Variant>> expected_first(
      std::make_pair("ddd", 100));
  Optional<std::pair<Variant, Variant>> expected_last(
      std::make_pair("aaa", 400));
  EXPECT_EQ(indexed_variant.GetFirstChild(), expected_first);
  EXPECT_EQ(indexed_variant.GetLastChild(), expected_last);
}

TEST(IndexedVariant, GetFirstAndLastChildLeaf) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByValue;
  Variant variant = 1000;
  IndexedVariant indexed_variant(variant, params);
  Optional<std::pair<Variant, Variant>> expected_first;
  Optional<std::pair<Variant, Variant>> expected_last;
  EXPECT_EQ(indexed_variant.GetFirstChild(), expected_first);
  EXPECT_EQ(indexed_variant.GetLastChild(), expected_last);
}

TEST(IndexedVariant, EqualityOperatorSame) {
  Variant variant(static_cast<int>(3141592654));
  QueryParams params;
  IndexedVariant indexed_variant(variant, params);
  IndexedVariant identical_indexed_variant(variant, params);

  // Verify the == and != operators return the expected result.
  // Check equality with self.
  EXPECT_TRUE(indexed_variant == indexed_variant);
  EXPECT_FALSE(indexed_variant != indexed_variant);

  // Check equality with identical change.
  EXPECT_TRUE(indexed_variant == identical_indexed_variant);
  EXPECT_FALSE(indexed_variant != identical_indexed_variant);
}

TEST(IndexedVariant, EqualityOperatorDifferent) {
  Variant variant(static_cast<int>(3141592654));
  QueryParams params;
  params.order_by = QueryParams::kOrderByKey;
  IndexedVariant indexed_variant(variant, params);

  Variant different_variant(static_cast<int>(2718281828));
  QueryParams different_params;
  different_params.order_by = QueryParams::kOrderByChild;
  IndexedVariant indexed_variant_different_variant(different_variant, params);
  IndexedVariant indexed_variant_different_params(variant, different_params);
  IndexedVariant indexed_variant_different_both(different_variant,
                                                different_params);

  // Verify the == and != operators return the expected result.
  EXPECT_FALSE(indexed_variant == indexed_variant_different_variant);
  EXPECT_TRUE(indexed_variant != indexed_variant_different_variant);

  EXPECT_FALSE(indexed_variant == indexed_variant_different_params);
  EXPECT_TRUE(indexed_variant != indexed_variant_different_params);

  EXPECT_FALSE(indexed_variant == indexed_variant_different_both);
  EXPECT_TRUE(indexed_variant != indexed_variant_different_both);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
