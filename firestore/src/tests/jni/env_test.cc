#include "firestore/src/jni/env.h"

#include "app/memory/unique_ptr.h"
#include "app/meta/move.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {

class EnvTest : public FirestoreIntegrationTest {
 public:
  EnvTest() : env_(MakeUnique<Env>(GetEnv())) {}

  ~EnvTest() override {
    // Ensure that after the test is done that any pending exception is cleared
    // to prevent spurious errors in the teardown of FirestoreIntegrationTest.
    env_->ExceptionClear();
  }

  Env& env() const { return *env_; }

 protected:
  // Env is declared as having a `noexcept(false)` destructor, which causes the
  // compiler to propagate this into `EnvTest`'s destructor, but this conflicts
  // with the declaration of the parent class. Holding `Env` with a unique
  // pointer sidesteps this restriction.
  UniquePtr<Env> env_;
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
  Local<Class> clazz = env().FindClass("java/lang/Integer");
  jmethodID new_integer = env().GetMethodId(clazz, "<init>", "(I)V");

  Local<Object> result = env().New<Object>(clazz, new_integer, 42);
  EXPECT_EQ("42", result.ToString(env()));
}

TEST_F(EnvTest, CallsBooleanMethods) {
  Local<String> haystack = env().NewStringUtf("Food");
  Local<String> needle = env().NewStringUtf("Foo");

  Local<Class> clazz = env().FindClass("java/lang/String");
  jmethodID starts_with =
      env().GetMethodId(clazz, "startsWith", "(Ljava/lang/String;)Z");

  bool result = env().Call<bool>(haystack, starts_with, needle);
  EXPECT_TRUE(result);

  needle = env().NewStringUtf("Bar");
  result = env().Call<bool>(haystack, starts_with, needle);
  EXPECT_FALSE(result);
}

TEST_F(EnvTest, CallsIntMethods) {
  Local<String> str = env().NewStringUtf("Foo");

  Local<Class> clazz = env().FindClass("java/lang/String");
  jmethodID index_of = env().GetMethodId(clazz, "indexOf", "(I)I");

  int32_t result = env().Call<int32_t>(str, index_of, jint('o'));
  EXPECT_EQ(1, result);

  result = env().Call<int32_t>(str, index_of, jint('z'));
  EXPECT_EQ(-1, result);
}

TEST_F(EnvTest, CallsObjectMethods) {
  Local<String> str = env().NewStringUtf("Foo");

  Local<Class> clazz = env().FindClass("java/lang/String");
  jmethodID to_lower_case =
      env().GetMethodId(clazz, "toLowerCase", "()Ljava/lang/String;");

  Local<String> result = env().Call<String>(str, to_lower_case);
  EXPECT_EQ("foo", result.ToString(env()));
}

TEST_F(EnvTest, CallsVoidMethods) {
  Local<Class> clazz = env().FindClass("java/lang/StringBuilder");
  jmethodID ctor = env().GetMethodId(clazz, "<init>", "()V");
  jmethodID get_length = env().GetMethodId(clazz, "length", "()I");
  jmethodID set_length = env().GetMethodId(clazz, "setLength", "(I)V");

  Local<Object> builder = env().New(clazz, ctor);
  env().Call<void>(builder, set_length, 42);

  int32_t length = env().Call<int32_t>(builder, get_length);
  EXPECT_EQ(length, 42);
}

TEST_F(EnvTest, GetsStaticFields) {
  Local<Class> clazz = env().FindClass("java/lang/String");
  jfieldID comparator = env().GetStaticFieldId(clazz, "CASE_INSENSITIVE_ORDER",
                                               "Ljava/util/Comparator;");

  Local<Object> result = env().GetStaticField<Object>(clazz, comparator);
  EXPECT_NE(result.get(), nullptr);
}

TEST_F(EnvTest, CallsStaticObjectMethods) {
  Local<Class> clazz = env().FindClass("java/lang/String");
  jmethodID value_of_int =
      env().GetStaticMethodId(clazz, "valueOf", "(I)Ljava/lang/String;");

  Local<String> result = env().CallStatic<String>(clazz, value_of_int, 42);
  EXPECT_EQ("42", result.ToString(env()));
}

TEST_F(EnvTest, CallsStaticVoidMethods) {
  Local<Class> clazz = env().FindClass("java/lang/System");
  jmethodID gc = env().GetStaticMethodId(clazz, "gc", "()V");

  env().CallStatic<void>(clazz, gc);
  EXPECT_TRUE(env().ok());
}

