// This is a sanity test using gtest. The goal of this test is to make sure the
// way we setup Android C++ test harness actually works. We write test in a
// cross-platform way with gtest and run test with Android JUnit4 test runner
// for Android. We want this sanity test be as simple as possible while using
// the most critical mechanism of gtest. We also print information to stdout
// for debugging if anything goes wrong.

#include <cstdio>
#include <cstdlib>
#include "gtest/gtest.h"

class SanityTest : public testing::Test {
 protected:
  void SetUp() override { printf("==== SetUp ====\n"); }
  void TearDown() override { printf("==== TearDown ====\n"); }
};

// So far, Android native method cannot be inside namespace. So this has to be
// defined outside of any namespace.
TEST_F(SanityTest, TestSanity) {
  printf("==== running %s ====\n", __PRETTY_FUNCTION__);
  EXPECT_TRUE(true);
}

TEST_F(SanityTest, TestAnotherSanity) {
  printf("==== running %s ====\n", __PRETTY_FUNCTION__);
  EXPECT_EQ(1, 1);
}

// Generally we do not put test inside #if's because Android test harness will
// generate JUnit test whether macro is true or false. It is fine here since the
// test is enabled for Android.
#if __cpp_exceptions
TEST_F(SanityTest, TestThrow) {
  printf("==== running %s ====\n", __PRETTY_FUNCTION__);
  EXPECT_ANY_THROW({ throw "exception"; });
}
#endif  // __cpp_exceptions
