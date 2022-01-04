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

// This is a sanity test using gtest. The goal of this test is to make sure the
// way we setup Android C++ test harness actually works. We write test in a
// cross-platform way with gtest and run test with Android JUnit4 test runner
// for Android. We want this sanity test be as simple as possible while using
// the most critical mechanism of gtest. We also print information to stdout
// for debugging if anything goes wrong.

#include <cstdio>
#include <cstdlib>

#include "gtest/gtest.h"

#if defined(_WIN32)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif  // defined(_WIN32)

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
