// Copyright 2018 Google LLC
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

#include "database/src/desktop/view/view_processor.h"

#include "app/src/variant_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/indexed_filter.h"

// There are four types of operations we can apply: Overwrites, Merges,
// AckUserWrites, and ListenCompletes. Overwrites and merges can come from
// either the client or the server. AckUserWrites and ListenCompletes only come
// from the server. A test has been written for each combination of Operation
// type and operation source, and in the cases where there are significantly
// diverging code paths within a given conbination, multiple tests have been
// written to test each code path.

using ::testing::Eq;
using ::testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(ViewProcessor, Constructor) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // No tests, just making sure the indexed filter doesn't leak after
  // destruction.
}

// Apply an Overwrite operation that was initiated by the user, using an empty
// path.
TEST(ViewProcessor, ApplyOperationUserOverwrite_WithEmptyPath) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Create a user-initiated overwrite with an empty path to change a value.
  Operation operation =
      Operation::Overwrite(OperationSource::kUser, Path(), Variant("apples"));

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Only the local cache should change.
  CacheNode expected_local_cache(Variant("apples"), true, false);
  ViewCache expected_view_cache(expected_local_cache, initial_server_cache);

  // Expect just a value change event.
  std::vector<Change> expected_changes{
      ValueChange(IndexedVariant(Variant("apples"))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

// Apply an Overwrite operation that was initiated by the user, using a
// .priority path.
TEST(ViewProcessor, ApplyOperationUserOverwrite_WithPriorityPath) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Create a user-initiated overwrite with an empty path to change a value.
  Operation operation = Operation::Overwrite(OperationSource::kUser,
                                             Path(".priority"), Variant(100));
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Only the local cache should change.
  CacheNode expected_local_cache(CombineValueAndPriority("local_values", 100),
                                 true, false);
  ViewCache expected_view_cache(expected_local_cache, initial_server_cache);

  // Expect just a value change event.
  std::vector<Change> expected_changes{
      ValueChange(IndexedVariant(CombineValueAndPriority("local_values", 100))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

// Apply an Overwrite operation that was initiated by the user, regular
// non-empty path.
TEST(ViewProcessor, ApplyOperationUserOverwrite_WithRegularPath) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Create a user-initiated overwrite with a non-empty path to change a value.
  Operation operation = Operation::Overwrite(
      OperationSource::kUser, Path("aaa/bbb"), Variant("apples"));

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path("aaa/bbb"));
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Only the local cache should change.
  CacheNode expected_local_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "apples"),
                         }),
      }),
      true, false);
  ViewCache expected_view_cache(expected_local_cache, initial_server_cache);

  // Expect one ChildChanged event and one Value event.
  std::vector<Change> expected_changes{
      ChildAddedChange("aaa",
                       std::map<Variant, Variant>{
                           std::make_pair("bbb", "apples"),
                       }),
      ValueChange(IndexedVariant(Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "apples"),
                         }),
      }))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

// Apply an Overwrite operation that was initiated by the server, using an empty
// path.
TEST(ViewProcessor, ApplyOperationServerOverwrite_WithEmptyPath) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Create a server-initiated overwrite with an empty path to change a value.
  Operation operation =
      Operation::Overwrite(OperationSource::kServer, Path(), Variant("apples"));
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Both the local and server caches have been set.
  CacheNode expected_cache(Variant("apples"), true, false);
  ViewCache expected_view_cache(expected_cache, expected_cache);

  // Expect just a value change event.
  std::vector<Change> expected_changes{
      ValueChange(IndexedVariant(Variant("apples"))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

// Apply an Overwrite operation that was initiated by the server, using a
// regular path.
TEST(ViewProcessor, ApplyOperationServerOverwrite_RegularPath) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  Operation operation =
      Operation::Overwrite(OperationSource::kServer, Path("aaa"),
                           Variant(std::map<Variant, Variant>{
                               std::make_pair("bbb", "apples"),
                           }));

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Both caches are expected to be the same.
  CacheNode expected_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "apples"),
                         }),
      }),
      true, false);
  ViewCache expected_view_cache(expected_cache, expected_cache);

  // Expect one ChildAdded event and one Value event.
  std::vector<Change> expected_changes{
      ChildAddedChange("aaa",
                       std::map<Variant, Variant>{
                           std::make_pair("bbb", "apples"),
                       }),
      ValueChange(IndexedVariant(Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "apples"),
                         }),
      }))),
  };
  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

