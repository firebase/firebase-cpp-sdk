// Copyright 2020 Google
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#include <jni.h>
#include "testing/run_all_tests.h"
#elif defined(__APPLE__) && TARGET_OS_IPHONE
#include "testing/ticker_ios.h"
#else  // defined(FIREBASE_ANDROID_FOR_DESKTOP), defined(__APPLE__)
#include "testing/ticker_desktop.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP), defined(__APPLE__)

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/ticker.h"

namespace firebase {
namespace testing {
namespace cppsdk {

// We count the status change by this.
static int g_status_count = 0;

// Now we define example ticker class. TickerObserver is abstract and we cannot
// test it directly. Generally speaking, fakes mimic callbacks by inheriting
// TickerObserver class and overriding Update() method.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_testing_cppsdk_TickerExample_nativeFunction(
    JNIEnv* env, jobject this_obj, jlong ticker, jlong delay) {
  if (ticker == delay) {
    ++g_status_count;
  }
}

class Tickers {
 public:
  Tickers(std::initializer_list<int64_t> delays) {
    JNIEnv* android_jni_env = GetTestJniEnv();
    jclass class_obj = android_jni_env->FindClass(
        "com/google/testing/TickerExample");
    jmethodID methid_id =
        android_jni_env->GetMethodID(class_obj, "<init>", "(J)V");
    for (int64_t delay : delays) {
      jobject observer =
          android_jni_env->NewObject(class_obj, methid_id, delay);
      android_jni_env->DeleteLocalRef(observer);
    }
    android_jni_env->DeleteLocalRef(class_obj);
  }
};
#else   // defined(FIREBASE_ANDROID_FOR_DESKTOP)
class TickerExample : public TickerObserver {
 public:
  explicit TickerExample(int64_t delay) : delay_(delay) {
    RegisterTicker(this);
  }

  void Elapse() override {
    if (TickerNow() == delay_) {
      ++g_status_count;
    }
  }

 private:
  // When the callback should happen.
  const int64_t delay_;
};

class Tickers {
 public:
  Tickers(std::initializer_list<int64_t> delays) {
    for (int64_t delay : delays) {
      tickers_.push_back(
          std::shared_ptr<TickerExample>(new TickerExample(delay)));
    }
  }

 private:
  std::vector<std::shared_ptr<TickerExample> > tickers_;
};
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

class TickerTest : public ::testing::Test {
 protected:
  void SetUp() override { g_status_count = 0; }
  void TearDown() override { TickerReset(); }
};

// This test make sure nothing is broken by calling a sequence of elapse and
// reset. Since there is no observer, we do not have anything to verify yet.
TEST_F(TickerTest, TestNoObserver) {
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(0, g_status_count);

  TickerReset();
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(0, g_status_count);
}

// Test one observer that changes status immediately.
TEST_F(TickerTest, TestObserverCallbackImmediate) {
  Tickers tickers({0L});

  // Now verify the status changed immediately.
  EXPECT_EQ(1, g_status_count);
  TickerElapse();
  EXPECT_EQ(1, g_status_count);
  TickerElapse();
  EXPECT_EQ(1, g_status_count);
  TickerElapse();
  EXPECT_EQ(1, g_status_count);
}

// Test one observer that changes status after two tickers.
TEST_F(TickerTest, TestObserverDelayTwo) {
  Tickers tickers({2L});

  // Now start the ticker and verify.
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(1, g_status_count);
  TickerElapse();
  EXPECT_EQ(1, g_status_count);
}

// Test two observers that changes status after one, respectively, two tickers.
TEST_F(TickerTest, TestMultipleObservers) {
  Tickers tickers({1L, 2L});

  // Now start the ticker and verify.
  EXPECT_EQ(0, g_status_count);
  TickerElapse();
  EXPECT_EQ(1, g_status_count);
  TickerElapse();
  EXPECT_EQ(2, g_status_count);
  TickerElapse();
  EXPECT_EQ(2, g_status_count);
}
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
