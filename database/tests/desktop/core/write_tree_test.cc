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

#include "database/src/desktop/core/write_tree.h"

#include "app/src/variant_util.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/tests/desktop/test/mock_write_tree.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::_;
using testing::Eq;
using testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(WriteTree, ChildWrites) {
  WriteTree write_tree;
  WriteTreeRef ref = write_tree.ChildWrites(Path("test/path"));

  EXPECT_EQ(ref.path(), Path("test/path"));
  EXPECT_EQ(ref.write_tree(), &write_tree);
}

TEST(WriteTree, AddOverwrite) {
  WriteTree write_tree;
  WriteTreeRef ref = write_tree.ChildWrites(Path("test/path"));

  EXPECT_EQ(ref.path(), Path("test/path"));
  EXPECT_EQ(ref.write_tree(), &write_tree);
}

TEST(WriteTreeDeathTest, AddOverwrite) {
  WriteTree write_tree;
  Variant snap("test_data");
  write_tree.AddOverwrite(Path("test/path"), snap, 100, kOverwriteVisible);

  UserWriteRecord* record = write_tree.GetWrite(100);
  EXPECT_TRUE(record->is_overwrite);
  EXPECT_TRUE(record->visible);
  EXPECT_EQ(record->path, Path("test/path"));
  EXPECT_EQ(record->overwrite, snap);
}

TEST(WriteTree, AddMerge) {
  WriteTree write_tree;
  CompoundWrite changed_children;
  write_tree.AddMerge(Path("test/path"), changed_children, 100);

  UserWriteRecord* record = write_tree.GetWrite(100);
  EXPECT_FALSE(record->is_overwrite);
  EXPECT_TRUE(record->visible);
  EXPECT_EQ(record->path, Path("test/path"));
}

TEST(WriteTreeDeathTest, AddMerge) {
  WriteTree write_tree;
  CompoundWrite changed_children;
  write_tree.AddMerge(Path("test/path"), changed_children, 100);
  EXPECT_DEATH(write_tree.AddMerge(Path("test/path"), changed_children, 50),
               DEATHTEST_SIGABRT);
}

TEST(WriteTree, GetWrite) {
  WriteTree write_tree;
  Variant snap("test_data");
  write_tree.AddOverwrite(Path("test/path/one"), snap, 100, kOverwriteVisible);
  write_tree.AddOverwrite(Path("test/path/two"), snap, 101, kOverwriteVisible);
  write_tree.AddOverwrite(Path("test/path/three"), snap, 102,
                          kOverwriteVisible);

  EXPECT_EQ(write_tree.GetWrite(99), nullptr);
  EXPECT_NE(write_tree.GetWrite(100), nullptr);
  EXPECT_EQ(write_tree.GetWrite(100)->path, Path("test/path/one"));
  EXPECT_NE(write_tree.GetWrite(101), nullptr);
  EXPECT_EQ(write_tree.GetWrite(101)->path, Path("test/path/two"));
  EXPECT_NE(write_tree.GetWrite(102), nullptr);
  EXPECT_EQ(write_tree.GetWrite(102)->path, Path("test/path/three"));
  EXPECT_EQ(write_tree.GetWrite(103), nullptr);
}

TEST(WriteTree, PurgeAllWrites) {
  WriteTree write_tree;
  Variant snap("test_data");
  write_tree.AddOverwrite(Path("test/path/one"), snap, 100, kOverwriteVisible);
  write_tree.AddOverwrite(Path("test/path/two"), snap, 101, kOverwriteVisible);
  write_tree.AddOverwrite(Path("test/path/three"), snap, 102,
                          kOverwriteVisible);

  std::vector<UserWriteRecord> purged_writes{
      UserWriteRecord(100, Path("test/path/one"), snap, kOverwriteVisible),
      UserWriteRecord(101, Path("test/path/two"), snap, kOverwriteVisible),
      UserWriteRecord(102, Path("test/path/three"), snap, kOverwriteVisible),
  };
  EXPECT_THAT(write_tree.PurgeAllWrites(), Pointwise(Eq(), purged_writes));
}

