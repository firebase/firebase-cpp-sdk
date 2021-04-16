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

#include <cstdint>
#include <limits>

namespace firebase {
namespace database {
namespace internal {

const uint64_t kServerUpdatesBetweenCacheSizeChecks = 1000;

const uint64_t kMaxNumberOfPrunableQueriesToKeep = 1000;

// 20% at a time until we're below our max.
const double kPercentOfQueriesToPruneAtOnce = 0.2;

CachePolicy::~CachePolicy() {}

LRUCachePolicy::LRUCachePolicy(uint64_t max_size_bytes)
    : max_size_bytes_(max_size_bytes) {}

LRUCachePolicy::~LRUCachePolicy() {}

bool LRUCachePolicy::ShouldPrune(uint64_t current_size_bytes,
                                 uint64_t count_of_prunable_queries) const {
  return current_size_bytes > max_size_bytes_ ||
         count_of_prunable_queries > kMaxNumberOfPrunableQueriesToKeep;
}

bool LRUCachePolicy::ShouldCheckCacheSize(
    uint64_t server_updates_since_last_check) const {
  return server_updates_since_last_check > kServerUpdatesBetweenCacheSizeChecks;
}

double LRUCachePolicy::GetPercentOfQueriesToPruneAtOnce() const {
  return kPercentOfQueriesToPruneAtOnce;
}

uint64_t LRUCachePolicy::GetMaxNumberOfQueriesToKeep() const {
  return kMaxNumberOfPrunableQueriesToKeep;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
