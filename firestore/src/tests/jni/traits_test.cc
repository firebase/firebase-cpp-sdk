#include "firestore/src/jni/traits.h"

#include <jni.h>

#include <limits>

#include "firestore/src/jni/object.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {

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

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