TEST(WriteTree, RemoveWrite) {
  WriteTree write_tree;
  Variant snap("test_data");
  write_tree.AddOverwrite(Path("test/path/one/visible"), snap, 100,
                          kOverwriteVisible);
  write_tree.AddOverwrite(Path("test/path/two/invisible"), snap, 101,
                          kOverwriteInvisible);
  write_tree.AddOverwrite(Path("test/path/three/visible"), snap, 102,
                          kOverwriteVisible);

  // Removing visible write returns true.
  EXPECT_TRUE(write_tree.RemoveWrite(100));
  // Removing invisible write returns false.
  EXPECT_FALSE(write_tree.RemoveWrite(101));

  EXPECT_EQ(write_tree.GetWrite(100), nullptr);
  EXPECT_EQ(write_tree.GetWrite(101), nullptr);
  EXPECT_NE(write_tree.GetWrite(102), nullptr);
}

TEST(WriteTreeDeathTest, RemoveWrite) {
  WriteTree write_tree;
  Variant snap("test_data");
  write_tree.AddOverwrite(Path("test/path/one/visible"), snap, 100,
                          kOverwriteVisible);
  write_tree.AddOverwrite(Path("test/path/two/invisible"), snap, 101,
                          kOverwriteInvisible);
  write_tree.AddOverwrite(Path("test/path/three/visible"), snap, 102,
                          kOverwriteVisible);

  // Cannot remove a write that never happened.
  EXPECT_DEATH(write_tree.RemoveWrite(200), DEATHTEST_SIGABRT);
}

TEST(WriteTree, GetCompleteWriteData) {
  WriteTree write_tree;
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
      std::make_pair(Path("ccc/fff"), Variant(std::map<Variant, Variant>{
                                          std::make_pair("ggg", 5),
                                          std::make_pair("hhh", 6),
                                      })),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  EXPECT_FALSE(write_tree.GetCompleteWriteData(Path()).has_value());
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/aaa")), 1);
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/bbb")), 2);
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/ccc/ddd")), 3);
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/ccc/eee")), 4);
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/ccc/fff/ggg")), 5);
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/ccc/fff/hhh")), 6);
  EXPECT_EQ(*write_tree.GetCompleteWriteData(Path("test/ccc/fff/iii")),
            Variant::Null());
  EXPECT_FALSE(write_tree.GetCompleteWriteData(Path("test/fff")).has_value());

  EXPECT_FALSE(write_tree.ShadowingWrite(Path()).has_value());
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/aaa")), 1);
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/bbb")), 2);
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/ccc/ddd")), 3);
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/ccc/eee")), 4);
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/ccc/fff/ggg")), 5);
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/ccc/fff/hhh")), 6);
  EXPECT_EQ(*write_tree.ShadowingWrite(Path("test/ccc/fff/iii")),
            Variant::Null());
  EXPECT_FALSE(write_tree.ShadowingWrite(Path("test/fff")).has_value());
}

