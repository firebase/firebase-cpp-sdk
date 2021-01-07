#include <utility>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#if defined(__ANDROID__)
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/field_value_stub.h"
#else
#include "firestore/src/ios/field_value_ios.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using Type = FieldValue::Type;
using FieldValueTest = testing::Test;

// Sanity test for stubs
TEST_F(FirestoreIntegrationTest, TestFieldValueTypes) {
  ASSERT_NO_THROW({
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
  });
}

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

TEST_F(FieldValueTest, Construction) {
  testutil::AssertWrapperConstructionContract<FieldValue>();
}

TEST_F(FieldValueTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<FieldValue>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

#if !defined(FIRESTORE_STUB_BUILD)

TEST_F(FirestoreIntegrationTest, TestNullType) {
  FieldValue value = FieldValue::Null();
  EXPECT_EQ(Type::kNull, value.type());
}

TEST_F(FirestoreIntegrationTest, TestBooleanType) {
  FieldValue value = FieldValue::Boolean(true);
  EXPECT_EQ(Type::kBoolean, value.type());
  EXPECT_EQ(true, value.boolean_value());
}

TEST_F(FirestoreIntegrationTest, TestIntegerType) {
  FieldValue value = FieldValue::Integer(123);
  EXPECT_EQ(Type::kInteger, value.type());
  EXPECT_EQ(123, value.integer_value());
}

TEST_F(FirestoreIntegrationTest, TestDoubleType) {
  FieldValue value = FieldValue::Double(3.1415926);
  EXPECT_EQ(Type::kDouble, value.type());
  EXPECT_EQ(3.1415926, value.double_value());
}

TEST_F(FirestoreIntegrationTest, TestTimestampType) {
  FieldValue value = FieldValue::Timestamp({12345, 54321});
  EXPECT_EQ(Type::kTimestamp, value.type());
  EXPECT_EQ(Timestamp(12345, 54321), value.timestamp_value());
}

TEST_F(FirestoreIntegrationTest, TestStringType) {
  FieldValue value = FieldValue::String("hello");
  EXPECT_EQ(Type::kString, value.type());
  EXPECT_STREQ("hello", value.string_value().c_str());
}

TEST_F(FirestoreIntegrationTest, TestBlobType) {
  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  FieldValue value = FieldValue::Blob(blob, sizeof(blob));
  EXPECT_EQ(Type::kBlob, value.type());
  EXPECT_EQ(sizeof(blob), value.blob_size());
  const uint8_t* value_blob = value.blob_value();

  FieldValue copied(value);
  EXPECT_EQ(Type::kBlob, copied.type());
  EXPECT_EQ(sizeof(blob), copied.blob_size());
  const uint8_t* copied_blob = copied.blob_value();

  for (int i = 0; i < sizeof(blob); ++i) {
    EXPECT_EQ(blob[i], value_blob[i]);
    EXPECT_EQ(blob[i], copied_blob[i]);
  }
}

TEST_F(FirestoreIntegrationTest, TestReferenceType) {
  FieldValue value =
      FieldValue::Reference(TestFirestore()->Document("foo/bar"));
  EXPECT_EQ(Type::kReference, value.type());
  EXPECT_EQ(value.reference_value().path(), "foo/bar");
}

TEST_F(FirestoreIntegrationTest, TestGeoPointType) {
  FieldValue value = FieldValue::GeoPoint({43, 80});
  EXPECT_EQ(Type::kGeoPoint, value.type());
  EXPECT_EQ(GeoPoint(43, 80), value.geo_point_value());
}

TEST_F(FirestoreIntegrationTest, TestArrayType) {
  FieldValue value = FieldValue::Array(
      {FieldValue::Boolean(true), FieldValue::Integer(123)});
  EXPECT_EQ(Type::kArray, value.type());
  const std::vector<FieldValue>& array = value.array_value();
  EXPECT_EQ(2, array.size());
  EXPECT_EQ(true, array[0].boolean_value());
  EXPECT_EQ(123, array[1].integer_value());
}

TEST_F(FirestoreIntegrationTest, TestMapType) {
  FieldValue value =
      FieldValue::Map(MapFieldValue{{"Bool", FieldValue::Boolean(true)},
                                    {"Int", FieldValue::Integer(123)}});
  EXPECT_EQ(Type::kMap, value.type());
  MapFieldValue map = value.map_value();
  EXPECT_EQ(2, map.size());
  EXPECT_EQ(true, map["Bool"].boolean_value());
  EXPECT_EQ(123, map["Int"].integer_value());
}

TEST_F(FirestoreIntegrationTest, TestSentinelType) {
  FieldValue delete_value = FieldValue::Delete();
  EXPECT_EQ(Type::kDelete, delete_value.type());

  FieldValue server_timestamp_value = FieldValue::ServerTimestamp();
  EXPECT_EQ(Type::kServerTimestamp, server_timestamp_value.type());

  std::vector<FieldValue> array = {FieldValue::Boolean(true),
                                   FieldValue::Integer(123)};
  FieldValue array_union = FieldValue::ArrayUnion(array);
  EXPECT_EQ(Type::kArrayUnion, array_union.type());
  FieldValue array_remove = FieldValue::ArrayRemove(array);
  EXPECT_EQ(Type::kArrayRemove, array_remove.type());

  FieldValue increment_integer = FieldValue::Increment(1);
  EXPECT_EQ(Type::kIncrementInteger, increment_integer.type());

  FieldValue increment_double = FieldValue::Increment(1.0);
  EXPECT_EQ(Type::kIncrementDouble, increment_double.type());
}

TEST_F(FirestoreIntegrationTest, TestEquality) {
  EXPECT_EQ(FieldValue::Null(), FieldValue::Null());
  EXPECT_EQ(FieldValue::Boolean(true), FieldValue::Boolean(true));
  EXPECT_EQ(FieldValue::Integer(123), FieldValue::Integer(123));
  EXPECT_EQ(FieldValue::Double(456.0), FieldValue::Double(456.0));
  EXPECT_EQ(FieldValue::String("foo"), FieldValue::String("foo"));

  EXPECT_EQ(FieldValue::Timestamp({123, 456}),
            FieldValue::Timestamp({123, 456}));

  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  EXPECT_EQ(FieldValue::Blob(blob, sizeof(blob)),
            FieldValue::Blob(blob, sizeof(blob)));

  EXPECT_EQ(FieldValue::GeoPoint({43, 80}), FieldValue::GeoPoint({43, 80}));

  EXPECT_EQ(
      FieldValue::Array({FieldValue::Integer(3), FieldValue::Double(4.0)}),
      FieldValue::Array({FieldValue::Integer(3), FieldValue::Double(4.0)}));

  EXPECT_EQ(FieldValue::Map(MapFieldValue{{"foo", FieldValue::Integer(3)}}),
            FieldValue::Map(MapFieldValue{{"foo", FieldValue::Integer(3)}}));

  EXPECT_EQ(FieldValue::Delete(), FieldValue::Delete());
  EXPECT_EQ(FieldValue::ServerTimestamp(), FieldValue::ServerTimestamp());
  // TODO(varconst): make this work on Android, or remove the tests below.
  // EXPECT_EQ(FieldValue::ArrayUnion({FieldValue::Null()}),
  //           FieldValue::ArrayUnion({FieldValue::Null()}));
  // EXPECT_EQ(FieldValue::ArrayRemove({FieldValue::Null()}),
  //           FieldValue::ArrayRemove({FieldValue::Null()}));
}

TEST_F(FirestoreIntegrationTest, TestInequality) {
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

TEST_F(FirestoreIntegrationTest, TestInequalityDueToDifferentTypes) {
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

TEST_F(FirestoreIntegrationTest, TestToString) {
  EXPECT_EQ("<invalid>", FieldValue().ToString());

  EXPECT_EQ("null", FieldValue::Null().ToString());
  EXPECT_EQ("true", FieldValue::Boolean(true).ToString());
  EXPECT_EQ("123", FieldValue::Integer(123L).ToString());
  EXPECT_EQ("3.14", FieldValue::Double(3.14).ToString());
  EXPECT_EQ("Timestamp(seconds=12345, nanoseconds=54321)",
            FieldValue::Timestamp({12345, 54321}).ToString());
  EXPECT_EQ("'hello'", FieldValue::String("hello").ToString());
  uint8_t blob[] = "( ͡° ͜ʖ ͡°)";
  EXPECT_EQ("Blob(28 20 cd a1 c2 b0 20 cd 9c ca 96 20 cd a1 c2 b0 29 00)",
            FieldValue::Blob(blob, sizeof(blob)).ToString());
  EXPECT_EQ("GeoPoint(latitude=43, longitude=80)",
            FieldValue::GeoPoint({43, 80}).ToString());

  EXPECT_EQ("DocumentReference(invalid)", FieldValue::Reference({}).ToString());

  EXPECT_EQ("[]", FieldValue::Array({}).ToString());
  EXPECT_EQ("[null]", FieldValue::Array({FieldValue::Null()}).ToString());
  EXPECT_EQ("[null, true, 1]",
            FieldValue::Array({FieldValue::Null(), FieldValue::Boolean(true),
                               FieldValue::Integer(1)})
                .ToString());
  // TODO(b/150016438): uncomment this case (fails on Android).
  // EXPECT_EQ("[<invalid>]", FieldValue::Array({FieldValue()}).ToString());

  EXPECT_EQ("{}", FieldValue::Map({}).ToString());
  // TODO(b/150016438): uncomment this case (fails on Android).
  // EXPECT_EQ("{bad: <invalid>}", FieldValue::Map({
  //                                                       {"bad",
  //                                                       FieldValue()},
  //                                                   })
  //                                   .ToString());
  EXPECT_EQ("{Null: null}", FieldValue::Map({
                                                {"Null", FieldValue::Null()},
                                            })
                                .ToString());
  // Note: because the map is unordered, it's hard to check the case where a map
  // has more than one element.

  EXPECT_EQ("FieldValue::Delete()", FieldValue::Delete().ToString());
  EXPECT_EQ("FieldValue::ServerTimestamp()",
            FieldValue::ServerTimestamp().ToString());
  EXPECT_EQ("FieldValue::ArrayUnion()",
            FieldValue::ArrayUnion({FieldValue::Null()}).ToString());
  EXPECT_EQ("FieldValue::ArrayRemove()",
            FieldValue::ArrayRemove({FieldValue::Null()}).ToString());

  EXPECT_EQ("FieldValue::Increment()", FieldValue::Increment(1).ToString());
  EXPECT_EQ("FieldValue::Increment()", FieldValue::Increment(1.0).ToString());
}

TEST_F(FirestoreIntegrationTest, TestIncrementChoosesTheCorrectType) {
  // Signed integers
  // NOLINTNEXTLINE -- exact integer width doesn't matter.
  short foo = 1;
  EXPECT_EQ(FieldValue::Increment(foo).type(), Type::kIncrementInteger);
  EXPECT_EQ(FieldValue::Increment(1).type(), Type::kIncrementInteger);
  EXPECT_EQ(FieldValue::Increment(1L).type(), Type::kIncrementInteger);
  // Note: using `long long` syntax to avoid go/lsc-long-long-literal.
  // NOLINTNEXTLINE -- exact integer width doesn't matter.
  long long llfoo = 1;
  EXPECT_EQ(FieldValue::Increment(llfoo).type(), Type::kIncrementInteger);

  // Unsigned integers
  // NOLINTNEXTLINE -- exact integer width doesn't matter.
  unsigned short ufoo = 1;
  EXPECT_EQ(FieldValue::Increment(ufoo).type(), Type::kIncrementInteger);
  EXPECT_EQ(FieldValue::Increment(1U).type(), Type::kIncrementInteger);

  // Floating point
  EXPECT_EQ(FieldValue::Increment(1.0f).type(), Type::kIncrementDouble);
  EXPECT_EQ(FieldValue::Increment(1.0).type(), Type::kIncrementDouble);

  // The statements below shouldn't compile (uncomment to check).

  // Types that would lead to truncation:
  // EXPECT_EQ(FieldValue::Increment(1UL).type(), Type::kIncrementInteger);
  // unsigned long long ullfoo = 1;
  // EXPECT_EQ(FieldValue::Increment(ullfoo).type(), Type::kIncrementInteger);
  // EXPECT_EQ(FieldValue::Increment(1.0L).type(), Type::kIncrementDouble);

  // Inapplicable types:
  // EXPECT_EQ(FieldValue::Increment(true).type(), Type::kIncrementInteger);
  // EXPECT_EQ(FieldValue::Increment('a').type(), Type::kIncrementInteger);
  // EXPECT_EQ(FieldValue::Increment("abc").type(), Type::kIncrementInteger);
}

#endif  // !defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
