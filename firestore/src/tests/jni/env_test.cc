#include "firestore/src/jni/env.h"

#include "firestore/src/tests/firestore_integration_test.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {

class EnvTest : public FirestoreIntegrationTest {
 public:
  EnvTest() : env_(GetEnv()) {}

 protected:
  Env env_;
};

#if __cpp_exceptions
TEST_F(EnvTest, ToolchainSupportsThrowingFromDestructors) {
  class ThrowsInDestructor {
   public:
    ~ThrowsInDestructor() noexcept(false) { throw std::exception(); }
  };

  try {
    { ThrowsInDestructor obj; }
    FAIL() << "Should have thrown";
  } catch (const std::exception& e) {
    SUCCEED() << "Caught exception";
  }
}
#endif  // __cpp_exceptions

TEST_F(EnvTest, ConstructsObjects) {
  Local<Class> clazz = env_.FindClass("java/lang/Integer");
  jmethodID new_integer = env_.GetMethodId(clazz, "<init>", "(I)V");

  Local<Object> result = env_.New<Object>(clazz, new_integer, 42);
  EXPECT_EQ("42", result.ToString(env_));
}

TEST_F(EnvTest, CallsBooleanMethods) {
  Local<String> haystack = env_.NewStringUtf("Food");
  Local<String> needle = env_.NewStringUtf("Foo");

  Local<Class> clazz = env_.FindClass("java/lang/String");
  jmethodID starts_with =
      env_.GetMethodId(clazz, "startsWith", "(Ljava/lang/String;)Z");

  bool result = env_.Call<bool>(haystack, starts_with, needle);
  EXPECT_TRUE(result);

  needle = env_.NewStringUtf("Bar");
  result = env_.Call<bool>(haystack, starts_with, needle);
  EXPECT_FALSE(result);
}

TEST_F(EnvTest, CallsIntMethods) {
  Local<String> str = env_.NewStringUtf("Foo");

  Local<Class> clazz = env_.FindClass("java/lang/String");
  jmethodID index_of = env_.GetMethodId(clazz, "indexOf", "(I)I");

  int32_t result = env_.Call<int32_t>(str, index_of, jint('o'));
  EXPECT_EQ(1, result);

  result = env_.Call<int32_t>(str, index_of, jint('z'));
  EXPECT_EQ(-1, result);
}

TEST_F(EnvTest, CallsObjectMethods) {
  Local<String> str = env_.NewStringUtf("Foo");

  Local<Class> clazz = env_.FindClass("java/lang/String");
  jmethodID to_lower_case =
      env_.GetMethodId(clazz, "toLowerCase", "()Ljava/lang/String;");

  Local<String> result = env_.Call<String>(str, to_lower_case);
  EXPECT_EQ("foo", result.ToString(env_));
}

TEST_F(EnvTest, CallsVoidMethods) {
  Local<Class> clazz = env_.FindClass("java/lang/StringBuilder");
  jmethodID ctor = env_.GetMethodId(clazz, "<init>", "()V");
  jmethodID get_length = env_.GetMethodId(clazz, "length", "()I");
  jmethodID set_length = env_.GetMethodId(clazz, "setLength", "(I)V");

  Local<Object> builder = env_.New(clazz, ctor);
  env_.Call<void>(builder, set_length, 42);

  int32_t length = env_.Call<int32_t>(builder, get_length);
  EXPECT_EQ(length, 42);
}

TEST_F(EnvTest, GetsStaticFields) {
  Local<Class> clazz = env_.FindClass("java/lang/String");
  jfieldID comparator = env_.GetStaticFieldId(clazz, "CASE_INSENSITIVE_ORDER",
                                              "Ljava/util/Comparator;");

  Local<Object> result = env_.GetStaticField<Object>(clazz, comparator);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(EnvTest, CallsStaticObjectMethods) {
  Local<Class> clazz = env_.FindClass("java/lang/String");
  jmethodID value_of_int =
      env_.GetStaticMethodId(clazz, "valueOf", "(I)Ljava/lang/String;");

  Local<String> result = env_.CallStatic<String>(clazz, value_of_int, 42);
  EXPECT_EQ("42", result.ToString(env_));
}

TEST_F(EnvTest, CallsStaticVoidMethods) {
  Local<Class> clazz = env_.FindClass("java/lang/System");
  jmethodID gc = env_.GetStaticMethodId(clazz, "gc", "()V");

  env_.CallStatic<void>(clazz, gc);
  EXPECT_TRUE(env_.ok());
}

TEST_F(EnvTest, GetStringUtfLength) {
  Local<String> str = env_.NewStringUtf("Foo");
  size_t len = env_.GetStringUtfLength(str);
  EXPECT_EQ(3, len);
}

TEST_F(EnvTest, GetStringUtfRegion) {
  Local<String> str = env_.NewStringUtf("Foo");
  std::string result = env_.GetStringUtfRegion(str, 1, 2);
  EXPECT_EQ("oo", result);
}

TEST_F(EnvTest, ToString) {
  Local<String> str = env_.NewStringUtf("Foo");
  std::string result = str.ToString(env_);
  EXPECT_EQ("Foo", result);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