TEST_F(EnvTest, GetStringUtfLength) {
  Local<String> str = env().NewStringUtf("Foo");
  size_t len = env().GetStringUtfLength(str);
  EXPECT_EQ(3, len);
}

TEST_F(EnvTest, GetStringUtfRegion) {
  Local<String> str = env().NewStringUtf("Foo");
  std::string result = env().GetStringUtfRegion(str, 1, 2);
  EXPECT_EQ("oo", result);
}

TEST_F(EnvTest, ToString) {
  Local<String> str = env().NewStringUtf("Foo");
  std::string result = str.ToString(env());
  EXPECT_EQ("Foo", result);
}

TEST_F(EnvTest, Throw) {
  Local<Class> clazz = env().FindClass("java/lang/Exception");
  jmethodID ctor = env().GetMethodId(clazz, "<init>", "(Ljava/lang/String;)V");

  Local<String> message = env().NewStringUtf("Testing throw");
  Local<Throwable> exception = env().New<Throwable>(clazz, ctor, message);

  // After throwing, use EXPECT rather than ASSERT to ensure that the exception
  // is cleared.
  env().Throw(exception);
  EXPECT_FALSE(env().ok());

  Local<Throwable> thrown = env().ClearExceptionOccurred();
  EXPECT_EQ(thrown.GetMessage(env()), "Testing throw");
}

TEST_F(EnvTest, ThrowShortCircuitsExecution) {
  // Set up the test by obtaining some classes and methods before throwing.
  Local<Class> integer_class = env().FindClass("java/lang/Integer");
  jmethodID integer_ctor = env().GetMethodId(integer_class, "<init>", "(I)V");
  jmethodID int_value = env().GetMethodId(integer_class, "intValue", "()I");
  Local<Object> integer = env().New(integer_class, integer_ctor, 42);

  // Verify things work under normal conditions
  ASSERT_EQ(env().Call<int32_t>(integer, int_value), 42);

  // After throwing, everything should short circuit.
  Local<Class> exception_class = env().FindClass("java/lang/Exception");
  env().ThrowNew(exception_class, "Testing throw");
  Local<Throwable> thrown = env().ExceptionOccurred();

  EXPECT_EQ(env().FindClass("java/lang/Double").get(), nullptr);
  EXPECT_EQ(env().GetMethodId(integer_class, "doubleValue", "()D"), nullptr);
  EXPECT_EQ(env().Call<int32_t>(integer, int_value), 0);

  EXPECT_EQ(env().New(integer_class, integer_ctor, 95).get(), nullptr);
  EXPECT_EQ(env().GetObjectClass(integer).get(), nullptr);

  // Methods like IsSameObject also return zero values to short circuit
  EXPECT_FALSE(env().IsInstanceOf(integer, integer_class));
  EXPECT_FALSE(env().IsSameObject(exception_class, exception_class));

  env().ExceptionClear();

  // Verify things are back to normal.
  EXPECT_EQ(env().Call<int32_t>(integer, int_value), 42);
  EXPECT_TRUE(env().IsInstanceOf(integer, integer_class));
  EXPECT_TRUE(env().IsSameObject(exception_class, exception_class));
}

TEST_F(EnvTest, ThrowShortCircuitsThrow) {
  Local<Class> exception_class = env().FindClass("java/lang/Exception");
  env().ThrowNew(exception_class, "Testing throw");
  Local<Throwable> thrown = env().ExceptionOccurred();

  env().ThrowNew(exception_class, "Testing throw 2");
  Local<Throwable> thrown_while_throwing = env().ExceptionOccurred();

  env().ExceptionClear();
  EXPECT_TRUE(env().IsSameObject(thrown, thrown_while_throwing));
  EXPECT_EQ(thrown_while_throwing.GetMessage(env()), "Testing throw");
}