TEST(WriteTree, CalcCompleteEventCache_NoExcludes_ShadowingWrite) {
  WriteTree write_tree;
  Path tree_path("test/ccc");
  Variant complete_server_cache;
  std::vector<WriteId> no_write_ids_to_exclude;
  const std::map<Path, Variant>& merge_data{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc"),
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  };
  CompoundWrite merge = CompoundWrite::FromPathMerge(merge_data);
  write_tree.AddMerge(Path("test"), merge, 100);

  Optional<Variant> result = write_tree.CalcCompleteEventCache(
      tree_path, &complete_server_cache, no_write_ids_to_exclude);

  Variant expected_result(std::map<Variant, Variant>{
      std::make_pair("ddd", 3),
      std::make_pair("eee", 4),
  });

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcCompleteEventCache_NoExcludes_NoChildMerge) {
  WriteTree write_tree;
  Path tree_path("test/not_present");
  Variant complete_server_cache("server_cache");
  std::vector<WriteId> no_write_ids_to_exclude;
  const std::map<Path, Variant>& merge_data{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite merge = CompoundWrite::FromPathMerge(merge_data);
  write_tree.AddMerge(Path("test"), merge, 100);

  Optional<Variant> result = write_tree.CalcCompleteEventCache(
      tree_path, &complete_server_cache, no_write_ids_to_exclude);

  Variant expected_result("server_cache");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcCompleteEventCache_NoExcludes_NoCompleteSnapshot) {
  WriteTree write_tree;
  Path tree_path("test/not_present");
  std::vector<WriteId> no_write_ids_to_exclude;
  const std::map<Path, Variant>& merge_data{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc"),
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  };
  CompoundWrite merge = CompoundWrite::FromPathMerge(merge_data);
  write_tree.AddMerge(Path("test"), merge, 100);

  Optional<Variant> result = write_tree.CalcCompleteEventCache(
      tree_path, nullptr, no_write_ids_to_exclude);

  EXPECT_FALSE(result.has_value());
}

TEST(WriteTree, CalcCompleteEventCache_NoExcludes_ApplyCache) {
  WriteTree write_tree;
  Path tree_path("test");
  Variant complete_server_cache(std::map<Variant, Variant>{
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", -3),
                         std::make_pair("fff", 5),
                     }),
  });
  std::vector<WriteId> no_write_ids_to_exclude;
  const std::map<Path, Variant>& merge_data{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite merge = CompoundWrite::FromPathMerge(merge_data);
  write_tree.AddMerge(Path("test"), merge, 100);

  Optional<Variant> result = write_tree.CalcCompleteEventCache(
      tree_path, &complete_server_cache, no_write_ids_to_exclude);

  Variant expected_result(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                         std::make_pair("fff", 5),
                     }),
  });

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree,
     CalcCompleteEventCache_HasExcludes_NoHiddenWritesAndEmptyMerge) {
  WriteTree write_tree;
  Path tree_path("test/not_present");
  Variant complete_server_cache("server_cache");
  std::vector<WriteId> write_ids_to_exclude{95};
  const std::map<Path, Variant>& merge_data{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite merge = CompoundWrite::FromPathMerge(merge_data);
  write_tree.AddMerge(Path("test"), merge, 100);

  Optional<Variant> result = write_tree.CalcCompleteEventCache(
      tree_path, &complete_server_cache, write_ids_to_exclude);

  Variant expected_result("server_cache");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcCompleteEventCache_HasExcludes_NoHiddenWritesAndMergeData) {
  WriteTree write_tree;
  Path tree_path("test");
  Variant complete_server_cache(std::map<Variant, Variant>{
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", -3),
                         std::make_pair("fff", 5),
                     }),
  });
  std::vector<WriteId> write_ids_to_exclude{101, 102};
  const std::map<Path, Variant>& merge_100{std::make_pair(Path("aaa"), 1)};
  const std::map<Path, Variant>& merge_101{std::make_pair(Path("bbb"), 2)};
  const std::map<Path, Variant>& merge_102{std::make_pair(Path("ccc/ddd"), 3)};
  const std::map<Path, Variant>& merge_103{std::make_pair(Path("ccc/eee"), 4)};
  CompoundWrite write_100 = CompoundWrite::FromPathMerge(merge_100);
  CompoundWrite write_101 = CompoundWrite::FromPathMerge(merge_101);
  CompoundWrite write_102 = CompoundWrite::FromPathMerge(merge_102);
  CompoundWrite write_103 = CompoundWrite::FromPathMerge(merge_103);
  write_tree.AddMerge(Path("test"), write_100, 100);
  write_tree.AddMerge(Path("test"), write_101, 101);
  write_tree.AddMerge(Path("test"), write_102, 102);
  write_tree.AddMerge(Path("test"), write_103, 103);

  Optional<Variant> result = write_tree.CalcCompleteEventCache(
      tree_path, &complete_server_cache, write_ids_to_exclude);

  Variant expected_result(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", -3),
                         std::make_pair("eee", 4),
                         std::make_pair("fff", 5),
                     }),
  });

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcCompleteEventChildren_WithTopLevelSet) {
  WriteTree write_tree;
  Path tree_path("test/ccc");
  Variant complete_server_children("Irrelevant");
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc"),
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  Variant result =
      write_tree.CalcCompleteEventChildren(tree_path, complete_server_children);
  Variant expected_result(std::map<Variant, Variant>{
      std::make_pair("ddd", 3),
      std::make_pair("eee", 4),
  });
  EXPECT_EQ(result, expected_result);
}

