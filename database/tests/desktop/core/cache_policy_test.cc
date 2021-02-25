// Copyright 2019 Google LLC
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

#include "database/src/desktop/core/cache_policy.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {

namespace {

TEST(LRUCachePolicy, ShouldPrune) {
  const uint64_t kMaxSizeBytes = 1000;
  LRUCachePolicy cache_policy(kMaxSizeBytes);

  uint64_t queries_to_keep = cache_policy.GetMaxNumberOfQueriesToKeep();
  EXPECT_EQ(queries_to_keep, 1000);

  // Should prune if the current number of bytes exceeds the max number of
  // bytes.
  EXPECT_TRUE(cache_policy.ShouldPrune(2000, 0));
  // Should prune if the number of prunable queries is greater than the maximum
  // number of prunable queries (defined in the LRUCachePolicy implementation).
  EXPECT_TRUE(cache_policy.ShouldPrune(0, 2000));
  // Should prune if both of the above are true.
  EXPECT_TRUE(cache_policy.ShouldPrune(2000, 2000));

  // Should not prune if at least one of the above conditions is not met.
  EXPECT_FALSE(cache_policy.ShouldPrune(0, 0));
}

TEST(LRUCachePolicy, ShouldCheckCacheSize) {
  const uint64_t kMaxSizeBytes = 1000;
  LRUCachePolicy cache_policy(kMaxSizeBytes);

  // Should check cache cize of the number of server updates is greater than
  // number of server updates between cache checks (defined in the
  // LRUCachePolicy implementation).
  EXPECT_TRUE(cache_policy.ShouldCheckCacheSize(2000));
  EXPECT_TRUE(cache_policy.ShouldCheckCacheSize(1001));
  EXPECT_FALSE(cache_policy.ShouldCheckCacheSize(1000));
  EXPECT_FALSE(cache_policy.ShouldCheckCacheSize(500));
}

TEST(LRUCachePolicy, GetPercentOfQueriesToPruneAtOnce) {
  const uint64_t kMaxSizeBytes = 1000;
  LRUCachePolicy cache_policy(kMaxSizeBytes);

  // This should be exactly 20%.
  EXPECT_EQ(cache_policy.GetPercentOfQueriesToPruneAtOnce(), .2);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
