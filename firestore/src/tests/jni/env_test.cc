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