TEST(WriteTree, CalcCompleteEventChildren_WithoutTopLevelSet) {
  WriteTree write_tree;
  Path tree_path("test");
  Variant complete_server_children(std::map<Variant, Variant>{
      std::make_pair("zzz", -1),
      std::make_pair("yyy", -2),
  });
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc"),
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  Variant result =
      write_tree.CalcCompleteEventChildren(tree_path, complete_server_children);
  Variant expected_result(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
      std::make_pair("zzz", -1),
      std::make_pair("yyy", -2),
  });

  EXPECT_EQ(result, expected_result);
}

TEST(WriteTree, CalcEventCacheAfterServerOverwrite_NoWritesAreShadowing) {
  WriteTree write_tree;
  Path tree_path("test/ccc");
  Path child_path("ddd");
  Variant existing_local_snap;
  Variant exsiting_server_snap(std::map<Variant, Variant>{
      std::make_pair("ddd", 3),
      std::make_pair("eee", 4),
  });
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  // Given that the underlying server data has updated, determine what, if
  // anything, needs to be applied to the event cache.  In this case, no writes
  // are shadowing. Events should be raised, the snap to be applied comes from
  // the server data.
  Optional<Variant> result = write_tree.CalcEventCacheAfterServerOverwrite(
      tree_path, child_path, &existing_local_snap, &exsiting_server_snap);
  Variant expected_result = 3;

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcEventCacheAfterServerOverwrite_CompleteShadowing) {
  WriteTree write_tree;
  Path tree_path("test");
  Path child_path("aaa");
  Variant existing_local_snap;
  Variant exsiting_server_snap;
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc"),
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  // Given that the underlying server data has updated, determine what, if
  // anything, needs to be applied to the event cache. The write at "test/aaa"
  // is completely shadowed by what is already in the tree.
  Optional<Variant> result = write_tree.CalcEventCacheAfterServerOverwrite(
      tree_path, child_path, &existing_local_snap, &exsiting_server_snap);

  EXPECT_FALSE(result.has_value());
}

TEST(WriteTree, CalcEventCacheAfterServerOverwrite_PartiallyShadowed) {
  WriteTree write_tree;
  Path tree_path("test");
  Path child_path;
  Variant existing_local_snap;
  Variant exsiting_server_snap(std::map<Variant, Variant>{
      std::make_pair("zzz", 100),
  });
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  // Given that the underlying server data has updated, determine what, if
  // anything, needs to be applied to the event cache. The write at "test" is
  // partially shadowed, so we'll need to merge the server snap with the write
  // to get the updated snapshot.
  Optional<Variant> result = write_tree.CalcEventCacheAfterServerOverwrite(
      tree_path, child_path, &existing_local_snap, &exsiting_server_snap);
  Variant expected_result(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
      std::make_pair("zzz", 100),
  });

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTreeDeathTest, CalcEventCacheAfterServerOverwrite) {
  WriteTree write_tree;
  EXPECT_DEATH(write_tree.CalcEventCacheAfterServerOverwrite(Path(), Path(),
                                                             nullptr, nullptr),
               DEATHTEST_SIGABRT);
}

TEST(WriteTree, CalcCompleteChild_HasShadowingVariant) {
  WriteTree write_tree;
  Path tree_path("test");
  std::string child_key("aaa");
  CacheNode existing_server_cache;
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  Optional<Variant> result =
      write_tree.CalcCompleteChild(tree_path, child_key, existing_server_cache);
  Variant expected_result = 1;

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcCompleteChild_HasCompleteChild) {
  WriteTree write_tree;
  Path tree_path("test");
  std::string child_key("bbb");
  CacheNode existing_server_cache(
      IndexedVariant(std::map<Variant, Variant>{std::make_pair("bbb", 2)}),
      true, false);
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  Optional<Variant> result =
      write_tree.CalcCompleteChild(tree_path, child_key, existing_server_cache);
  Variant expected_result = 2;

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcCompleteChild_NoCompleteChild) {
  WriteTree write_tree;
  Path tree_path("test");
  std::string child_key("ccc");
  CacheNode existing_server_cache(
      IndexedVariant(std::map<Variant, Variant>{std::make_pair("bbb", 2)}),
      true, false);
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
  };
  write_tree.AddMerge(Path("test"), CompoundWrite::FromPathMerge(merge), 100);

  Variant expected_result;
  Optional<Variant> result =
      write_tree.CalcCompleteChild(tree_path, child_key, existing_server_cache);

  EXPECT_EQ(*result, expected_result);
}

