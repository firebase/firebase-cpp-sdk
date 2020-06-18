/*
 * Copyright 2017 Google LLC
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

#include "app/memory/atomic.h"

#include <thread>  // NOLINT
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

// Basic sanity tests for atomic operations.

namespace firebase {
namespace compat {
namespace {

using ::testing::Eq;

const uint64_t kValue = 10;
const uint64_t kUpdatedValue = 20;

TEST(AtomicTest, DefaultConstructedAtomicIsEqualToZero) {
  Atomic<uint64_t> atomic;
  EXPECT_THAT(atomic.load(), Eq(0));
}

TEST(AtomicTest, AssignedValueIsProperlyLoadedViaLoad) {
  Atomic<uint64_t> atomic(kValue);
  EXPECT_THAT(atomic.load(), Eq(kValue));
}

TEST(AtomicTest, FetchAddProperlyAddsValueAndReturnsValueBeforeAddition) {
  Atomic<uint64_t> atomic(kValue);
  EXPECT_THAT(atomic.fetch_add(kValue), kValue);
  EXPECT_THAT(atomic.load(), Eq(2 * kValue));
}

TEST(AtomicTest,
     FetchSubProperlySubtractsValueAndReturnsValueBeforeSubtraction) {
  Atomic<uint64_t> atomic(kValue);
  EXPECT_THAT(atomic.fetch_sub(kValue), kValue);
  EXPECT_THAT(atomic.load(), Eq(0));
}

TEST(AtomicTest, NewValueIsProperlyAssignedWithAssignmentOperator) {
  Atomic<uint64_t> atomic;
  atomic = kValue;
  EXPECT_THAT(atomic.load(), Eq(kValue));
}

// Note: This test needs to spin and can't use synchronization like
// mutex+condvar because their use renders the test useless due to the fact that
// in the presence of synchronization non-atomic updates are also guaranteed to
// be visible across threads.
TEST(AtomicTest, AtomicUpdatesAreVisibleAcrossThreads) {
  Atomic<uint64_t> atomic(kValue);

  std::thread thread([&atomic]() {
    while (atomic.load() == kValue) {
    }
    atomic.fetch_add(1);
  });
  atomic.store(kUpdatedValue);
  thread.join();

  EXPECT_THAT(atomic.load(), Eq(kUpdatedValue + 1));
}

TEST(AtomicTest, AtomicUpdatesAreVisibleAcrossMultipleThreads) {
  Atomic<uint64_t> atomic;

  const int num_threads = 10;
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&atomic] { atomic.fetch_add(1); });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_THAT(atomic.load(), Eq(num_threads));
}

}  // namespace
}  // namespace compat
}  // namespace firebase
