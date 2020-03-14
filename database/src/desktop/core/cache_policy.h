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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_CACHE_POLICY_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_CACHE_POLICY_H_

#include <cstdint>

namespace firebase {
namespace database {
namespace internal {

class CachePolicy {
 public:
  virtual ~CachePolicy();

  virtual bool ShouldPrune(uint64_t current_size_bytes,
                           uint64_t count_of_prunable_queries) const = 0;

  virtual bool ShouldCheckCacheSize(
      uint64_t serverUpdatesSinceLastCheck) const = 0;

  virtual double GetPercentOfQueriesToPruneAtOnce() const = 0;

  virtual uint64_t GetMaxNumberOfQueriesToKeep() const = 0;
};

class LRUCachePolicy : public CachePolicy {
 public:
  explicit LRUCachePolicy(uint64_t max_size_bytes);

  ~LRUCachePolicy() override;

  bool ShouldPrune(uint64_t current_size_bytes,
                   uint64_t count_of_prunable_queries) const override;

  bool ShouldCheckCacheSize(
      uint64_t server_updates_since_last_check) const override;

  double GetPercentOfQueriesToPruneAtOnce() const override;

  uint64_t GetMaxNumberOfQueriesToKeep() const override;

 private:
  const uint64_t max_size_bytes_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_CACHE_POLICY_H_