TEST(WriteTree, CalcNextVariantAfterPost_WithShadowingVariant) {
  WriteTree write_tree;
  write_tree.AddOverwrite(Path("test"),
                          Variant(std::map<Variant, Variant>{
                              std::make_pair("aaa", 5),
                              std::make_pair("bbb", 4),
                              std::make_pair("ccc", 3),
                              std::make_pair("ddd", 2),
                              std::make_pair("eee", 1),
                          }),
                          101, kOverwriteVisible);

  Path tree_path("test");
  Optional<Variant> complete_server_data;
  IterationDirection direction = kIterateForward;
  QuerySpec query_spec;
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("aaa", 5),
                direction, query_spec.params),
            std::make_pair(Variant("bbb"), Variant(4)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("bbb", 4),
                direction, query_spec.params),
            std::make_pair(Variant("ccc"), Variant(3)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("ccc", 3),
                direction, query_spec.params),
            std::make_pair(Variant("ddd"), Variant(2)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("ddd", 2),
                direction, query_spec.params),
            std::make_pair(Variant("eee"), Variant(1)));
  EXPECT_FALSE(write_tree
                   .CalcNextVariantAfterPost(tree_path, complete_server_data,
                                             std::make_pair("eee", 1),
                                             direction, query_spec.params)
                   .has_value());
}

TEST(WriteTree, CalcNextVariantAfterPost_WithoutShadowingVariant) {
  WriteTree write_tree;
  Path tree_path("test");
  Optional<Variant> complete_server_data(std::map<Variant, Variant>{
      std::make_pair("aaa", 5),
      std::make_pair("bbb", 4),
      std::make_pair("ccc", 3),
      std::make_pair("ddd", 2),
      std::make_pair("eee", 1),
  });
  IterationDirection direction = kIterateForward;
  QuerySpec query_spec;
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("aaa", 5),
                direction, query_spec.params),
            std::make_pair(Variant("bbb"), Variant(4)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("bbb", 4),
                direction, query_spec.params),
            std::make_pair(Variant("ccc"), Variant(3)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("ccc", 3),
                direction, query_spec.params),
            std::make_pair(Variant("ddd"), Variant(2)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("ddd", 2),
                direction, query_spec.params),
            std::make_pair(Variant("eee"), Variant(1)));
  EXPECT_FALSE(write_tree
                   .CalcNextVariantAfterPost(tree_path, complete_server_data,
                                             std::make_pair("eee", 1),
                                             direction, query_spec.params)
                   .has_value());
}

TEST(WriteTree, CalcNextVariantAfterPost_WithoutShadowingVariantOrServerData) {
  WriteTree write_tree;
  Path tree_path("test");
  Optional<Variant> complete_server_data;
  IterationDirection direction = kIterateForward;
  QuerySpec query_spec;
  EXPECT_FALSE(write_tree
                   .CalcNextVariantAfterPost(tree_path, complete_server_data,
                                             std::make_pair("aaa", 5),
                                             direction, query_spec.params)
                   .has_value());
}

