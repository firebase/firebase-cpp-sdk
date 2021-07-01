// Copyright 2021 Google LLC

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define __ANDROID__
#include <jni.h>

#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/include/firebase/performance/http_metric.h"
#include "performance/src/include/firebase/performance/trace.h"
#include "performance/src/performance_common.h"

#ifdef __ANDROID__
#include "app/src/util_android.h"
#endif  // __ANDROID__

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"
#include "testing/reporter.h"

namespace firebase {
namespace performance {

class PerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::ConfigSet("{}");
    reporter_.reset();

    firebase_app_.reset(testing::CreateApp());
    AddExpectationAndroid("FirebasePerformance.getInstance", {});
    performance::Initialize(*firebase_app_);
  }

  void TearDown() override {
    firebase::testing::cppsdk::ConfigReset();
    Terminate();
    firebase_app_.reset();
    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  void AddExpectationAndroid(const char* fake,
                             std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kAndroid,
                             args);
  }

  void AddExpectationApple(const char* fake,
                           std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kIos, args);
  }

  std::unique_ptr<App> firebase_app_;
  firebase::testing::cppsdk::Reporter reporter_;
};

TEST_F(PerformanceTest, TestDestroyDefaultApp) {
  EXPECT_TRUE(internal::IsInitialized());
  firebase_app_.reset();
  EXPECT_FALSE(internal::IsInitialized());
}

TEST_F(PerformanceTest, TestSetPerformanceCollectionEnabled) {
  AddExpectationApple("-[FIRPerformance setDataCollectionEnabled:]", {"YES"});
  AddExpectationAndroid("FirebasePerformance.setPerformanceCollectionEnabled",
                        {"true"});
  SetPerformanceCollectionEnabled(true);
}

TEST_F(PerformanceTest, TestSetPerformanceCollectionDisabled) {
  AddExpectationApple("-[FIRPerformance setDataCollectionEnabled:]", {"NO"});
  AddExpectationAndroid("FirebasePerformance.setPerformanceCollectionEnabled",
                        {"false"});
  SetPerformanceCollectionEnabled(false);
}

}  // namespace performance
}  // namespace firebase