TEST_F(EnvTest, ExceptionClearGuardRunsWhilePending) {
  Local<Class> exception_class = env().FindClass("java/lang/Exception");
  env().ThrowNew(exception_class, "Testing throw");
  Local<Throwable> thrown = env().ExceptionOccurred();

  EXPECT_EQ(thrown.GetMessage(env()), "Testing throw");
  EXPECT_FALSE(env().ok());

  {
    ExceptionClearGuard block(env());
    EXPECT_TRUE(env().ok());
  }

  EXPECT_EQ(thrown.GetMessage(env()), "Testing throw");
  EXPECT_FALSE(env().ok());

  {
    ExceptionClearGuard block(env());
    EXPECT_TRUE(env().ok());
    EXPECT_TRUE(env().IsInstanceOf(thrown, exception_class));

    // A new exception thrown while in the block will cause the prior exception
    // to be lost. This mirrors the behavior of a Java finally block.
    env().ThrowNew(exception_class, "Testing throw 2");
    EXPECT_EQ(env().ExceptionOccurred().GetMessage(env()), "Testing throw 2");
  }
  EXPECT_EQ(env().ExceptionOccurred().GetMessage(env()), "Testing throw 2");

  env().ExceptionClear();
  {
    ExceptionClearGuard block(env());
    env().ThrowNew(exception_class, "Testing throw 3");
  }

  // Outside the block, the exception persists, again mirroring the behavior of
  // a Java finally block.
  EXPECT_EQ(env().ExceptionOccurred().GetMessage(env()), "Testing throw 3");
}

TEST_F(EnvTest, DestructorCallsExceptionHandler) {
  struct Result {
    Local<Throwable> exception;
    int calls = 0;
  };

  auto handler = [](Env& env, Local<Throwable>&& exception, void* context) {
    env.ExceptionClear();

    auto result = static_cast<Result*>(context);
    result->exception = Move(exception);
    result->calls++;
  };

  Result result;
  {
    Env env;
    env.SetUnhandledExceptionHandler(handler, &result);
  }
  EXPECT_EQ(result.exception.get(), nullptr);
  EXPECT_EQ(result.calls, 0);

  {
    Env env;
    env.SetUnhandledExceptionHandler(handler, &result);
    env.ThrowNew(env.FindClass("java/lang/Exception"), "testing");
    EXPECT_EQ(result.calls, 0);
  }
  EXPECT_NE(result.exception.get(), nullptr);
  EXPECT_EQ(result.exception.GetMessage(env()), "testing");
  EXPECT_EQ(result.calls, 1);
}

#if __cpp_exceptions
TEST_F(EnvTest, DestructorCanThrow) {
  bool caught_exception = false;
  try {
    Env env;
    env.SetUnhandledExceptionHandler(GlobalUnhandledExceptionHandler, nullptr);

    env.ThrowNew(env.FindClass("java/lang/Exception"), "testing");

    // When `env` is destroyed with a pending exception, it will throw.
  } catch (const FirestoreException& e) {
    caught_exception = true;
    EXPECT_STREQ(e.what(), "testing");
  }
  EXPECT_TRUE(caught_exception);
}
#endif  // __cpp_exceptions

TEST_F(EnvTest, ObjectArrayOperations) {
  Env env;
  Local<Array<String>> array = env.NewArray<String>(2, String::GetClass());

  array.Set(env, 0, env.NewStringUtf("str"));
  Local<String> value = array.Get(env, 0);
  ASSERT_EQ(value.ToString(env), "str");

  value = array.Get(env, 1);
  ASSERT_EQ(value.get(), nullptr);
}

TEST_F(EnvTest, PrimitiveArrayOperations) {
  Env env;

  Class string_class = String::GetClass();
  jmethodID ctor =
      env.GetMethodId(string_class, "<init>", "([BLjava/lang/String;)V");
  jmethodID get_bytes =
      env.GetMethodId(string_class, "getBytes", "(Ljava/lang/String;)[B");

  Local<String> encoding = env.NewStringUtf("UTF-8");

  std::vector<uint8_t> blob = {'f', 'o', 'o'};
  Local<Array<uint8_t>> array = env.NewArray<uint8_t>(blob.size());
  env.SetArrayRegion(array, 0, blob.size(), &blob[0]);

  Local<String> str = env.New<String>(string_class, ctor, array, encoding);
  ASSERT_EQ(str.ToString(env), "foo");

  Local<Array<uint8_t>> str_bytes =
      env.Call<Array<uint8_t>>(str, get_bytes, encoding);

  std::vector<uint8_t> result(2);
  env.GetArrayRegion(str_bytes, 1, 2, &result[0]);

  std::vector<uint8_t> expected = {'o', 'o'};
  ASSERT_EQ(expected, result);

  result = env.GetArrayRegion<uint8_t>(str_bytes, 2, 1);
  expected = {'o'};
  ASSERT_EQ(expected, result);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
