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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_WRITE_TREE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_WRITE_TREE_H_

#include <string>
#include <vector>
#include "app/src/include/firebase/variant.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

class MockWriteTree : public WriteTree {
 public:
  MOCK_METHOD(Optional<Variant>, CalcCompleteEventCache,
              (const Path& tree_path, const Variant* complete_server_cache),
              (const, override));

  MOCK_METHOD(Optional<Variant>, CalcCompleteEventCache,
              (const Path& tree_path, const Variant* complete_server_cache,
               const std::vector<WriteId>& write_ids_to_exclude),
              (const, override));

  MOCK_METHOD(Optional<Variant>, CalcCompleteEventCache,
              (const Path& tree_path, const Variant* complete_server_cache,
               const std::vector<WriteId>& write_ids_to_exclude,
               HiddenWriteInclusion include_hidden_writes),
              (const, override));

  MOCK_METHOD(Variant, CalcCompleteEventChildren,
              (const Path& tree_path, const Variant& complete_server_children),
              (const, override));

  MOCK_METHOD(Optional<Variant>, CalcEventCacheAfterServerOverwrite,
              (const Path& tree_path, const Path& path,
               const Variant* existing_local_snap,
               const Variant* existing_server_snap),
              (const, override));

  MOCK_METHOD((Optional<std::pair<Variant, Variant>>), CalcNextVariantAfterPost,
              (const Path& tree_path,
               const Optional<Variant>& complete_server_data,
               (const std::pair<Variant, Variant>& post),
               IterationDirection direction, const QueryParams& params),
              (const, override));

  MOCK_METHOD(Optional<Variant>, ShadowingWrite, (const Path& path),
              (const, override));

  MOCK_METHOD(Optional<Variant>, CalcCompleteChild,
              (const Path& tree_path, const std::string& child_key,
               const CacheNode& existing_server_cache),
              (const, override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_WRITE_TREE_H_