TEST(WriteTree, CalcNextVariantAfterPost_Reverse) {
  WriteTree write_tree;
  write_tree.AddOverwrite(Path("test"),
                          Variant(std::map<Variant, Variant>{
                              std::make_pair("aaa", 5),
                              std::make_pair("bbb", 4),
                              std::make_pair("ccc", 3),
                              std::make_pair("ddd", 2),
                              std::make_pair("eee", 1),
                          }),
                          101, kOverwriteVisible);

  Path tree_path("test");
  Optional<Variant> complete_server_data;
  IterationDirection direction = kIterateReverse;
  QuerySpec query_spec;
  EXPECT_FALSE(write_tree
                   .CalcNextVariantAfterPost(tree_path, complete_server_data,
                                             std::make_pair("aaa", 5),
                                             kIterateReverse, query_spec.params)
                   .has_value());
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("bbb", 4),
                direction, query_spec.params),
            std::make_pair(Variant("aaa"), Variant(5)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("ccc", 3),
                direction, query_spec.params),
            std::make_pair(Variant("bbb"), Variant(4)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("ddd", 2),
                direction, query_spec.params),
            std::make_pair(Variant("ccc"), Variant(3)));
  EXPECT_EQ(*write_tree.CalcNextVariantAfterPost(
                tree_path, complete_server_data, std::make_pair("eee", 1),
                direction, query_spec.params),
            std::make_pair(Variant("ddd"), Variant(2)));
}

TEST(WriteTreeRef, Constructor) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);

  EXPECT_EQ(ref.path(), Path("test/path"));
  EXPECT_EQ(ref.write_tree(), &write_tree);
}

TEST(WriteTreeRef, CalcCompleteEventCache1) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);
  Variant complete_server_cache;

  EXPECT_CALL(write_tree, CalcCompleteEventCache(Eq(Path("test/path")),
                                                 Eq(&complete_server_cache)));
  ref.CalcCompleteEventCache(&complete_server_cache);
}

TEST(WriteTreeRef, CalcCompleteEventCache2) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);
  Variant complete_server_cache;
  std::vector<WriteId> write_ids_to_exclude;

  EXPECT_CALL(write_tree, CalcCompleteEventCache(Eq(Path("test/path")),
                                                 Eq(&complete_server_cache),
                                                 Eq(write_ids_to_exclude)));
  ref.CalcCompleteEventCache(&complete_server_cache, write_ids_to_exclude);
}

TEST(WriteTreeRef, CalcCompleteEventCache3) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);
  Variant complete_server_cache;
  std::vector<WriteId> write_ids_to_exclude;
  HiddenWriteInclusion include_hidden_writes = kExcludeHiddenWrites;

  EXPECT_CALL(write_tree, CalcCompleteEventCache(Eq(Path("test/path")),
                                                 Eq(&complete_server_cache),
                                                 Eq(write_ids_to_exclude),
                                                 Eq(include_hidden_writes)));
  ref.CalcCompleteEventCache(&complete_server_cache, write_ids_to_exclude,
                             include_hidden_writes);
}

TEST(WriteTreeRef, CalcEventCacheAfterServerOverwrite) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);
  Path path("another/path");
  Variant existing_local_snap;
  Variant existing_server_snap;

  EXPECT_CALL(write_tree,
              CalcEventCacheAfterServerOverwrite(
                  Eq(Path("test/path")), Eq(Path("another/path")),
                  Eq(&existing_local_snap), Eq(&existing_server_snap)));
  ref.CalcEventCacheAfterServerOverwrite(path, &existing_local_snap,
                                         &existing_server_snap);
}

TEST(WriteTreeRef, ShadowingWrite) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);
  Path path("another/path");

  EXPECT_CALL(write_tree, ShadowingWrite(Eq(Path("test/path/another/path"))));
  ref.ShadowingWrite(path);
}

TEST(WriteTreeRef, CalcCompleteChild) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);
  CacheNode existing_server_cache;

  EXPECT_CALL(write_tree,
              CalcCompleteChild(Eq(Path("test/path")), Eq("child_key"), _));
  ref.CalcCompleteChild("child_key", existing_server_cache);
}

TEST(WriteTreeRef, Child) {
  MockWriteTree write_tree;
  WriteTreeRef ref(Path("test/path"), &write_tree);

  WriteTreeRef child_ref = ref.Child("child_key");

  EXPECT_EQ(child_ref.path(), Path("test/path/child_key"));
  EXPECT_EQ(child_ref.write_tree(), &write_tree);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
