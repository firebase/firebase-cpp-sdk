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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_CACHE_POLICY_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_CACHE_POLICY_H_

#include <cstdint>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/cache_policy.h"

namespace firebase {
namespace database {
namespace internal {

class MockCachePolicy : public CachePolicy {
 public:
  ~MockCachePolicy() override {}

  MOCK_METHOD(bool, ShouldPrune,
              (uint64_t current_size_bytes, uint64_t count_of_prunable_queries),
              (const, override));
  MOCK_METHOD(bool, ShouldCheckCacheSize,
              (uint64_t server_updates_since_last_check), (const, override));
  MOCK_METHOD(double, GetPercentOfQueriesToPruneAtOnce, (), (const, override));
  MOCK_METHOD(uint64_t, GetMaxNumberOfQueriesToKeep, (), (const, override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_CACHE_POLICY_H_
