/*
 * Copyright 2024 Google LLC
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

#include "firestore/src/include/firebase/firestore.h"

#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/compiler_info.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/main/promise_factory_main.h"
#include "firestore/src/main/promise_main.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

void TestThrowHandler(util::ExceptionType type,
                      const char* /*file*/,
                      const char* /*func*/,
                      int /*line*/,
                      const std::string& message) {
  switch (type) {
    case util::ExceptionType::InvalidArgument:
      throw std::invalid_argument(message);
    case util::ExceptionType::IllegalState:
      throw std::logic_error(message);
    case util::ExceptionType::AssertionFailure:
      throw std::runtime_error(message);
  }
  throw std::runtime_error(message);
}

class FirestoreUnitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_handler_ = util::SetThrowHandler(TestThrowHandler);
  }

  void TearDown() override {
    util::SetThrowHandler(previous_handler_);
  }

  util::ThrowHandler previous_handler_ = nullptr;
};

TEST_F(FirestoreUnitTest, FieldPathLifecycleAndHashing) {
  FieldPath invalid_path;
  EXPECT_FALSE(invalid_path.is_valid());

  // Copy construction and assignment from an invalid FieldPath must be safe.
  FieldPath copy_invalid(invalid_path);
  EXPECT_FALSE(copy_invalid.is_valid());

  FieldPath valid_path({"a", "b"});
  EXPECT_TRUE(valid_path.is_valid());
  EXPECT_EQ(valid_path.ToString(), "a.b");

  valid_path = invalid_path;
  EXPECT_FALSE(valid_path.is_valid());

  // Hash of invalid FieldPath must not crash.
  std::hash<FieldPath> hasher;
  EXPECT_EQ(hasher(invalid_path), 0u);

  // Move construction and move assignment must null out source and not leak.
  FieldPath src({"x", "y"});
  FieldPath dst({"old"});
  dst = std::move(src);
  EXPECT_TRUE(dst.is_valid());
  EXPECT_EQ(dst.ToString(), "x.y");
  EXPECT_FALSE(src.is_valid());

  // Validation of invalid field paths.
  EXPECT_THROW(FieldPath({""}), std::invalid_argument);
  EXPECT_THROW(SetOptions::MergeFields({"a..b"}), std::invalid_argument);
  EXPECT_THROW(SetOptions::MergeFields({"a*b"}), std::invalid_argument);
}

TEST_F(FirestoreUnitTest, GeoPointAndTimestampOperations) {
  GeoPoint gp(37.7749, -122.4194);
  EXPECT_DOUBLE_EQ(gp.latitude(), 37.7749);
  EXPECT_DOUBLE_EQ(gp.longitude(), -122.4194);
  EXPECT_EQ(gp, GeoPoint(37.7749, -122.4194));
  EXPECT_NE(gp.ToString(), "");
  EXPECT_THROW(GeoPoint(95.0, 0.0), std::runtime_error);
  EXPECT_THROW(GeoPoint(0.0, 185.0), std::runtime_error);

  Timestamp ts(1700000000, 500);
  EXPECT_EQ(ts.seconds(), 1700000000);
  EXPECT_EQ(ts.nanoseconds(), 500);
  EXPECT_EQ(ts, Timestamp(1700000000, 500));
  EXPECT_NE(ts.ToString(), "");
  EXPECT_THROW(Timestamp(0, -1), std::runtime_error);
  EXPECT_THROW(Timestamp(0, 1000000000), std::runtime_error);
}

