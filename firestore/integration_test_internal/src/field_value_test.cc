/*
 * Copyright 2021 Google LLC
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

#include <utility>

#include "firebase/firestore.h"
#include "firestore_integration_test.h"
#if defined(__ANDROID__)
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#else
#include "firestore/src/main/field_value_main.h"
#endif  // defined(__ANDROID__)

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using Type = FieldValue::Type;
using FieldValueTest = FirestoreIntegrationTest;

// Sanity test for stubs
TEST_F(FieldValueTest, TestFieldValueTypes) {
  FieldValue::Null();
  FieldValue::Boolean(true);
  FieldValue::Integer(123L);
  FieldValue::Double(3.1415926);
  FieldValue::Timestamp({12345, 54321});
  FieldValue::String("hello");
  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  FieldValue::Blob(blob, sizeof(blob));
  FieldValue::GeoPoint({43, 80});
  FieldValue::Array(std::vector<FieldValue>{FieldValue::Null()});
  FieldValue::Map(MapFieldValue{{"Null", FieldValue::Null()}});
  FieldValue::Delete();
  FieldValue::ServerTimestamp();
  FieldValue::ArrayUnion(std::vector<FieldValue>{FieldValue::Null()});
  FieldValue::ArrayRemove(std::vector<FieldValue>{FieldValue::Null()});
}

#if defined(__ANDROID__)

TEST_F(FieldValueTest, Construction) {
  testutil::AssertWrapperConstructionContract<FieldValue>();
}

TEST_F(FieldValueTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<FieldValue>();
}

#endif  // defined(__ANDROID__)

TEST_F(FieldValueTest, TestNullType) {
  FieldValue value = FieldValue::Null();
  EXPECT_TRUE(Type::kNull == value.type());
}

TEST_F(FieldValueTest, TestBooleanType) {
  FieldValue value = FieldValue::Boolean(true);
  EXPECT_TRUE(Type::kBoolean == value.type());
  EXPECT_TRUE(true == value.boolean_value());
}

TEST_F(FieldValueTest, TestIntegerType) {
  FieldValue value = FieldValue::Integer(123);
  EXPECT_TRUE(Type::kInteger == value.type());
  EXPECT_TRUE(123 == value.integer_value());
}

TEST_F(FieldValueTest, TestDoubleType) {
  FieldValue value = FieldValue::Double(3.1415926);
  EXPECT_TRUE(Type::kDouble == value.type());
  EXPECT_TRUE(3.1415926 == value.double_value());
}

TEST_F(FieldValueTest, TestTimestampType) {
  FieldValue value = FieldValue::Timestamp({12345, 54321});
  EXPECT_TRUE(Type::kTimestamp == value.type());
  EXPECT_TRUE(Timestamp(12345, 54321) == value.timestamp_value());
}

TEST_F(FieldValueTest, TestStringType) {
  FieldValue value = FieldValue::String("hello");
  EXPECT_TRUE(Type::kString == value.type());
  EXPECT_STREQ("hello", value.string_value().c_str());
}

TEST_F(FieldValueTest, TestStringTypeSpecialCases) {
  // Latin small letter e with acute accent. Codepoints above 7F are encoded
  // in multiple bytes.
  std::string str = u8"\u00E9clair";
  EXPECT_TRUE(FieldValue::String(str).string_value() == str);

  // Latin small letter e + combining acute accent. Similar to above but using
  // a combining character, which is not normalized.
  str = u8"e\u0301clair";
  EXPECT_TRUE(FieldValue::String(str).string_value() == str);

  // Face with tears of joy. This is an emoji outside the BMP and encodes as
  // four bytes in UTF-8 and as a surrogate pair in UTF-16. JNI's modified UTF-8
  // encodes each surrogate as a separate three byte value for a total of six
  // bytes.
  str = u8"\U0001F602!!";
  EXPECT_TRUE(FieldValue::String(str).string_value() == str);

  // Embedded null character. JNI's modified UTF-8 encoding encodes this in a
  // two byte sequence that doesn't contain a zero byte.
  str = u8"aaa";
  str[1] = '\0';
  FieldValue value = FieldValue::String(str);
  EXPECT_TRUE(value.string_value() == str);
  EXPECT_STREQ(value.string_value().c_str(), "a");
}

TEST_F(FieldValueTest, TestBlobType) {
  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  FieldValue value = FieldValue::Blob(blob, sizeof(blob));
  EXPECT_TRUE(Type::kBlob == value.type());
  EXPECT_TRUE(sizeof(blob) == value.blob_size());
  const uint8_t* value_blob = value.blob_value();

  FieldValue copied(value);
  EXPECT_TRUE(Type::kBlob == copied.type());
  EXPECT_TRUE(sizeof(blob) == copied.blob_size());
  const uint8_t* copied_blob = copied.blob_value();

  for (int i = 0; i < sizeof(blob); ++i) {
    EXPECT_TRUE(blob[i] == value_blob[i]);
    EXPECT_TRUE(blob[i] == copied_blob[i]);
  }
}

TEST_F(FieldValueTest, TestReferenceType) {
  FieldValue value =
      FieldValue::Reference(TestFirestore()->Document("foo/bar"));
  EXPECT_TRUE(Type::kReference == value.type());
  EXPECT_TRUE(value.reference_value().path() == "foo/bar");
}

TEST_F(FieldValueTest, TestGeoPointType) {
  FieldValue value = FieldValue::GeoPoint({43, 80});
  EXPECT_TRUE(Type::kGeoPoint == value.type());
  EXPECT_TRUE(GeoPoint(43, 80) == value.geo_point_value());
}

TEST_F(FieldValueTest, TestArrayType) {
  FieldValue value =
      FieldValue::Array({FieldValue::Boolean(true), FieldValue::Integer(123)});
  EXPECT_TRUE(Type::kArray == value.type());
  const std::vector<FieldValue>& array = value.array_value();
  EXPECT_TRUE(2 == array.size());
  EXPECT_TRUE(true == array[0].boolean_value());
  EXPECT_TRUE(123 == array[1].integer_value());
}

TEST_F(FieldValueTest, TestMapType) {
  FieldValue value = FieldValue::Map(MapFieldValue{
      {"Bool", FieldValue::Boolean(true)}, {"Int", FieldValue::Integer(123)}});
  EXPECT_TRUE(Type::kMap == value.type());
  MapFieldValue map = value.map_value();
  EXPECT_TRUE(2 == map.size());
  EXPECT_TRUE(true == map["Bool"].boolean_value());
  EXPECT_TRUE(123 == map["Int"].integer_value());
}

TEST_F(FieldValueTest, TestSentinelType) {
  FieldValue delete_value = FieldValue::Delete();
  EXPECT_TRUE(Type::kDelete == delete_value.type());

  FieldValue server_timestamp_value = FieldValue::ServerTimestamp();
  EXPECT_TRUE(Type::kServerTimestamp == server_timestamp_value.type());

  std::vector<FieldValue> array = {FieldValue::Boolean(true),
                                   FieldValue::Integer(123)};
  FieldValue array_union = FieldValue::ArrayUnion(array);
  EXPECT_TRUE(Type::kArrayUnion == array_union.type());
  FieldValue array_remove = FieldValue::ArrayRemove(array);
  EXPECT_TRUE(Type::kArrayRemove == array_remove.type());

  FieldValue increment_integer = FieldValue::Increment(1);
  EXPECT_TRUE(Type::kIncrementInteger == increment_integer.type());

  FieldValue increment_double = FieldValue::Increment(1.0);
  EXPECT_TRUE(Type::kIncrementDouble == increment_double.type());
}

TEST_F(FieldValueTest, TestEquality) {
  EXPECT_TRUE(FieldValue::Null() == FieldValue::Null());
  EXPECT_TRUE(FieldValue::Boolean(true) == FieldValue::Boolean(true));
  EXPECT_TRUE(FieldValue::Integer(123) == FieldValue::Integer(123));
  EXPECT_TRUE(FieldValue::Double(456.0) == FieldValue::Double(456.0));
  EXPECT_TRUE(FieldValue::String("foo") == FieldValue::String("foo"));

  EXPECT_TRUE(FieldValue::Timestamp({123, 456}) ==
              FieldValue::Timestamp({123, 456}));

  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  EXPECT_TRUE(FieldValue::Blob(blob, sizeof(blob)) ==
              FieldValue::Blob(blob, sizeof(blob)));

  EXPECT_TRUE(FieldValue::GeoPoint({43, 80}) == FieldValue::GeoPoint({43, 80}));

  EXPECT_TRUE(
      FieldValue::Array({FieldValue::Integer(3), FieldValue::Double(4.0)}) ==
      FieldValue::Array({FieldValue::Integer(3), FieldValue::Double(4.0)}));

  EXPECT_TRUE(FieldValue::Map(MapFieldValue{{"foo", FieldValue::Integer(3)}}) ==
              FieldValue::Map(MapFieldValue{{"foo", FieldValue::Integer(3)}}));

  EXPECT_TRUE(FieldValue::Delete() == FieldValue::Delete());
  EXPECT_TRUE(FieldValue::ServerTimestamp() == FieldValue::ServerTimestamp());
  // TODO(varconst): make this work on Android, or remove the tests below.
  // EXPECT_TRUE(FieldValue::ArrayUnion({FieldValue::Null()}) ==
  //           FieldValue::ArrayUnion({FieldValue::Null()}));
  // EXPECT_TRUE(FieldValue::ArrayRemove({FieldValue::Null()}) ==
  //           FieldValue::ArrayRemove({FieldValue::Null()}));
}

TEST_F(FieldValueTest, TestInequality) {
  EXPECT_NE(FieldValue::Boolean(false), FieldValue::Boolean(true));
  EXPECT_NE(FieldValue::Integer(123), FieldValue::Integer(456));
  EXPECT_NE(FieldValue::Double(123.0), FieldValue::Double(456.0));
  EXPECT_NE(FieldValue::String("foo"), FieldValue::String("bar"));

  EXPECT_NE(FieldValue::Timestamp({123, 456}),
            FieldValue::Timestamp({789, 123}));

  uint8_t blob1[] = "( ͡° ͜ʖ ͡°)";
  uint8_t blob2[] = "___";
  EXPECT_NE(FieldValue::Blob(blob1, sizeof(blob2)),
            FieldValue::Blob(blob2, sizeof(blob2)));

  EXPECT_NE(FieldValue::GeoPoint({43, 80}), FieldValue::GeoPoint({12, 34}));

  EXPECT_NE(
      FieldValue::Array({FieldValue::Integer(3), FieldValue::Double(4.0)}),
      FieldValue::Array({FieldValue::Integer(5), FieldValue::Double(4.0)}));

  EXPECT_NE(FieldValue::Map(MapFieldValue{{"foo", FieldValue::Integer(3)}}),
            FieldValue::Map(MapFieldValue{{"foo", FieldValue::Integer(4)}}));

  EXPECT_NE(FieldValue::Delete(), FieldValue::ServerTimestamp());
  EXPECT_NE(FieldValue::ArrayUnion({FieldValue::Null()}),
            FieldValue::ArrayUnion({FieldValue::Boolean(false)}));
  EXPECT_NE(FieldValue::ArrayRemove({FieldValue::Null()}),
            FieldValue::ArrayRemove({FieldValue::Boolean(false)}));
}

TEST_F(FieldValueTest, TestInequalityDueToDifferentTypes) {
  EXPECT_NE(FieldValue::Null(), FieldValue::Delete());
  EXPECT_NE(FieldValue::Integer(1), FieldValue::Boolean(true));
  EXPECT_NE(FieldValue::Integer(123), FieldValue::Double(123));
  EXPECT_NE(FieldValue::ArrayUnion({FieldValue::Null()}),
            FieldValue::ArrayRemove({FieldValue::Null()}));
  EXPECT_NE(FieldValue::Array({FieldValue::Null()}),
            FieldValue::ArrayRemove({FieldValue::Null()}));
  // Fully exhaustive check seems overkill, just check the types that are known
  // to have the same (or very similar) representation.
}

TEST_F(FieldValueTest, TestToString) {
  EXPECT_TRUE("<invalid>" == FieldValue().ToString());

  EXPECT_TRUE("null" == FieldValue::Null().ToString());
  EXPECT_TRUE("true" == FieldValue::Boolean(true).ToString());
  EXPECT_TRUE("123" == FieldValue::Integer(123L).ToString());
  EXPECT_TRUE("3.14" == FieldValue::Double(3.14).ToString());
  EXPECT_TRUE("Timestamp(seconds=12345, nanoseconds=54321)" ==
              FieldValue::Timestamp({12345, 54321}).ToString());
  EXPECT_TRUE("'hello'" == FieldValue::String("hello").ToString());
  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  EXPECT_TRUE("Blob(28 20 cd a1 c2 b0 20 cd 9c ca 96 20 cd a1 c2 b0 29 00)" ==
              FieldValue::Blob(blob, sizeof(blob)).ToString());
  EXPECT_TRUE("GeoPoint(latitude=43, longitude=80)" ==
              FieldValue::GeoPoint({43, 80}).ToString());

  EXPECT_TRUE("DocumentReference(invalid)" ==
              FieldValue::Reference({}).ToString());

  EXPECT_TRUE("[]" == FieldValue::Array({}).ToString());
  EXPECT_TRUE("[null]" == FieldValue::Array({FieldValue::Null()}).ToString());
  EXPECT_TRUE("[null, true, 1]" ==
              FieldValue::Array({FieldValue::Null(), FieldValue::Boolean(true),
                                 FieldValue::Integer(1)})
                  .ToString());
  // TODO(b/150016438): uncomment this case (fails on Android).
  // EXPECT_TRUE("[<invalid>]" == FieldValue::Array({FieldValue()}).ToString());

  EXPECT_TRUE("{}" == FieldValue::Map({}).ToString());
  // TODO(b/150016438): uncomment this case (fails on Android).
  // EXPECT_TRUE("{bad: <invalid>}" == FieldValue::Map({
  //                                                       {"bad",
  //                                                       FieldValue()},
  //                                                   })
  //                                   .ToString());
  EXPECT_TRUE("{Null: null}" ==
              FieldValue::Map({
                                  {"Null", FieldValue::Null()},
                              })
                  .ToString());
  // Note: because the map is unordered, it's hard to check the case where a map
  // has more than one element.

  EXPECT_TRUE("FieldValue::Delete()" == FieldValue::Delete().ToString());
  EXPECT_TRUE("FieldValue::ServerTimestamp()" ==
              FieldValue::ServerTimestamp().ToString());
  EXPECT_TRUE("FieldValue::ArrayUnion()" ==
              FieldValue::ArrayUnion({FieldValue::Null()}).ToString());
  EXPECT_TRUE("FieldValue::ArrayRemove()" ==
              FieldValue::ArrayRemove({FieldValue::Null()}).ToString());

  EXPECT_TRUE("FieldValue::Increment()" == FieldValue::Increment(1).ToString());
  EXPECT_TRUE("FieldValue::Increment()" ==
              FieldValue::Increment(1.0).ToString());
}

TEST_F(FieldValueTest, TestIncrementChoosesTheCorrectType) {
  // Signed integers
  // NOLINTNEXTLINE -- exact integer width doesn't matter.
  short foo = 1;
  EXPECT_TRUE(FieldValue::Increment(foo).type() == Type::kIncrementInteger);
  EXPECT_TRUE(FieldValue::Increment(1).type() == Type::kIncrementInteger);
  EXPECT_TRUE(FieldValue::Increment(1L).type() == Type::kIncrementInteger);
  // Note: using `long long` syntax to avoid go/lsc-long-long-literal.
  // NOLINTNEXTLINE -- exact integer width doesn't matter.
  long long llfoo = 1;
  EXPECT_TRUE(FieldValue::Increment(llfoo).type() == Type::kIncrementInteger);

  // Unsigned integers
  // NOLINTNEXTLINE -- exact integer width doesn't matter.
  unsigned short ufoo = 1;
  EXPECT_TRUE(FieldValue::Increment(ufoo).type() == Type::kIncrementInteger);
  EXPECT_TRUE(FieldValue::Increment(1U).type() == Type::kIncrementInteger);

  // Floating point
  EXPECT_TRUE(FieldValue::Increment(1.0f).type() == Type::kIncrementDouble);
  EXPECT_TRUE(FieldValue::Increment(1.0).type() == Type::kIncrementDouble);

  // The statements below shouldn't compile (uncomment to check).

  // clang-format off
  // Types that would lead to truncation:
  // EXPECT_TRUE(FieldValue::Increment(1UL).type() == Type::kIncrementInteger);
  // unsigned long long ullfoo = 1;
  // EXPECT_TRUE(FieldValue::Increment(ullfoo).type() == Type::kIncrementInteger);
  // EXPECT_TRUE(FieldValue::Increment(1.0L).type() == Type::kIncrementDouble);

  // Inapplicable types:
  // EXPECT_TRUE(FieldValue::Increment(true).type() == Type::kIncrementInteger);
  // EXPECT_TRUE(FieldValue::Increment('a').type() == Type::kIncrementInteger);
  // EXPECT_TRUE(FieldValue::Increment("abc").type() == Type::kIncrementInteger);
  // clang-format on
}

}  // namespace firestore
}  // namespace firebase
