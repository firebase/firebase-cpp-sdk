// Copyright 2023 Google LLC
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

#include "performance/src/include/firebase/performance.h"

#include <memory>
#include <utility>

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "performance/src/include/firebase/performance/http_metric.h"
#include "performance/src/include/firebase/performance/trace.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {
namespace {

class PerformanceDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    app_.reset(testing::CreateApp());
    EXPECT_EQ(Initialize(*app_), kInitResultSuccess);
  }

  void TearDown() override {
    if (internal::IsInitialized()) {
      Terminate();
    }
    app_.reset();
  }

  std::unique_ptr<App> app_;
};

TEST_F(PerformanceDesktopTest, LifecycleAndCollectionToggle) {
  EXPECT_TRUE(internal::IsInitialized());
  EXPECT_TRUE(GetPerformanceCollectionEnabled());
  SetPerformanceCollectionEnabled(false);
  EXPECT_FALSE(GetPerformanceCollectionEnabled());
  SetPerformanceCollectionEnabled(true);
  EXPECT_TRUE(GetPerformanceCollectionEnabled());

  app_.reset();
  EXPECT_FALSE(internal::IsInitialized());
}

TEST_F(PerformanceDesktopTest, TraceStateAndMovedFromSafety) {
  Trace unstarted;
  EXPECT_FALSE(unstarted.is_started());
  EXPECT_EQ(unstarted.GetLongMetric("m"), 0);
  EXPECT_EQ(unstarted.GetAttribute("a"), "");

  unstarted.Create("created_only");
  EXPECT_FALSE(unstarted.is_started());
  unstarted.StartCreatedTrace();
  EXPECT_TRUE(unstarted.is_started());

  unstarted.SetMetric("m", 10);
  unstarted.IncrementMetric("m", 5);
  EXPECT_EQ(unstarted.GetLongMetric("m"), 15);
  EXPECT_EQ(unstarted.GetLongMetric(nullptr), 0);

  unstarted.SetAttribute("a", "v");
  EXPECT_EQ(unstarted.GetAttribute("a"), "v");
  unstarted.SetAttribute("a", nullptr);
  EXPECT_EQ(unstarted.GetAttribute("a"), "");

  Trace moved(std::move(unstarted));
  EXPECT_FALSE(unstarted.is_started());
  EXPECT_NO_THROW(unstarted.Stop());
  EXPECT_NO_THROW(unstarted.Cancel());
  EXPECT_EQ(unstarted.GetLongMetric("m"), 0);
  EXPECT_EQ(unstarted.GetAttribute("a"), "");

  EXPECT_TRUE(moved.is_started());
  moved.Stop();
  EXPECT_FALSE(moved.is_started());
}

TEST_F(PerformanceDesktopTest, HttpMetricStateAndMovedFromSafety) {
  HttpMetric metric;
  EXPECT_FALSE(metric.is_started());

  metric.Create("https://example.com", kHttpMethodGet);
  EXPECT_FALSE(metric.is_started());
  metric.StartCreatedHttpMetric();
  EXPECT_TRUE(metric.is_started());

  metric.SetAttribute("k", "val");
  EXPECT_EQ(metric.GetAttribute("k"), "val");
  metric.SetAttribute("k", nullptr);
  EXPECT_EQ(metric.GetAttribute("k"), "");

  metric.set_http_response_code(200);
  metric.set_request_payload_size(128);
  metric.set_response_payload_size(256);
  metric.set_response_content_type("application/json");
  metric.set_response_content_type(nullptr);

  HttpMetric moved = std::move(metric);
  EXPECT_FALSE(metric.is_started());
  EXPECT_NO_THROW(metric.Stop());
  EXPECT_NO_THROW(metric.Cancel());
  EXPECT_EQ(metric.GetAttribute("k"), "");

  EXPECT_TRUE(moved.is_started());
  moved.Cancel();
  EXPECT_FALSE(moved.is_started());
}

}  // namespace
}  // namespace performance
}  // namespace firebase