// Apply an Overwrite operation that was initiated by the server, using a path
// that is deeper than a direct child of the location.
TEST(ViewProcessor, ApplyOperationServerOverwrite_DistantDescendantChange) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_cache(Variant(std::map<Variant, Variant>{std::make_pair(
                              "aaa",
                              std::map<Variant, Variant>{std::make_pair(
                                  "bbb",
                                  std::map<Variant, Variant>{
                                      std::make_pair("ccc", 1000),
                                  })})}),
                          true, false);
  ViewCache old_view_cache(initial_cache, initial_cache);

  // Make sure the data being updated is deeply nested in the variant.
  Operation operation = Operation::Overwrite(
      OperationSource::kServer, Path("aaa/bbb/ccc"), Variant(-9999));

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Both caches are expected to be the same.
  CacheNode expected_cache(Variant(std::map<Variant, Variant>{std::make_pair(
                               "aaa",
                               std::map<Variant, Variant>{std::make_pair(
                                   "bbb",
                                   std::map<Variant, Variant>{
                                       std::make_pair("ccc", -9999),
                                   })})}),
                           true, false);
  ViewCache expected_view_cache(expected_cache, expected_cache);

  // Expect one ChildChanged event and one Value event.
  std::vector<Change> expected_changes{
      ChildChangedChange("aaa",
                         IndexedVariant(Variant(std::map<Variant, Variant>{
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", -9999),
                                            })})),
                         IndexedVariant(Variant(std::map<Variant, Variant>{
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 1000),
                                            })}))),
      ValueChange(IndexedVariant(Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", -9999),
                                            })})}))),
  };
  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

// Apply a Merge operation that was initiated by the user.
TEST(ViewProcessor, ApplyOperationUserMerge) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));
  // Set up some dummy data.
  CacheNode initial_local_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "zzz"),
                         }),
      }),
      true, false);
  CacheNode initial_server_cache(Variant("aaa"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);
  // The merge operation should consist of multiple changes in different
  // locations.
  CompoundWrite write =
      CompoundWrite()
          .AddWrite(Path("aaa/bbb/ccc"), Variant("apples"))
          .AddWrite(Path("aaa/ddd"), Variant("bananas"))
          .AddWrite(Path("aaa/eee/fff"), Variant("vegetables"));
  Operation operation = Operation::Merge(OperationSource::kUser, Path(), write);
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Only the local cache should change.
  CacheNode expected_local_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair(
              "aaa",
              std::map<Variant, Variant>{
                  std::make_pair("bbb",
                                 std::map<Variant, Variant>{
                                     std::make_pair("ccc", "apples"),
                                 }),
                  std::make_pair("ddd", "bananas"),
                  std::make_pair("eee",
                                 std::map<Variant, Variant>{
                                     std::make_pair("fff", "vegetables"),
                                 }),
              }),
      }),
      true, false);
  CacheNode expected_server_cache(Variant("aaa"), true, false);
  ViewCache expected_view_cache(expected_local_cache, expected_server_cache);

  // Expect one ChildChanged event and one Value event.
  std::vector<Change> expected_changes{
      ChildChangedChange(
          "aaa",
          Variant(std::map<Variant, Variant>{
              std::make_pair("bbb",
                             std::map<Variant, Variant>{
                                 std::make_pair("ccc", "apples"),
                             }),
              std::make_pair("ddd", "bananas"),
              std::make_pair("eee",
                             std::map<Variant, Variant>{
                                 std::make_pair("fff", "vegetables"),
                             }),
          }),
          Variant(std::map<Variant, Variant>{
              std::make_pair("bbb", "zzz"),
          })),
      ValueChange(IndexedVariant(Variant(std::map<Variant, Variant>{
          std::make_pair(
              "aaa",
              std::map<Variant, Variant>{
                  std::make_pair("bbb",
                                 std::map<Variant, Variant>{
                                     std::make_pair("ccc", "apples"),
                                 }),
                  std::make_pair("ddd", "bananas"),
                  std::make_pair("eee",
                                 std::map<Variant, Variant>{
                                     std::make_pair("fff", "vegetables"),
                                 }),
              }),
      }))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

TEST(ViewProcessor, ApplyOperationServerMerge) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "zzz"),
                         }),
      }),
      true, false);
  CacheNode initial_server_cache(Variant("aaa"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // The merge operation should consist of multiple changes in different
  // locations.
  CompoundWrite write =
      CompoundWrite()
          .AddWrite(Path("bbb/ccc"), Variant("apples"))
          .AddWrite(Path("bbb/ddd"), Variant("bananas"))
          .AddWrite(Path("bbb/eee/fff"), Variant("vegetables"));
  Operation operation =
      Operation::Merge(OperationSource::kServer, Path("aaa"), write);

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path("aaa"));
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Both caches are expected to be the same.
  CacheNode expected_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair(
              "aaa",
              std::map<Variant, Variant>{
                  std::make_pair(
                      "bbb",
                      std::map<Variant, Variant>{
                          std::make_pair("ccc", "apples"),
                          std::make_pair("ddd", "bananas"),
                          std::make_pair(
                              "eee",
                              std::map<Variant, Variant>{
                                  std::make_pair("fff", "vegetables"),
                              }),
                      }),
              }),
      }),
      true, false);
  ViewCache expected_view_cache(expected_cache, expected_cache);

  // Expect one ChildChanged event and one Value event.
  std::vector<Change> expected_changes{
      ChildChangedChange(
          "aaa",
          Variant(std::map<Variant, Variant>{
              std::make_pair(
                  "bbb",
                  std::map<Variant, Variant>{
                      std::make_pair("ccc", "apples"),
                      std::make_pair("ddd", "bananas"),
                      std::make_pair("eee",
                                     std::map<Variant, Variant>{
                                         std::make_pair("fff", "vegetables"),
                                     }),
                  }),
          }),
          Variant(std::map<Variant, Variant>{
              std::make_pair("bbb", "zzz"),
          })),
      ValueChange(IndexedVariant(Variant(std::map<Variant, Variant>{
          std::make_pair(
              "aaa",
              std::map<Variant, Variant>{
                  std::make_pair(
                      "bbb",
                      std::map<Variant, Variant>{
                          std::make_pair("ccc", "apples"),
                          std::make_pair("ddd", "bananas"),
                          std::make_pair(
                              "eee",
                              std::map<Variant, Variant>{
                                  std::make_pair("fff", "vegetables"),
                              }),
                      }),
              }),
      }))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

TEST(ViewProcessor, ApplyOperationAck_HasShadowingWrite) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Create an Ack with a shadowing write.
  // These values don't matter for this test because the shadowing write will
  // short circuit everything.
  Tree<bool> affected_tree;
  Operation operation =
      Operation::AckUserWrite(Path("aaa"), affected_tree, kAckConfirm);

  // Set up shadowing write.
  WriteTree writes_cache;
  writes_cache.AddOverwrite(Path("aaa"), Variant("overwrite"), 100,
                            kOverwriteVisible);
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Expect no changes in the view cache.
  ViewCache expected_view_cache = old_view_cache;

  // Expect no Changes as a result of this.
  std::vector<Change> expected_changes;

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

TEST(ViewProcessor, ApplyOperationAck_IsOverwrite) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "new_value"),
                         }),
      }),
      true, false);
  CacheNode initial_server_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "new_value"),
                         }),
      }),
      true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  Tree<bool> affected_tree;
  affected_tree.set_value(true);
  affected_tree.SetValueAt(Path("aaa/bbb"), true);
  Operation operation =
      Operation::AckUserWrite(Path(), affected_tree, kAckConfirm);

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  writes_cache.AddOverwrite(Path("aaa/bbb"), "new_value", 1234,
                            kOverwriteVisible);
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Expect no changes in the view cache.
  ViewCache expected_view_cache(initial_local_cache, initial_server_cache);

  // Expect no Changes as a result of this.
  std::vector<Change> expected_changes;

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

