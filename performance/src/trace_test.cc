// Copyright 2021 Google LLC

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#ifndef __ANDROID__
#define __ANDROID__
#endif  // __ANDROID__
#include <jni.h>

#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/include/firebase/performance/trace.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"
#include "testing/reporter.h"

namespace firebase {
namespace performance {

class TraceTest : public ::testing::Test {
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

  void AddExpectationApple(const char* fake,
                           std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kIos, args);
  }

  void AddExpectationAndroid(const char* fake,
                             std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kAndroid,
                             args);
  }

  std::unique_ptr<App> firebase_app_;
  firebase::testing::cppsdk::Reporter reporter_;
};

TEST_F(TraceTest, TestCreateAndDestroyTrace) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"my_codepath"});
  AddExpectationApple("-[FIRTrace start]", {});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("new Trace", {"my_codepath"});
  AddExpectationAndroid("Trace.start", {});
  AddExpectationAndroid("Trace.stop", {});

  Trace trace("my_codepath");
}

TEST_F(TraceTest, TestDelayedCreateTrace) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"my_codepath"});
  AddExpectationApple("-[FIRTrace start]", {});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("new Trace", {"my_codepath"});
  AddExpectationAndroid("Trace.start", {});
  AddExpectationAndroid("Trace.stop", {});

  Trace trace;
  trace.Start("my_codepath");
}

TEST_F(TraceTest, TestCreateTraceCppObject) {
  Trace trace;

  // No expectations, it shouldn't call into the native implementations.
}

TEST_F(TraceTest, TestTraceCreateButNotStart) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"my_codepath"});
  AddExpectationAndroid("new Trace", {"my_codepath"});

  Trace trace;
  trace.Create("my_codepath");
}

TEST_F(TraceTest, TestTraceStartAfterCreate) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"my_codepath"});
  AddExpectationApple("-[FIRTrace start]", {});
  // Create() + StartCreatedTrace() is the Unity two-phase API where Unity owns
  // the trace lifecycle (stop_on_destroy is false), so ~Trace() cancels rather
  // than stops the underlying native trace.

  AddExpectationAndroid("new Trace", {"my_codepath"});
  AddExpectationAndroid("Trace.start", {});

  Trace trace;
  trace.Create("my_codepath");
  trace.StartCreatedTrace();
}

TEST_F(TraceTest, TestMoveConstructor) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"my_codepath"});
  AddExpectationApple("-[FIRTrace start]", {});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("new Trace", {"my_codepath"});
  AddExpectationAndroid("Trace.start", {});
  AddExpectationAndroid("Trace.stop", {});

  Trace trace("my_codepath");
  EXPECT_TRUE(trace.is_started());

  Trace moved_trace(std::move(trace));
  EXPECT_FALSE(trace.is_started());
  EXPECT_TRUE(moved_trace.is_started());
}

TEST_F(TraceTest, TestMoveAssignment) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"trace_1"});
  AddExpectationApple("-[FIRTrace start]", {});
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"trace_2"});
  AddExpectationApple("-[FIRTrace start]", {});
  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("new Trace", {"trace_1"});
  AddExpectationAndroid("Trace.start", {});
  AddExpectationAndroid("new Trace", {"trace_2"});
  AddExpectationAndroid("Trace.start", {});
  AddExpectationAndroid("Trace.stop", {});
  AddExpectationAndroid("Trace.stop", {});

  Trace trace1("trace_1");
  Trace trace2("trace_2");
  trace1 = std::move(trace2);
  EXPECT_TRUE(trace1.is_started());
  EXPECT_FALSE(trace2.is_started());
}

TEST_F(TraceTest, TestCreateTraceWithNullName) {
  Trace trace(nullptr);
  EXPECT_FALSE(trace.is_started());
}

TEST_F(TraceTest, TestIsStarted) {
  AddExpectationApple("-[FIRTrace initTraceWithName:]", {"my_codepath"});
  AddExpectationApple("-[FIRTrace start]", {});

  AddExpectationAndroid("new Trace", {"my_codepath"});
  AddExpectationAndroid("Trace.start", {});

  Trace trace("my_codepath");
  EXPECT_TRUE(trace.is_started());

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  trace.Stop();
  EXPECT_FALSE(trace.is_started());
}

TEST_F(TraceTest, TestSetAttribute) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace setValue:forAttribute:]",
                      {"my_value", "my_attribute"});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("Trace.putAttribute", {"my_attribute", "my_value"});
  AddExpectationAndroid("Trace.stop", {});

  trace.SetAttribute("my_attribute", "my_value");
}

TEST_F(TraceTest, TestSetAttributeNullName) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  EXPECT_NO_THROW(trace.SetAttribute(nullptr, "my_value"));
}

TEST_F(TraceTest, TestSetAttributeNotStarted) {
  Trace trace("my_codepath");
  trace.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(trace.SetAttribute("my_attribute", "my_value"));
}