TEST_F(FirestoreUnitTest, SettingsAndMetadataFormatting) {
  Settings settings;
  settings.set_host("localhost:8080");
  settings.set_ssl_enabled(false);
  settings.set_persistence_enabled(false);
  settings.set_cache_size_bytes(Settings::kCacheSizeUnlimited);

  std::string str = settings.ToString();
  EXPECT_NE(str.find("host='localhost:8080'"), std::string::npos);
  EXPECT_NE(str.find("ssl_enabled=false"), std::string::npos);
  EXPECT_NE(str.find("persistence_enabled=false"), std::string::npos);

  Settings copy = settings;
  EXPECT_EQ(copy, settings);
  copy.set_ssl_enabled(true);
  EXPECT_NE(copy, settings);

  SnapshotMetadata meta_a(false, false);
  SnapshotMetadata meta_b(true, false);
  SnapshotMetadata meta_c(false, true);
  SnapshotMetadata meta_d(true, true);
  EXPECT_NE(meta_a, meta_b);
  EXPECT_NE(meta_a, meta_c);
  EXPECT_NE(meta_b, meta_d);
  EXPECT_EQ(meta_a, SnapshotMetadata(false, false));

  LoadBundleTaskProgress progress(
      2, 5, 100, 250, LoadBundleTaskProgress::State::kInProgress);
  EXPECT_EQ(progress.documents_loaded(), 2);
  EXPECT_EQ(progress.total_documents(), 5);
  EXPECT_EQ(progress.bytes_loaded(), 100);
  EXPECT_EQ(progress.total_bytes(), 250);
  EXPECT_EQ(progress.state(), LoadBundleTaskProgress::State::kInProgress);
  EXPECT_EQ(progress,
            LoadBundleTaskProgress(
                2, 5, 100, 250, LoadBundleTaskProgress::State::kInProgress));

  SetOptions merge_all = SetOptions::Merge();
  SetOptions merge_fields = SetOptions::MergeFields({"a", "b"});
  EXPECT_EQ(merge_all, SetOptions::Merge());
  EXPECT_NE(merge_all, merge_fields);

  TransactionOptions tx_opts;
  EXPECT_EQ(tx_opts.max_attempts(), 5);
  tx_opts.set_max_attempts(10);
  EXPECT_EQ(tx_opts.max_attempts(), 10);
  EXPECT_THROW(tx_opts.set_max_attempts(0), std::invalid_argument);
  EXPECT_THROW(tx_opts.set_max_attempts(-1), std::invalid_argument);

  EXPECT_NE(GetFullCompilerInfo(), "");
}

TEST_F(FirestoreUnitTest, CleanupNotifierRegistersAndCleansUpAllItems) {
  struct DummyItem {
    bool cleaned = false;
  };
  DummyItem a;
  DummyItem b;
  {
    CleanupNotifier cleanup;
    cleanup.RegisterObject(&a, [](void* item) {
      static_cast<DummyItem*>(item)->cleaned = true;
    });
    cleanup.RegisterObject(&b, [](void* item) {
      static_cast<DummyItem*>(item)->cleaned = true;
    });
    cleanup.UnregisterObject(&a);
  }
  EXPECT_FALSE(a.cleaned);
  EXPECT_TRUE(b.cleaned);
}

namespace {
enum class TestAsyncOp {
  kOp = 0,
  kCount,
};
}  // namespace

TEST_F(FirestoreUnitTest, PromiseAndPromiseFactoryMoveAndCleanupSafety) {
  FutureManager future_mgr;

  {
    CleanupNotifier cleanup;
    PromiseFactory<TestAsyncOp> factory1(&cleanup, &future_mgr);
    Promise<int> promise = factory1.CreatePromise<int>(TestAsyncOp::kOp);

    // Self-copy and self-move assignment must preserve the underlying state.
    auto& promise_ref = promise;
    promise = promise_ref;
    promise = std::move(promise_ref);

    // Moving PromiseFactory must not clean up futures when moved-from factory
    // is destroyed.
    PromiseFactory<TestAsyncOp> factory2(std::move(factory1));
    EXPECT_FALSE(promise.IsCleanedUp());

    Future<int> fut = promise.future();
    EXPECT_EQ(fut.status(), kFutureStatusPending);
    promise.SetValue(123);
    EXPECT_EQ(fut.status(), kFutureStatusComplete);
    EXPECT_EQ(fut.error(), 0);
    ASSERT_NE(fut.result(), nullptr);
    EXPECT_EQ(*fut.result(), 123);
  }

  // Triggering cleanup (via CleanupNotifier destruction) marks remaining
  // promises as cleaned up.
  auto cleanup2 = std::make_unique<CleanupNotifier>();
  PromiseFactory<TestAsyncOp> factory3(cleanup2.get(), &future_mgr);
  Promise<int> promise2 = factory3.CreatePromise<int>(TestAsyncOp::kOp);
  EXPECT_FALSE(promise2.IsCleanedUp());
  cleanup2.reset();
  EXPECT_TRUE(promise2.IsCleanedUp());
  // Setting value after cleanup must be a safe no-op.
  promise2.SetValue(456);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