TEST(ViewProcessor, ApplyOperationAckRevert) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "new_value"),
                         }),
      }),
      true, false);
  CacheNode initial_server_cache(
      Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "old_value"),
                         }),
      }),
      true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Mark the value we're going to be reverting.
  Tree<bool> affected_tree;
  affected_tree.set_value(true);
  affected_tree.SetValueAt(Path("aaa/bbb"), true);
  Operation operation =
      Operation::AckUserWrite(Path(), affected_tree, kAckRevert);

  // Hold the old value in the writes cache.
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  writes_cache.AddOverwrite(Path("aaa/bbb"), "old_value", 1234,
                            kOverwriteVisible);
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // Expect that the local cache gets reverted to the old value.
  ViewCache expected_view_cache(initial_server_cache, initial_server_cache);

  // Expect a ChildChanged and Value Changes, setting things back to the old
  // value.
  std::vector<Change> expected_changes{
      ChildChangedChange("aaa",
                         Variant(std::map<Variant, Variant>{
                             std::make_pair("bbb", "old_value"),
                         }),
                         Variant(std::map<Variant, Variant>{
                             std::make_pair("bbb", "new_value"),
                         })),
      ValueChange(IndexedVariant(Variant(std::map<Variant, Variant>{
          std::make_pair("aaa",
                         std::map<Variant, Variant>{
                             std::make_pair("bbb", "old_value"),
                         }),
      }))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

TEST(ViewProcessor, ApplyOperationListenComplete) {
  ViewProcessor view_processor(MakeUnique<IndexedFilter>(QueryParams()));

  // Set up some dummy data.
  CacheNode initial_local_cache(Variant("local_values"), true, false);
  CacheNode initial_server_cache(Variant("server_values"), true, false);
  ViewCache old_view_cache(initial_local_cache, initial_server_cache);

  // Create a server-initiated listen complete with an empty path to change a
  // value.
  Operation operation =
      Operation::ListenComplete(OperationSource::kServer, Path());

  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref = writes_cache.ChildWrites(Path());
  Variant complete_cache;

  // Apply the operation.
  ViewCache resultant_view_cache;
  std::vector<Change> resultant_changes;
  view_processor.ApplyOperation(old_view_cache, operation, writes_cache_ref,
                                &complete_cache, &resultant_view_cache,
                                &resultant_changes);

  // The local cache should now reflect the server cache.
  ViewCache expected_view_cache(initial_server_cache, initial_server_cache);

  // Expect just a value change event.
  std::vector<Change> expected_changes{
      ValueChange(IndexedVariant(Variant("server_values"))),
  };

  EXPECT_EQ(resultant_view_cache, expected_view_cache);
  EXPECT_THAT(resultant_changes, Pointwise(Eq(), expected_changes));
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