TEST_F(TraceTest, TestGetAttribute) {
  Trace trace("my_codepath");
  reporter_.reset();

  // FIRTrace (iOS fake), Trace.java (Android fake), and trace_stub.cc (desktop
  // stub) all persist attributes in-memory on the trace instance.
  AddExpectationApple("-[FIRTrace setValue:forAttribute:]",
                      {"my_value", "my_attribute"});
  AddExpectationApple("-[FIRTrace valueForAttribute:]", {"my_attribute"});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("Trace.putAttribute", {"my_attribute", "my_value"});
  AddExpectationAndroid("Trace.getAttribute", {"my_attribute"});
  AddExpectationAndroid("Trace.stop", {});

  trace.SetAttribute("my_attribute", "my_value");
  EXPECT_THAT(trace.GetAttribute("my_attribute"), ::testing::Eq("my_value"));
}

TEST_F(TraceTest, TestGetAttributeNullName) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  EXPECT_THAT(trace.GetAttribute(nullptr), ::testing::Eq(""));
}

TEST_F(TraceTest, TestGetAttributeNotStarted) {
  Trace trace("my_codepath");
  trace.Stop();
  reporter_.reset();

  EXPECT_THAT(trace.GetAttribute("my_attribute"), ::testing::Eq(""));
}

TEST_F(TraceTest, TestRemoveAttribute) {
  Trace trace("my_codepath");
  reporter_.reset();
  AddExpectationApple("-[FIRTrace removeAttribute:]", {"my_attribute"});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("Trace.removeAttribute", {"my_attribute"});
  AddExpectationAndroid("Trace.stop", {});

  trace.SetAttribute("my_attribute", nullptr);
}

TEST_F(TraceTest, TestRemoveAttributeNullName) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  EXPECT_NO_THROW(trace.SetAttribute(nullptr, nullptr));
}

TEST_F(TraceTest, TestRemoveAttributeNotStarted) {
  Trace trace("my_codepath");
  trace.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(trace.SetAttribute("my_attribute", nullptr));
  EXPECT_THAT(trace.GetAttribute("my_attribute"), ::testing::Eq(""));
}

TEST_F(TraceTest, TestSetMetric) {
  Trace trace("my_codepath");
  reporter_.reset();
  AddExpectationApple("-[FIRTrace setIntValue:forMetric:]",
                      {"my_metric", "2000"});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("Trace.putMetric", {"my_metric", "2000"});
  AddExpectationAndroid("Trace.stop", {});

  trace.SetMetric("my_metric", 2000);
}

TEST_F(TraceTest, TestSetMetricNullName) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  EXPECT_NO_THROW(trace.SetMetric(nullptr, 2000));
}

TEST_F(TraceTest, TestSetMetricNotStarted) {
  Trace trace("my_codepath");
  trace.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(trace.SetMetric("my_metric", 2000));
}

TEST_F(TraceTest, TestGetLongMetric) {
  Trace trace("my_codepath");
  reporter_.reset();

  // FIRTrace (iOS fake), Trace.java (Android fake), and trace_stub.cc (desktop
  // stub) all persist metrics in-memory on the trace instance.
  AddExpectationApple("-[FIRTrace setIntValue:forMetric:]",
                      {"my_metric", "2000"});
  AddExpectationApple("-[FIRTrace valueForIntMetric:]", {"my_metric"});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("Trace.putMetric", {"my_metric", "2000"});
  AddExpectationAndroid("Trace.getLongMetric", {"my_metric"});
  AddExpectationAndroid("Trace.stop", {});

  trace.SetMetric("my_metric", 2000);
  EXPECT_EQ(trace.GetLongMetric("my_metric"), 2000);
}

TEST_F(TraceTest, TestGetLongMetricNullName) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  EXPECT_EQ(trace.GetLongMetric(nullptr), 0);
}

TEST_F(TraceTest, TestGetLongMetricNotStarted) {
  Trace trace("my_codepath");
  trace.Stop();
  reporter_.reset();

  EXPECT_EQ(trace.GetLongMetric("my_metric"), 0);
}

TEST_F(TraceTest, TestIncrementMetric) {
  Trace trace("my_codepath");
  reporter_.reset();
  AddExpectationApple("-[FIRTrace incrementMetric:byInt:]", {"my_metric", "5"});
  AddExpectationApple("-[FIRTrace stop]", {});

  AddExpectationAndroid("Trace.incrementMetric", {"my_metric", "5"});
  AddExpectationAndroid("Trace.stop", {});

  trace.IncrementMetric("my_metric", 5);
}

TEST_F(TraceTest, TestIncrementMetricNullName) {
  Trace trace("my_codepath");
  reporter_.reset();

  AddExpectationApple("-[FIRTrace stop]", {});
  AddExpectationAndroid("Trace.stop", {});

  EXPECT_NO_THROW(trace.IncrementMetric(nullptr, 2000));
}

TEST_F(TraceTest, TestIncrementMetricNotStarted) {
  Trace trace("my_codepath");
  trace.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(trace.IncrementMetric("my_metric", 2000));
}

}  // namespace performance
}  // namespace firebase
