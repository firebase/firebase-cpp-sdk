#include "firestore/src/jni/traits.h"

#include <jni.h>

#include <limits>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

using testing::StaticAssertTypeEq;

using TraitsTest = FirestoreIntegrationTest;

template <typename C, typename J>
void ExpectConvertsPrimitive() {
  static_assert(sizeof(C) >= sizeof(J),
                "Java types should never be bigger than C++ equivalents");

  // Some C++ types (notably size_t) have a size that is not fixed. Use the
  // maximum value supported by the Java type for testing.
  C cpp_value = static_cast<C>(std::numeric_limits<J>::max());
  J jni_value = ToJni(cpp_value);
  EXPECT_EQ(jni_value, static_cast<J>(cpp_value));
}

class TestObject : public Object {
 public:
  using Object::Object;
};

TEST_F(TraitsTest, ConvertsPrimitives) {
  ExpectConvertsPrimitive<bool, jboolean>();
  ExpectConvertsPrimitive<uint8_t, jbyte>();
  ExpectConvertsPrimitive<uint16_t, jchar>();
  ExpectConvertsPrimitive<int16_t, jshort>();
  ExpectConvertsPrimitive<int32_t, jint>();
  ExpectConvertsPrimitive<int64_t, jlong>();
  ExpectConvertsPrimitive<float, jfloat>();
  ExpectConvertsPrimitive<double, jdouble>();
  ExpectConvertsPrimitive<size_t, jsize>();
}

TEST_F(TraitsTest, PassesThroughJniPrimitives) {
  ExpectConvertsPrimitive<jboolean, jboolean>();
  ExpectConvertsPrimitive<jbyte, jbyte>();
  ExpectConvertsPrimitive<jchar, jchar>();
  ExpectConvertsPrimitive<jshort, jshort>();
  ExpectConvertsPrimitive<jint, jint>();
  ExpectConvertsPrimitive<jlong, jlong>();
  ExpectConvertsPrimitive<jfloat, jfloat>();
  ExpectConvertsPrimitive<jdouble, jdouble>();
  ExpectConvertsPrimitive<jsize, jsize>();
}

TEST_F(TraitsTest, ConvertsObjects) {
  Object cpp_value;
  jobject jni_value = ToJni(cpp_value);
  EXPECT_EQ(jni_value, nullptr);

  jobject jobject_value = nullptr;
  jni_value = ToJni(jobject_value);
  EXPECT_EQ(jni_value, nullptr);

  jni_value = ToJni(nullptr);
  EXPECT_EQ(jni_value, nullptr);
}

TEST_F(TraitsTest, ConvertsStrings) {
  Env env;

  String empty_value;
  jstring jni_value = ToJni(empty_value);
  EXPECT_EQ(jni_value, nullptr);

  Local<String> cpp_value = env.NewStringUtf("testing");
  jni_value = ToJni(cpp_value);
  EXPECT_EQ(jni_value, cpp_value.get());

  jstring jstring_value = nullptr;
  jni_value = ToJni(jstring_value);
  EXPECT_EQ(jni_value, nullptr);
}

TEST_F(TraitsTest, ConvertsArbitrarySubclassesOfObject) {
  TestObject cpp_value;
  jobject jni_value = ToJni(cpp_value);
  EXPECT_EQ(jni_value, nullptr);
}

TEST_F(TraitsTest, ConvertsOwnershipWrappers) {
  StaticAssertTypeEq<JniType<Local<Object>>, jobject>();
  StaticAssertTypeEq<JniType<Global<String>>, jstring>();
  StaticAssertTypeEq<JniType<const Local<String>&>, jstring>();

  Local<Object> local_value;
  jobject jni_value = ToJni(local_value);
  EXPECT_EQ(jni_value, nullptr);

  Local<TestObject> test_value;
  jni_value = ToJni(test_value);
  EXPECT_EQ(jni_value, nullptr);

  Global<Object> global_value;
  jni_value = ToJni(global_value);
  EXPECT_EQ(jni_value, nullptr);
}

// Conversion implicitly tests type mapping. Additionally test variations of
// types that should be equivalent.
TEST_F(TraitsTest, DecaysBeforeMappingTypes) {
  StaticAssertTypeEq<JniType<int32_t>, jint>();
  StaticAssertTypeEq<JniType<const int32_t>, jint>();

  StaticAssertTypeEq<JniType<jobject>, jobject>();
  StaticAssertTypeEq<JniType<const jobject>, jobject>();

  StaticAssertTypeEq<JniType<Object>, jobject>();
  StaticAssertTypeEq<JniType<const Object>, jobject>();
  StaticAssertTypeEq<JniType<const Object&>, jobject>();
  StaticAssertTypeEq<JniType<Object&&>, jobject>();
}

TEST_F(TraitsTest, ToJniHandlesPointers) {
  // Baseline sanity checks that undergird our reasoning for being able to
  // reinterpret_cast pointers to these types. Note that in C++ we prefer
  // uint8_t for bytes despite the fact that Java defines these as signed.
  // Even though signedness differs, these types are still similar and therefore
  // valid to reinterpret.
  static_assert(is_same<int8_t, jbyte>::value, "JNI type is sane");
  static_assert(is_same<uint16_t, jchar>::value, "JNI type is sane");
  static_assert(is_same<int16_t, jshort>::value, "JNI type is sane");
  static_assert(is_same<int16_t, jshort>::value, "JNI type is sane");
  static_assert(is_same<int32_t, jint>::value, "JNI type is sane");
  static_assert(is_same<int64_t, jlong>::value, "JNI type is sane");

  // These assertions reflect our C++ preferred type.
  static_assert(!internal::IsConvertiblePointer<bool>(), "bool not OK");
  static_assert(internal::IsConvertiblePointer<uint8_t>(), "uint8_t OK");
  static_assert(internal::IsConvertiblePointer<uint16_t>(), "uint16_t OK");
  static_assert(internal::IsConvertiblePointer<int16_t>(), "int16_t OK");
  static_assert(internal::IsConvertiblePointer<int32_t>(), "int32_t OK");
  static_assert(internal::IsConvertiblePointer<int64_t>(), "int64_t OK");
  static_assert(!internal::IsConvertiblePointer<size_t>(), "size_t not OK");

  uint8_t bytes[2] = {1, 2};
  const jbyte* bytes_result = ToJni(bytes);
  EXPECT_EQ(*bytes_result, 1);
  EXPECT_EQ(bytes_result[1], 2);

  int64_t longs[2] = {1, 2};
  jlong* longs_result = ToJni(static_cast<int64_t*>(longs));
  EXPECT_EQ(*longs_result, 1);
  EXPECT_EQ(longs_result[1], 2);
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
