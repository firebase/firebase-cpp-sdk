/*
 * Copyright 2016 Google LLC
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

#include "app/src/include/firebase/variant.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace firebase {
namespace internal {
class VariantInternal {
 public:
  static constexpr uint32_t kInternalTypeSmallString =
      Variant::kInternalTypeSmallString;

  static uint32_t type(const Variant& v) {
    return v.type_;
  }
};
}  // namespace internal
}  // namespace firebase

using firebase::internal::VariantInternal;

namespace firebase {
namespace testing {

const int64_t kTestInt64 = 12345L;
const char* kTestString = "Hello, world!";
const std::string kTestSmallString = "<eight"; // NOLINT
// Note: Mutable string needs to be bigger than the small string optimisation
const std::string kTestMutableString =  // NOLINT
    "I am just great, thanks for asking!";
const double kTestDouble = 3.1415926535;
const bool kTestBool = true;
// NOLINTNEXTLINE
const std::vector<Variant> kTestVector = {int64_t(1L), "one", true, 1.0};
// NOLINTNEXTLINE
const std::vector<Variant> kTestComplexVector = {int64_t(2L), "two",
                                                 kTestVector, false, 2.0};
const uint8_t kTestBlobData[] = {89, 0, 65, 198, 4, 99, 0, 9};
const size_t kTestBlobSize = sizeof(kTestBlobData);  // size in bytes
std::map<Variant, Variant> g_test_map;               // NOLINT
std::map<Variant, Variant> g_test_complex_map;       // NOLINT

class VariantTest : public ::testing::Test {
 protected:
  VariantTest() {}
  void SetUp() override {
    g_test_map.clear();
    g_test_map["first"] = 101;
    g_test_map["second"] = 202.2;
    g_test_map["third"] = "three";

    g_test_complex_map.clear();
    g_test_complex_map["one"] = kTestString;
    g_test_complex_map[2] = 123;
    g_test_complex_map[3.0] =
        Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
    g_test_complex_map[kTestVector] = kTestComplexVector;
    g_test_complex_map[std::string("five")] = g_test_map;
    g_test_complex_map[Variant::FromMutableBlob(kTestBlobData, kTestBlobSize)] =
        kTestMutableString;
  }
};

TEST_F(VariantTest, TestScalarTypes) {
  {
    Variant v;
    EXPECT_THAT(v.type(), Eq(Variant::kTypeNull));
    EXPECT_TRUE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
  {
    Variant v(kTestInt64);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(v.int64_value(), Eq(kTestInt64));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
  {
    // Ensure that 0 comes through as an integer, not a bool.
    Variant v(0);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(v.int64_value(), Eq(0));
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
  {
    Variant v(kTestString);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v.string_value(), Eq(kTestString));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
  {
    Variant v(kTestSmallString);
    EXPECT_THAT(VariantInternal::type(v),
                Eq(VariantInternal::kInternalTypeSmallString));
    EXPECT_THAT(v.string_value(), Eq(kTestSmallString));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());

    // Should be able to upgrade to mutable string
    EXPECT_THAT(v.mutable_string(), Eq(kTestSmallString));
    EXPECT_THAT(v.type(), Eq(Variant::kTypeMutableString));
  }
  {
    Variant v(kTestMutableString);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v.mutable_string(), Eq(kTestMutableString));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
  {
    Variant v(kTestBool);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeBool));
    EXPECT_THAT(v.bool_value(), Eq(kTestBool));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
  {
    Variant v(kTestDouble);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(v.double_value(), Eq(kTestDouble));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_fundamental_type());
    EXPECT_FALSE(v.is_container_type());
  }
}

TEST_F(VariantTest, TestInvalidTypeAsserts1) {
  {
    Variant v;
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.string_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.mutable_string(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
  {
    Variant v(kTestInt64);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.string_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.mutable_string(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
  {
    Variant v(kTestDouble);
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.string_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.mutable_string(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
  {
    Variant v(kTestBool);
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.string_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.mutable_string(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
}

TEST_F(VariantTest, TestInvalidTypeAsserts2) {
  {
    Variant v(kTestString);
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
  {
    Variant v(kTestMutableString);
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
  {
    Variant v(kTestVector);
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.string_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.mutable_string(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.map(), DEATHTEST_SIGABRT);
  }
  {
    Variant v(g_test_map);
    EXPECT_DEATH(v.int64_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.double_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.bool_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.string_value(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.mutable_string(), DEATHTEST_SIGABRT);
    EXPECT_DEATH(v.vector(), DEATHTEST_SIGABRT);
  }
}

TEST_F(VariantTest, TestMutableStringPromotion) {
  Variant v("Hello!");
  EXPECT_THAT(v.type(), Eq(Variant::kTypeStaticString));
  EXPECT_THAT(v.string_value(), StrEq("Hello!"));
  (void)v.mutable_string();
  EXPECT_THAT(v.type(), Eq(Variant::kTypeMutableString));
  EXPECT_THAT(v.mutable_string(), StrEq("Hello!"));
  EXPECT_THAT(v.string_value(), StrEq("Hello!"));
  v.mutable_string()[5] = '?';
  EXPECT_THAT(v.mutable_string(), StrEq("Hello?"));
  EXPECT_THAT(v.string_value(), StrEq("Hello?"));
  v.set_string_value("Goodbye.");
  EXPECT_THAT(v.type(), Eq(Variant::kTypeStaticString));
  EXPECT_THAT(v.string_value(), StrEq("Goodbye."));
}

TEST_F(VariantTest, TestSmallString) {
  std::string max_small_str;

  if (sizeof(void*) == 8) {
    max_small_str = "1234567812345678";  // 16 bytes on x64
  } else {
    max_small_str = "12345678";  // 8 bytes on x32
  }

  std::string small_str = max_small_str;
  small_str.pop_back();  // Make room for the trailing \0.

  // Test construction from std::string
  Variant v1(small_str);
  EXPECT_THAT(VariantInternal::type(v1),
              Eq(VariantInternal::kInternalTypeSmallString));
  EXPECT_THAT(v1.string_value(), StrEq(small_str.c_str()));

  // Test copy constructor
  Variant v1c(v1);
  EXPECT_THAT(VariantInternal::type(v1c),
              Eq(VariantInternal::kInternalTypeSmallString));
  EXPECT_THAT(v1c.string_value(), StrEq(small_str.c_str()));

#ifdef FIREBASE_USE_MOVE_OPERATORS
  // Test move constructor
  Variant temp(small_str);
  Variant v2(std::move(temp));
  EXPECT_THAT(VariantInternal::type(v2),
              Eq(VariantInternal::kInternalTypeSmallString));
  EXPECT_THAT(v2.string_value(), StrEq(small_str.c_str()));
#endif

  // Test construction of string bigger than max
  Variant v3(max_small_str);
  EXPECT_THAT(v3.type(), Eq(Variant::kTypeMutableString));
  EXPECT_THAT(v3.string_value(), StrEq(max_small_str.c_str()));

  // Copy normal string to ensure type changes to mutable string
  v1 = v3;
  EXPECT_THAT(v1.type(), Eq(Variant::kTypeMutableString));
  EXPECT_THAT(v1.string_value(), StrEq(max_small_str.c_str()));

  // Test set using smaller string
  v1c.set_mutable_string("a");
  EXPECT_THAT(VariantInternal::type(v1c),
              Eq(VariantInternal::kInternalTypeSmallString));
  EXPECT_THAT(v1c.string_value(), StrEq("a"));

  // Test can set small string as mutable
  v1c.set_mutable_string("b", false);
  EXPECT_THAT(v1c.type(), Eq(Variant::kTypeMutableString));
  EXPECT_THAT(v1c.string_value(), StrEq("b"));
}

TEST_F(VariantTest, TestBasicVector) {
  Variant v1(kTestInt64);
  Variant v2(kTestString);
  Variant v3(kTestDouble);
  Variant v4(kTestBool);
  Variant v5(kTestMutableString);
  Variant v(std::vector<Variant>{v1, v2, v3, v4, v5});

  EXPECT_THAT(v.type(), Eq(Variant::kTypeVector));
  EXPECT_TRUE(v.is_container_type());
  EXPECT_FALSE(v.is_fundamental_type());
  EXPECT_THAT(
      v.vector(),
      ElementsAre(
          AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                Property(&Variant::int64_value, Eq(kTestInt64))),
          AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                Property(&Variant::string_value, Eq(kTestString))),
          AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                Property(&Variant::double_value, Eq(kTestDouble))),
          AllOf(Property(&Variant::type, Eq(Variant::kTypeBool)),
                Property(&Variant::bool_value, Eq(kTestBool))),
          AllOf(Property(&Variant::type, Eq(Variant::kTypeMutableString)),
                Property(&Variant::mutable_string, Eq(kTestMutableString)))));
}

TEST_F(VariantTest, TestConstructingVectorViaTemplate) {
  {
    std::vector<int64_t> list{8, 6, 7, 5, 3, 0, 9};
    Variant v(list);
    EXPECT_THAT(
        v.vector(),
        ElementsAre(AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(8))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(6))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(7))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(5))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(3))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(0))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                          Property(&Variant::int64_value, Eq(9)))));
  }
  {
    std::vector<double> list{0, 1.1, 2.2, 3.3, 4};
    Variant v(list);
    EXPECT_THAT(
        v.vector(),
        ElementsAre(AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                          Property(&Variant::double_value, Eq(0))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                          Property(&Variant::double_value, Eq(1.1))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                          Property(&Variant::double_value, Eq(2.2))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                          Property(&Variant::double_value, Eq(3.3))),
                    AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                          Property(&Variant::double_value, Eq(4)))));
  }
  {
    std::vector<const char*> list1 {
      "hello",
      "world",
      "how",
      "are",
      "you with more chars"
    };
    std::vector<std::string> list2 {
      "hello",
      "world",
      "how",
      "are",
      "you with more chars"
    };
    Variant v1(list1), v2(list2);
    EXPECT_THAT(
        v1.vector(),
        ElementsAre(
            AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                  Property(&Variant::string_value, StrEq("hello"))),
            AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                  Property(&Variant::string_value, StrEq("world"))),
            AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                  Property(&Variant::string_value, StrEq("how"))),
            AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                  Property(&Variant::string_value, StrEq("are"))),
            AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                  Property(&Variant::string_value,
                           StrEq("you with more chars")))));
    EXPECT_THAT(
        v2.vector(),
        ElementsAre(
            AllOf(ResultOf(&VariantInternal::type,
                           Eq(VariantInternal::kInternalTypeSmallString)),
                  Property(&Variant::string_value, StrEq("hello"))),
            AllOf(ResultOf(&VariantInternal::type,
                           Eq(VariantInternal::kInternalTypeSmallString)),
                  Property(&Variant::string_value, StrEq("world"))),
            AllOf(ResultOf(&VariantInternal::type,
                           Eq(VariantInternal::kInternalTypeSmallString)),
                  Property(&Variant::string_value, StrEq("how"))),
            AllOf(ResultOf(&VariantInternal::type,
                           Eq(VariantInternal::kInternalTypeSmallString)),
                  Property(&Variant::string_value, StrEq("are"))),
            AllOf(ResultOf(&VariantInternal::type,
                           Eq(Variant::kTypeMutableString)),
                  Property(&Variant::string_value,
                           StrEq("you with more chars")))));

    // Static and mutable strings are considered equal. So these should be
    // equal.
    EXPECT_EQ(v1, v2);
  }
}

TEST_F(VariantTest, TestNestedVectors) {
  Variant v(std::vector<Variant>{
      kTestInt64, std::vector<int>{10, 20, 30, 40, 50},
      std::vector<const char*>{"apples", "oranges", "lemons"},
      std::vector<std::string>{"sneezy", "bashful", "dopey", "doc"},
      std::vector<bool>{true, false, false, true, false}, kTestString,
      std::vector<double>{3.14159, 2.71828, 1.41421, 0}, kTestBool,
      std::vector<Variant>{int64_t(100L), "one hundred", 100.0,
                           std::vector<Variant>{}, Variant(), 0},
      kTestDouble});

  EXPECT_THAT(v.type(), Eq(Variant::kTypeVector));
  EXPECT_THAT(
      v.vector(),
      ElementsAre(
          Property(&Variant::int64_value, Eq(kTestInt64)),
          Property(&Variant::vector,
                   ElementsAre(Property(&Variant::int64_value, Eq(10)),
                               Property(&Variant::int64_value, Eq(20)),
                               Property(&Variant::int64_value, Eq(30)),
                               Property(&Variant::int64_value, Eq(40)),
                               Property(&Variant::int64_value, Eq(50)))),
          Property(
              &Variant::vector,
              ElementsAre(Property(&Variant::string_value, StrEq("apples")),
                          Property(&Variant::string_value, StrEq("oranges")),
                          Property(&Variant::string_value, StrEq("lemons")))),
          Property(
              &Variant::vector,
              ElementsAre(Property(&Variant::string_value, StrEq("sneezy")),
                          Property(&Variant::string_value, StrEq("bashful")),
                          Property(&Variant::string_value, StrEq("dopey")),
                          Property(&Variant::string_value, StrEq("doc")))),
          Property(&Variant::vector,
                   ElementsAre(Property(&Variant::bool_value, Eq(true)),
                               Property(&Variant::bool_value, Eq(false)),
                               Property(&Variant::bool_value, Eq(false)),
                               Property(&Variant::bool_value, Eq(true)),
                               Property(&Variant::bool_value, Eq(false)))),
          Property(&Variant::string_value, Eq(kTestString)),
          Property(&Variant::vector,
                   ElementsAre(Property(&Variant::double_value, Eq(3.14159)),
                               Property(&Variant::double_value, Eq(2.71828)),
                               Property(&Variant::double_value, Eq(1.41421)),
                               Property(&Variant::double_value, Eq(0)))),
          Property(&Variant::bool_value, Eq(kTestBool)),
          Property(&Variant::vector,
                   ElementsAre(
                       Property(&Variant::int64_value, Eq(100L)),
                       Property(&Variant::string_value, StrEq("one hundred")),
                       Property(&Variant::double_value, Eq(100.0)),
                       Property(&Variant::vector, IsEmpty()),
                       Property(&Variant::is_null, Eq(true)),
                       Property(&Variant::int64_value, Eq(0)))),
          Property(&Variant::double_value, Eq(kTestDouble))));
}

TEST_F(VariantTest, TestBasicMap) {
  {
    // Map of strings to Variant.
    std::map<Variant, Variant> m;
    m["hello"] = kTestInt64;
    m["world"] = kTestString;
    m["how"] = kTestDouble;
    m["are"] = kTestBool;
    m["you"] = Variant();
    m["dude"] = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
    Variant v(m);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeMap));
    EXPECT_TRUE(v.is_container_type());
    EXPECT_FALSE(v.is_fundamental_type());
    EXPECT_THAT(v.map(),
                UnorderedElementsAre(
                    Pair(Property(&Variant::string_value, StrEq("hello")),
                         Property(&Variant::int64_value, Eq(kTestInt64))),
                    Pair(Property(&Variant::string_value, StrEq("world")),
                         Property(&Variant::string_value, Eq(kTestString))),
                    Pair(Property(&Variant::string_value, StrEq("how")),
                         Property(&Variant::double_value, Eq(kTestDouble))),
                    Pair(Property(&Variant::string_value, StrEq("are")),
                         Property(&Variant::bool_value, Eq(kTestBool))),
                    Pair(Property(&Variant::string_value, StrEq("you")),
                         Property(&Variant::is_null, Eq(true))),
                    Pair(Property(&Variant::string_value, StrEq("dude")),
                         Property(&Variant::blob_size, Eq(kTestBlobSize)))));
  }
  {
    std::map<Variant, Variant> m;
    m["0"] = kTestInt64;
    m[0] = kTestString;
    m[0.0] = kTestBool;
    m[false] = kTestDouble;
    m[Variant::Null()] = kTestMutableString;
    Variant v(m);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(
        v.map(),
        UnorderedElementsAre(
            Pair(AllOf(Property(&Variant::is_string, Eq(true)),
                       Property(&Variant::string_value, StrEq("0"))),
                 AllOf(Property(&Variant::is_int64, Eq(true)),
                       Property(&Variant::int64_value, Eq(kTestInt64)))),
            Pair(AllOf(Property(&Variant::is_int64, Eq(true)),
                       Property(&Variant::int64_value, Eq(0))),
                 AllOf(Property(&Variant::is_string, Eq(true)),
                       Property(&Variant::string_value, Eq(kTestString)))),
            Pair(AllOf(Property(&Variant::is_double, Eq(true)),
                       Property(&Variant::double_value, Eq(0.0))),
                 AllOf(Property(&Variant::is_bool, Eq(true)),
                       Property(&Variant::bool_value, Eq(kTestBool)))),
            Pair(AllOf(Property(&Variant::is_bool, Eq(true)),
                       Property(&Variant::bool_value, Eq(false))),
                 AllOf(Property(&Variant::is_double, Eq(true)),
                       Property(&Variant::double_value, Eq(kTestDouble)))),
            Pair(Property(&Variant::is_null, Eq(true)),
                 AllOf(Property(&Variant::is_string, Eq(true)),
                       Property(&Variant::mutable_string,
                                Eq(kTestMutableString))))));
  }
  {
    // Ensure that if you reassign to a key in the map, it modifies it.
    std::vector<int> vect1 = {1, 2, 3, 4};
    std::vector<int> vect2 = {1, 2, 4, 4};
    std::vector<int> vect1copy = {1, 2, 3, 4};
    Variant v = Variant::EmptyMap();
    v.map()[vect1] = "Hello";
    v.map()[vect2] = "world";
    EXPECT_THAT(v.map(),
                UnorderedElementsAre(
                    Pair(Property(&Variant::vector, ElementsAre(1, 2, 3, 4)),
                         Property(&Variant::string_value, StrEq("Hello"))),
                    Pair(Property(&Variant::vector, ElementsAre(1, 2, 4, 4)),
                         Property(&Variant::string_value, StrEq("world")))));
    EXPECT_THAT(vect1, Eq(vect1copy));
    v.map()[vect1copy] = "Goodbye";
    EXPECT_THAT(v.map(),
                UnorderedElementsAre(
                    Pair(Property(&Variant::vector, ElementsAre(1, 2, 3, 4)),
                         Property(&Variant::string_value, StrEq("Goodbye"))),
                    Pair(Property(&Variant::vector, ElementsAre(1, 2, 4, 4)),
                         Property(&Variant::string_value, StrEq("world")))));
  }
}

TEST_F(VariantTest, TestConstructingMapViaTemplate) {
  {
    std::map<int, const char*> m{std::make_pair(23, "apple"),
                                 std::make_pair(45, "banana"),
                                 std::make_pair(67, "orange")};
    Variant v(m);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(
        v.map(),
        UnorderedElementsAre(
            Pair(AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                       Property(&Variant::int64_value, Eq(23))),
                 AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                       Property(&Variant::string_value, StrEq("apple")))),
            Pair(AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                       Property(&Variant::int64_value, Eq(45))),
                 AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                       Property(&Variant::string_value, StrEq("banana")))),
            Pair(AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                       Property(&Variant::int64_value, Eq(67))),
                 AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                       Property(&Variant::string_value, StrEq("orange"))))));
  }
}

TEST_F(VariantTest, TestNestedMaps) {
  // TODO(jsimantov): Implement tests for maps of maps.
}

TEST_F(VariantTest, TestComplexNesting) {
  // TODO(jsimantov): Implement tests for complex nesting, e.g. maps of vectors
  // of maps of etc.
}

TEST_F(VariantTest, TestCopyAndAssignment) {
  // Test copy constructor and assignment operator.
  {
    Variant v1(kTestString);
    Variant v2(kTestInt64);
    Variant v3(kTestMutableString);
    Variant v4(kTestVector);

    EXPECT_THAT(v1.string_value(), Eq(kTestString));
    EXPECT_THAT(v2.int64_value(), Eq(kTestInt64));
    EXPECT_THAT(v3.mutable_string(), Eq(kTestMutableString));

    v1 = v2;
    EXPECT_THAT(v1.int64_value(), Eq(kTestInt64));
    EXPECT_THAT(v2.int64_value(), Eq(kTestInt64));

    v1 = v3;
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v1.mutable_string(), Eq(kTestMutableString));
    EXPECT_THAT(v3.mutable_string(), Eq(kTestMutableString));
    // Ensure they don't point to the same mutable string.
    EXPECT_THAT(&v1.mutable_string(), Ne(&v3.mutable_string()));

    v1 = v4;
    EXPECT_THAT(v1.vector(), Eq(kTestVector));
    EXPECT_THAT(v4.vector(), Eq(kTestVector));

    Variant v5(kTestDouble);
    Variant v6(v5);  // NOLINT
    EXPECT_THAT(v6, Eq(v5));

    Variant v7(std::string("Mutable Longer string"));
    Variant v8("Static");
    Variant v9(v7);
    Variant v10(v8);  // NOLINT
    EXPECT_THAT(v7.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v8.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v9.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v10.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v7.string_value(), StrEq("Mutable Longer string"));
    v7 = v8;
    EXPECT_THAT(v7.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v8.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v9.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v10.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v7.string_value(), StrEq("Static"));
    v7 = v9;
    EXPECT_THAT(v7.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v8.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v9.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v10.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v7.string_value(), StrEq("Mutable Longer string"));
    v7 = v10;
    EXPECT_THAT(v7.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v8.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v9.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v10.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v7.string_value(), StrEq("Static"));
  }

  // Test move constructor.
  {
    Variant v1(kTestMutableString);
    EXPECT_THAT(v1.mutable_string(), Eq(kTestMutableString));
    const std::string* v1_ptr = &v1.mutable_string();

    Variant v2(std::move(v1));
    // Ensure v2 has the value that v1 had.
    EXPECT_THAT(v2.mutable_string(), Eq(kTestMutableString));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const std::string* v2_ptr = &v2.mutable_string();
    EXPECT_THAT(v1_ptr, Eq(v2_ptr));

    Variant v3(kTestVector);
    EXPECT_THAT(v3.type(), Eq(Variant::kTypeVector));
    v3 = std::move(v2);
    EXPECT_THAT(v3.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v3.mutable_string(), Eq(kTestMutableString));
    EXPECT_TRUE(v2.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const std::string* v3_ptr = &v3.mutable_string();
    EXPECT_THAT(v2_ptr, Eq(v3_ptr));
  }

  {
    Variant v = std::string("Hello");
    EXPECT_THAT(v, Eq("Hello"));
    v = *&v;
    EXPECT_THAT(v, Eq("Hello"));
    Variant v1 = std::move(v);
    v = std::move(v1);
    EXPECT_THAT(v, Eq("Hello"));
  }

  {
    Variant v1 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
    Variant v2 = Variant::FromMutableBlob(kTestBlobData, kTestBlobSize);
    EXPECT_THAT(v1, Eq(v2));
    Variant v3 = v1;
    EXPECT_THAT(v1, Eq(v2));
    EXPECT_THAT(v1, Eq(v3));
    EXPECT_THAT(v2, Eq(v3));
    v3 = v2;
    EXPECT_THAT(v1, Eq(v2));
    EXPECT_THAT(v1, Eq(v3));
    EXPECT_THAT(v2, Eq(v3));
    Variant v0 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
    v3 = std::move(v1);
    EXPECT_THAT(v3, Eq(v0));
    v3 = std::move(v2);
    EXPECT_THAT(v3, Eq(v0));
  }
}

TEST_F(VariantTest, TestEqualityOperators) {
  {
    Variant v0(3);
    Variant v1(3);
    Variant v2(4);
    EXPECT_EQ(v0, v1);
    EXPECT_NE(v1, v2);
    EXPECT_NE(v0, v2);
    EXPECT_TRUE(v0 < v2 || v2 < v0);
    EXPECT_FALSE(v0 < v2 && v2 < v0);

    EXPECT_THAT(v0, Not(Lt(v1)));
    EXPECT_THAT(v0, Not(Gt(v1)));
  }
  {
    Variant v1("Hello, world!");
    Variant v2(std::string("Hello, world!"));
    EXPECT_EQ(v1, v2);
  }
  {
    Variant v1(std::vector<int>{0, 1});
    Variant v2(std::vector<int>{1, 0});
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeVector));
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeVector));
    EXPECT_FALSE(v1 < v2 && v2 < v1);
  }
}

TEST_F(VariantTest, TestDefaults) {
  EXPECT_THAT(Variant::Null(),
              Property(&Variant::type, Eq(Variant::kTypeNull)));
  EXPECT_THAT(Variant::Zero(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeInt64)),
                    Property(&Variant::int64_value, Eq(0))));
  EXPECT_THAT(Variant::ZeroPointZero(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeDouble)),
                    Property(&Variant::double_value, Eq(0.0))));
  EXPECT_THAT(Variant::False(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeBool)),
                    Property(&Variant::bool_value, Eq(false))));
  EXPECT_THAT(Variant::True(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeBool)),
                    Property(&Variant::bool_value, Eq(true))));
  EXPECT_THAT(Variant::EmptyString(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeStaticString)),
                    Property(&Variant::string_value, StrEq(""))));
  EXPECT_THAT(Variant::EmptyMutableString(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeMutableString)),
                    Property(&Variant::string_value, StrEq(""))));
  EXPECT_THAT(Variant::EmptyVector(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeVector)),
                    Property(&Variant::vector, IsEmpty())));
  EXPECT_THAT(Variant::EmptyMap(),
              AllOf(Property(&Variant::type, Eq(Variant::kTypeMap)),
                    Property(&Variant::map, IsEmpty())));
}

TEST_F(VariantTest, TestSettersAndGetters) {
  // TODO(jsimantov): Implement tests for setters and getters, including
  // modifying the contents of Variant containers. Also verifies that const
  // getters work, and are returning the same thing as non-const versions.
  {
    Variant v;
    const Variant& vconst = v;
    EXPECT_THAT(v.type(), Eq(Variant::kTypeNull));
    v.set_int64_value(123);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(v.int64_value(), Eq(123));
    EXPECT_THAT(vconst.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(vconst.int64_value(), Eq(123));
    EXPECT_EQ(v, vconst);
    v.set_vector({4, 5, 6});
    EXPECT_THAT(v.type(), Eq(Variant::kTypeVector));
    EXPECT_THAT(v.vector(), ElementsAre(4, 5, 6));
    EXPECT_THAT(vconst.type(), Eq(Variant::kTypeVector));
    EXPECT_THAT(vconst.vector(), ElementsAre(4, 5, 6));
    EXPECT_EQ(v, vconst);
    v.set_double_value(456.7);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(v.double_value(), Eq(456.7));
    EXPECT_THAT(vconst.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(vconst.double_value(), Eq(456.7));
    EXPECT_EQ(v, vconst);
    v.set_bool_value(false);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeBool));
    EXPECT_THAT(v.bool_value(), Eq(false));
    EXPECT_THAT(vconst.type(), Eq(Variant::kTypeBool));
    EXPECT_THAT(vconst.bool_value(), Eq(false));
    EXPECT_EQ(v, vconst);
    v.set_map({std::make_pair(33, 44), std::make_pair(55, 66)});
    EXPECT_THAT(v.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v.map(), UnorderedElementsAre(Pair(33, 44), Pair(55, 66)));
    EXPECT_THAT(vconst.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(vconst.map(), UnorderedElementsAre(Pair(33, 44), Pair(55, 66)));
    EXPECT_EQ(v, vconst);
  }
}

TEST_F(VariantTest, TestConversionFunctions) {
  {
    EXPECT_EQ(Variant::Null().AsBool(), Variant::False());
    EXPECT_EQ(Variant::Zero().AsBool(), Variant::False());
    EXPECT_EQ(Variant::ZeroPointZero().AsBool(), Variant::False());
    EXPECT_EQ(Variant::EmptyMap().AsBool(), Variant::False());
    EXPECT_EQ(Variant::EmptyVector().AsBool(), Variant::False());
    EXPECT_EQ(Variant::EmptyString().AsBool(), Variant::False());
    EXPECT_EQ(Variant::EmptyMutableString().AsBool(), Variant::False());

    EXPECT_EQ(Variant::One().AsBool(), Variant::True());
    EXPECT_EQ(Variant::OnePointZero().AsBool(), Variant::True());
    EXPECT_EQ(Variant(123).AsBool(), Variant::True());
    EXPECT_EQ(Variant(456.7).AsBool(), Variant::True());
    EXPECT_EQ(Variant("Hello").AsBool(), Variant::True());
    EXPECT_EQ(Variant::MutableStringFromStaticString("Hello").AsBool(),
              Variant::True());
    EXPECT_EQ(Variant(std::vector<Variant>{0}).AsBool(), Variant::True());
    EXPECT_EQ(Variant(std::map<Variant, Variant>{std::make_pair(23, "apple"),
                                                 std::make_pair(45, "banana"),
                                                 std::make_pair(67, "orange")})
                  .AsBool(),
              Variant::True());
    EXPECT_EQ(Variant::FromStaticBlob(kTestBlobData, 0).AsBool(),
              Variant::False());
    EXPECT_EQ(Variant::FromMutableBlob(kTestBlobData, 0).AsBool(),
              Variant::False());
    EXPECT_EQ(Variant::FromStaticBlob(kTestBlobData, kTestBlobSize).AsBool(),
              Variant::True());
    EXPECT_EQ(Variant::FromMutableBlob(kTestBlobData, kTestBlobSize).AsBool(),
              Variant::True());
  }
  {
    const Variant vint(12345);
    EXPECT_THAT(vint.type(), Eq(Variant::kTypeInt64));

    Variant vdouble = vint.AsDouble();
    EXPECT_THAT(vdouble.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(vdouble.double_value(), Eq(12345.0));

    const Variant vstring("87755.899");
    EXPECT_TRUE(vstring.is_string());
    vdouble = vstring.AsDouble();
    EXPECT_THAT(vdouble.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(vdouble.double_value(), Eq(87755.899));

    EXPECT_EQ(vdouble.AsDouble(), vdouble);

    EXPECT_THAT(Variant::True().AsDouble(), Eq(Variant(1.0)));
    EXPECT_THAT(Variant::False().AsDouble(), Eq(Variant::ZeroPointZero()));
    EXPECT_THAT(Variant::False().AsDouble(), Eq(Variant::ZeroPointZero()));
    EXPECT_THAT(Variant::Null().AsDouble(), Eq(Variant::ZeroPointZero()));
    EXPECT_THAT(Variant(kTestVector).AsDouble(), Eq(Variant::ZeroPointZero()));
    EXPECT_THAT(Variant(g_test_map).AsDouble(), Eq(Variant::ZeroPointZero()));
  }
  {
    Variant vstring(std::string("38294"));
    EXPECT_TRUE(vstring.is_string());

    Variant vint = vstring.AsInt64();
    EXPECT_THAT(vint.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(vint.int64_value(), Eq(38294));

    // Check truncation.
    Variant vdouble(399.9);
    EXPECT_THAT(vdouble.type(), Eq(Variant::kTypeDouble));
    vint = vdouble.AsInt64();
    EXPECT_THAT(vint.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(vint.int64_value(), Eq(399));

    EXPECT_THAT(Variant::True().AsInt64(), Eq(Variant(1)));
    EXPECT_THAT(Variant::False().AsInt64(), Eq(Variant::Zero()));
    EXPECT_THAT(Variant::Null().AsInt64(), Eq(Variant::Zero()));
    EXPECT_THAT(Variant(kTestVector).AsInt64(), Eq(Variant::Zero()));
    EXPECT_THAT(Variant(g_test_map).AsInt64(), Eq(Variant::Zero()));
  }
  {
    Variant vint(int64_t(9223372036800000000L));  // almost max value
    EXPECT_THAT(vint.type(), Eq(Variant::kTypeInt64));

    Variant vstring = vint.AsString();
    EXPECT_TRUE(vstring.is_string());
    EXPECT_THAT(vstring.string_value(), StrEq("9223372036800000000"));

    Variant vdouble(34491282.2909820005297661);
    EXPECT_THAT(vdouble.type(), Eq(Variant::kTypeDouble));
    vstring = vdouble.AsString();
    EXPECT_TRUE(vstring.is_string());
    EXPECT_THAT(vstring.string_value(), StrEq("34491282.2909820005297661"));

    EXPECT_THAT(Variant::True().AsString(), Eq(Variant("true")));
    EXPECT_THAT(Variant::False().AsString(), Eq(Variant("false")));
    EXPECT_THAT(Variant::Null().AsString(), Eq(Variant::EmptyString()));
    EXPECT_THAT(Variant(kTestVector).AsString(), Eq(Variant::EmptyString()));
    EXPECT_THAT(Variant(g_test_map).AsString(), Eq(Variant::EmptyString()));
  }
}

// Copy a buffer+size into a vector, so gMock matchers can properly access it.
template <typename T>
static std::vector<T> AsVector(const T* buffer, size_t size_bytes) {
  return std::vector<T>(buffer, buffer + (size_bytes / sizeof(*buffer)));
}

TEST_F(VariantTest, TestBlobs) {
  Variant v1 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
  EXPECT_THAT(v1.type(), Eq(Variant::kTypeStaticBlob));
  EXPECT_THAT(v1.blob_size(), Eq(kTestBlobSize));
  EXPECT_THAT(v1.blob_data(), Eq(kTestBlobData));

  Variant v2 = Variant::FromMutableBlob(kTestBlobData, kTestBlobSize);
  EXPECT_THAT(v2.type(), Eq(Variant::kTypeMutableBlob));
  EXPECT_THAT(v2.blob_size(), Eq(kTestBlobSize));
  EXPECT_THAT(v2.blob_data(), Ne(kTestBlobData));

  EXPECT_THAT(v1, Eq(v2));
  EXPECT_THAT(v1, Not(Lt(v2)));
  EXPECT_THAT(v1, Not(Gt(v2)));

  // Make a copy of the mutable buffer that we can modify.
  Variant v3 = v2;

  // Modify something within the mutable buffer, then ensure that they are
  // no longer equal. Note that we don't care which is < the other.
  reinterpret_cast<uint8_t*>(v3.mutable_blob_data())[kTestBlobSize / 2]++;
  EXPECT_THAT(v1, Not(Eq(v3)));
  EXPECT_THAT(v1, AnyOf(Lt(v3), Gt(v3)));
  EXPECT_THAT(v2, Not(Eq(v3)));
  EXPECT_THAT(v2, AnyOf(Lt(v3), Gt(v3)));

  // Ensure two blobs that are mostly the same but different sizes compare as
  // different.
  Variant v4 = Variant::FromMutableBlob(v2.blob_data(), v2.blob_size() - 1);
  EXPECT_THAT(v2, Not(Eq(v4)));
  EXPECT_THAT(v2, AnyOf(Lt(v4), Gt(v4)));

  // Check that two static blobs from the same data point to the same copy.
  Variant v5 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
  EXPECT_THAT(v5.blob_data(), Eq(v1.blob_data()));
  EXPECT_THAT(v5.blob_data(), Not(Eq(v2.blob_data())));
}

TEST_F(VariantTest, TestMutableBlobPromotion) {
  Variant v = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);

  EXPECT_THAT(v.type(), Eq(Variant::kTypeStaticBlob));
  EXPECT_THAT(v.blob_size(), Eq(kTestBlobSize));
  EXPECT_THAT(AsVector(v.blob_data(), v.blob_size()),
              ElementsAreArray(kTestBlobData, kTestBlobSize));
  (void)v.mutable_blob_data();
  EXPECT_THAT(v.type(), Eq(Variant::kTypeMutableBlob));
  EXPECT_THAT(v.blob_size(), Eq(kTestBlobSize));
  EXPECT_THAT(AsVector(v.blob_data(), v.blob_size()),
              ElementsAreArray(kTestBlobData, kTestBlobSize));
  // Modify one byte of the buffer.
  reinterpret_cast<uint8_t*>(v.mutable_blob_data())[kTestBlobSize / 3] += 99;
  uint8_t compare_buffer[kTestBlobSize];
  memcpy(compare_buffer, kTestBlobData, kTestBlobSize);
  // Make the same change to a local buffer for comparison.
  compare_buffer[kTestBlobSize / 3] += 99;
  EXPECT_THAT(AsVector(v.blob_data(), v.blob_size()),
              ElementsAreArray(compare_buffer, kTestBlobSize));
  v.set_static_blob(kTestBlobData, kTestBlobSize);
  EXPECT_THAT(v.blob_size(), Eq(kTestBlobSize));
  EXPECT_THAT(AsVector(v.blob_data(), v.blob_size()),
              ElementsAreArray(kTestBlobData, kTestBlobSize));

  // Check that two static blobs from the same data point to the same copy, but
  // not after promotion.
  Variant v1 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
  Variant v2 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
  EXPECT_THAT(v1.blob_data(), Eq(v2.blob_data()));
  (void)v2.mutable_blob_data();
  EXPECT_THAT(v1.blob_data(), Ne(v2.blob_data()));

  // Check that you can call set_mutable_blob on a Variant's own blob_data and
  // blob_size.
  Variant v3 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
  EXPECT_THAT(v3.type(), Eq(Variant::kTypeStaticBlob));
  EXPECT_THAT(AsVector(v3.blob_data(), v3.blob_size()),
              ElementsAreArray(kTestBlobData, kTestBlobSize));
  v3.set_mutable_blob(v3.blob_data(), v3.blob_size());
  EXPECT_THAT(v3.type(), Eq(Variant::kTypeMutableBlob));
  EXPECT_THAT(AsVector(v3.blob_data(), v3.blob_size()),
              ElementsAreArray(kTestBlobData, kTestBlobSize));
}

TEST_F(VariantTest, TestMoveConstructorOnAllTypes) {
  // Test fundamental/statically allocated types.
  {
    Variant v1(kTestInt64);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(v1.int64_value(), Eq(kTestInt64));
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeInt64));
    EXPECT_THAT(v2.int64_value(), Eq(kTestInt64));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
  }
  {
    Variant v1(kTestDouble);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(v1.double_value(), Eq(kTestDouble));
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeDouble));
    EXPECT_THAT(v2.double_value(), Eq(kTestDouble));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
  }
  {
    Variant v1(kTestBool);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeBool));
    EXPECT_THAT(v1.bool_value(), Eq(kTestBool));
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeBool));
    EXPECT_THAT(v2.bool_value(), Eq(kTestBool));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
  }
  {
    // Static string.
    Variant v1(kTestString);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v1.string_value(), Eq(kTestString));
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeStaticString));
    EXPECT_THAT(v2.string_value(), Eq(kTestString));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
  }
  {
    // Static blob.
    Variant v1 = Variant::FromStaticBlob(kTestBlobData, kTestBlobSize);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeStaticBlob));
    EXPECT_THAT(AsVector(v1.blob_data(), v1.blob_size()),
                ElementsAreArray(kTestBlobData, kTestBlobSize));
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeStaticBlob));
    EXPECT_THAT(AsVector(v2.blob_data(), v2.blob_size()),
                ElementsAreArray(kTestBlobData, kTestBlobSize));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
  }

  // Test allocated types (mutable string, blob, containers)
  {
    Variant v1(kTestMutableString);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v1.mutable_string(), Eq(kTestMutableString));
    const std::string* v1_ptr = &v1.mutable_string();
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeMutableString));
    EXPECT_THAT(v2.mutable_string(), Eq(kTestMutableString));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const std::string* v2_ptr = &v2.mutable_string();
    EXPECT_THAT(v1_ptr, Eq(v2_ptr));
  }
  {
    Variant v1(kTestVector);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeVector));
    EXPECT_THAT(v1.vector(), Eq(kTestVector));
    const std::vector<Variant>* v1_ptr = &v1.vector();
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeVector));
    EXPECT_THAT(v2.vector(), Eq(kTestVector));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const std::vector<Variant>* v2_ptr = &v2.vector();
    EXPECT_THAT(v1_ptr, Eq(v2_ptr));
  }
  {
    Variant v1(g_test_map);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v1.map(), Eq(g_test_map));
    const std::map<Variant, Variant>* v1_ptr = &v1.map();
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v2.map(), Eq(g_test_map));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const std::map<Variant, Variant>* v2_ptr = &v2.map();
    EXPECT_THAT(v1_ptr, Eq(v2_ptr));
  }
  {
    Variant v1 = Variant::FromMutableBlob(kTestBlobData, kTestBlobSize);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeMutableBlob));
    EXPECT_THAT(AsVector(v1.blob_data(), v1.blob_size()),
                ElementsAreArray(kTestBlobData, kTestBlobSize));
    const void* v1_ptr = v1.blob_data();
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeMutableBlob));
    EXPECT_THAT(AsVector(v2.blob_data(), v2.blob_size()),
                ElementsAreArray(kTestBlobData, kTestBlobSize));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const void* v2_ptr = v2.blob_data();
    EXPECT_THAT(v1_ptr, Eq(v2_ptr));
  }
  // Test complex nested container type.
  {
    Variant v1(g_test_complex_map);
    EXPECT_THAT(v1.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v1.map(), Eq(g_test_complex_map));
    const std::map<Variant, Variant>* v1_ptr = &v1.map();
    Variant v2(std::move(v1));
    // Ensure v2 has the type and value that v1 had.
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v2.map(), Eq(g_test_complex_map));
    // Ensure v1 no longer has that value.
    EXPECT_TRUE(v1.is_null());  // NOLINT
    // Bonus points: Ensure that the pointer was simply moved.
    const std::map<Variant, Variant>* v2_ptr = &v2.map();
    EXPECT_THAT(v1_ptr, Eq(v2_ptr));
  }

  // Test moving over existing variant values.
  {
    Variant v2(kTestString);
    Variant v1 = Variant::Null();
    v2 = std::move(v1);
    EXPECT_TRUE(v1.is_null());  // NOLINT
    EXPECT_TRUE(v2.is_null());
  }
  {
    Variant v2(g_test_complex_map);
    EXPECT_THAT(v2.type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v2.map(), Eq(g_test_complex_map));
    Variant v1 = kTestComplexVector;
    EXPECT_TRUE(v1.is_vector());
    EXPECT_THAT(v1.vector(), Eq(kTestComplexVector));
    v2 = std::move(v1);
    EXPECT_TRUE(v1.is_null());  // NOLINT
    EXPECT_TRUE(v2.is_vector());
    EXPECT_THAT(v2.vector(), Eq(kTestComplexVector));
  }
  {
    Variant v(kTestComplexVector);
    EXPECT_THAT(v.type(), Eq(Variant::kTypeVector));
    EXPECT_THAT(v.vector(), Eq(kTestComplexVector));
    Variant v2(g_test_complex_map);
    v.vector()[2] = std::move(v2);
    EXPECT_THAT(v.vector()[2].type(), Eq(Variant::kTypeMap));
    EXPECT_THAT(v.vector()[2], Eq(g_test_complex_map));
  }
}

}  // namespace testing
}  // namespace firebase
