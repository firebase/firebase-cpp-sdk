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

#include "database/src/desktop/core/operation.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(OperationSource, ConstructorSource) {
  OperationSource user_source(OperationSource::kSourceUser);
  EXPECT_EQ(user_source.source, OperationSource::kSourceUser);
  EXPECT_FALSE(user_source.query_params.has_value());
  EXPECT_FALSE(user_source.tagged);

  OperationSource server_source(OperationSource::kSourceServer);
  EXPECT_EQ(server_source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(server_source.query_params.has_value());
  EXPECT_FALSE(server_source.tagged);
}

TEST(OperationSource, ConstructorQueryParams) {
  QueryParams params;
  OperationSource source((Optional<QueryParams>(params)));

  EXPECT_EQ(source.source, OperationSource::kSourceServer);
  EXPECT_EQ(source.query_params.value(), params);
  EXPECT_FALSE(source.tagged);
}

TEST(OperationSource, OperationSourceAllArgConstructor) {
  QueryParams params;
  {
    OperationSource source(OperationSource::kSourceServer,
                           Optional<QueryParams>(params), false);

    EXPECT_EQ(source.source, OperationSource::kSourceServer);
    EXPECT_EQ(source.query_params.value(), params);
    EXPECT_FALSE(source.tagged);
  }
  {
    OperationSource source(OperationSource::kSourceServer,
                           Optional<QueryParams>(params), true);

    EXPECT_EQ(source.source, OperationSource::kSourceServer);
    EXPECT_EQ(source.query_params.value(), params);
    EXPECT_TRUE(source.tagged);
  }
  {
    OperationSource source(OperationSource::kSourceUser,
                           Optional<QueryParams>(params), false);

    EXPECT_EQ(source.source, OperationSource::kSourceUser);
    EXPECT_EQ(source.query_params.value(), params);
    EXPECT_FALSE(source.tagged);
  }
}

TEST(OperationSourceDeathTest, BadConstructorArgs) {
  QueryParams params;
  EXPECT_DEATH(OperationSource(OperationSource::kSourceUser,
                               Optional<QueryParams>(params), true),
               "");
}

TEST(OperationSource, ForServerTaggedQuery) {
  QueryParams params;
  OperationSource expected(OperationSource::kSourceServer,
                           Optional<QueryParams>(params), true);

  OperationSource actual = OperationSource::ForServerTaggedQuery(params);

  EXPECT_EQ(actual.source, expected.source);
  EXPECT_EQ(actual.query_params, expected.query_params);
  EXPECT_EQ(actual.tagged, expected.tagged);
}

TEST(Operation, Overwrite) {
  Operation op = Operation::Overwrite(OperationSource::kServer, Path("A/B/C"),
                                      Variant(100));
  EXPECT_EQ(op.type, Operation::kTypeOverwrite);
  EXPECT_EQ(op.source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(op.source.query_params.has_value());
  EXPECT_EQ(op.path.str(), "A/B/C");
  EXPECT_EQ(op.snapshot, 100);
}

TEST(Operation, Merge) {
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite write = CompoundWrite::FromPathMerge(merge);
  Operation op =
      Operation::Merge(OperationSource::kServer, Path("A/B/C"), write);

  EXPECT_EQ(op.type, Operation::kTypeMerge);
  EXPECT_EQ(op.source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(op.source.query_params.has_value());
  EXPECT_EQ(op.path.str(), "A/B/C");
  EXPECT_FALSE(op.children.IsEmpty());
  EXPECT_FALSE(op.children.write_tree().IsEmpty());
  EXPECT_FALSE(op.children.write_tree().value().has_value());
  EXPECT_EQ(*op.children.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*op.children.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*op.children.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*op.children.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(op.children.write_tree().GetValueAt(Path("fff")), nullptr);
}

TEST(Operation, AckUserWrite) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path("Z/Y/X"), true);
  affected_tree.SetValueAt(Path("Z/Y/X/W"), false);
  affected_tree.SetValueAt(Path("Z/Y/X/V"), true);
  affected_tree.SetValueAt(Path("Z/Y/U"), false);
  Operation op =
      Operation::AckUserWrite(Path("A/B/C"), affected_tree, kAckRevert);

  EXPECT_EQ(op.type, Operation::kTypeAckUserWrite);
  EXPECT_EQ(op.source.source, OperationSource::kSourceUser);
  EXPECT_FALSE(op.source.query_params.has_value());
  EXPECT_EQ(op.path.str(), "A/B/C");
  EXPECT_TRUE(*op.affected_tree.GetValueAt(Path("Z/Y/X")));
  EXPECT_FALSE(*op.affected_tree.GetValueAt(Path("Z/Y/X/W")));
  EXPECT_TRUE(*op.affected_tree.GetValueAt(Path("Z/Y/X/V")));
  EXPECT_FALSE(*op.affected_tree.GetValueAt(Path("Z/Y/U")));
  EXPECT_TRUE(op.revert);
}

TEST(Operation, ListenComplete) {
  Operation op =
      Operation::ListenComplete(OperationSource::kServer, Path("A/B/C"));
  EXPECT_EQ(op.type, Operation::kTypeListenComplete);
  EXPECT_EQ(op.source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(op.source.query_params.has_value());
  EXPECT_EQ(op.path.str(), "A/B/C");
}

TEST(OperationDeathTest, ListenCompleteWithWrongSource) {
  // ListenCompletes must come from the server, not the user.
  EXPECT_DEATH(Operation::ListenComplete(OperationSource::kUser, Path("A/B/C")),
               DEATHTEST_SIGABRT);
}

TEST(Operation, OperationForChildOverwriteEmptyPath) {
  std::map<Variant, Variant> variant_data{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  Operation op = Operation::Overwrite(OperationSource::kServer, Path(),
                                      Variant(variant_data));
  Optional<Operation> result = OperationForChild(op, "aaa");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeOverwrite);
  EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "");
  EXPECT_EQ(result->snapshot, Variant(100));
}

TEST(Operation, OperationForChildOverwriteNonEmptyPath) {
  std::map<Variant, Variant> variant_data{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  Operation op = Operation::Overwrite(OperationSource::kServer, Path("A/B/C"),
                                      Variant(variant_data));
  Optional<Operation> result = OperationForChild(op, "A");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeOverwrite);
  EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "B/C");
  EXPECT_EQ(result->snapshot, variant_data);
}

TEST(Operation, OperationForChildMergeEmptyPath) {
  {
    std::map<Path, Variant> merge_data{
        std::make_pair(Path("aaa"), 100),
        std::make_pair(Path("bbb"), 200),
        std::make_pair(Path("ccc/ddd"), 300),
    };
    CompoundWrite write = CompoundWrite::FromPathMerge(merge_data);
    Operation op = Operation::Merge(OperationSource::kServer, Path(), write);

    Optional<Operation> result = OperationForChild(op, "zzz");

    EXPECT_FALSE(result.has_value());
  }
  {
    std::map<Path, Variant> merge_data{
        std::make_pair(Path("aaa"), 100),
        std::make_pair(Path("bbb"), 200),
        std::make_pair(Path("ccc/ddd"), 300),
    };
    CompoundWrite write = CompoundWrite::FromPathMerge(merge_data);
    Operation op = Operation::Merge(OperationSource::kServer, Path(), write);

    Optional<Operation> result = OperationForChild(op, "aaa");

    EXPECT_TRUE(result.has_value());
    // In this case we expect to generate an Overwrite operation.
    EXPECT_EQ(result->type, Operation::kTypeOverwrite);
    EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
    EXPECT_FALSE(result->source.query_params.has_value());
    EXPECT_EQ(result->path.str(), "");
  }
  {
    std::map<Path, Variant> merge_data{
        std::make_pair(Path("aaa"), 100),
        std::make_pair(Path("bbb"), 200),
        std::make_pair(Path("ccc/ddd"), 300),
    };
    CompoundWrite write = CompoundWrite::FromPathMerge(merge_data);
    Operation op = Operation::Merge(OperationSource::kServer, Path(), write);

    Optional<Operation> result = OperationForChild(op, "ccc");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->type, Operation::kTypeMerge);
    EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
    EXPECT_FALSE(result->source.query_params.has_value());
    EXPECT_EQ(result->path.str(), "");
  }
}

TEST(Operation, OperationForChildMergeNonEmptyPath) {
  {
    std::map<Path, Variant> merge_data{
        std::make_pair(Path("aaa"), 100),
        std::make_pair(Path("bbb"), 200),
        std::make_pair(Path("ccc/ddd"), 300),
    };
    CompoundWrite write = CompoundWrite::FromPathMerge(merge_data);
    Operation op =
        Operation::Merge(OperationSource::kServer, Path("A/B/C"), write);

    Optional<Operation> result = OperationForChild(op, "A");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->type, Operation::kTypeMerge);
    EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
    EXPECT_FALSE(result->source.query_params.has_value());
    EXPECT_EQ(result->path.str(), "B/C");
    const Tree<Variant>& write_tree = result->children.write_tree();
    EXPECT_EQ(*write_tree.GetValueAt(Path("aaa")), 100);
    EXPECT_EQ(*write_tree.GetValueAt(Path("bbb")), 200);
    EXPECT_EQ(*write_tree.GetValueAt(Path("ccc/ddd")), 300);
  }
  {
    std::map<Path, Variant> merge_data{
        std::make_pair(Path("aaa"), 100),
        std::make_pair(Path("bbb"), 200),
        std::make_pair(Path("ccc/ddd"), 300),
    };
    CompoundWrite write = CompoundWrite::FromPathMerge(merge_data);
    Operation op =
        Operation::Merge(OperationSource::kServer, Path("A/B/C"), write);

    Optional<Operation> result = OperationForChild(op, "Z");

    EXPECT_FALSE(result.has_value());
  }
}

TEST(Operation, OperationForChildAckUserWriteNonEmptyPath) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path("aaa"), true);
  affected_tree.SetValueAt(Path("bbb"), false);
  affected_tree.SetValueAt(Path("ccc/ddd"), true);
  affected_tree.SetValueAt(Path("ccc/eee"), false);
  Operation op =
      Operation::AckUserWrite(Path("A/B/C"), affected_tree, kAckRevert);

  Optional<Operation> result = OperationForChild(op, "A");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeAckUserWrite);
  EXPECT_EQ(result->source.source, OperationSource::kSourceUser);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "B/C");
  EXPECT_TRUE(*result->affected_tree.GetValueAt(Path("aaa")));
  EXPECT_FALSE(*result->affected_tree.GetValueAt(Path("bbb")));
  EXPECT_TRUE(*result->affected_tree.GetValueAt(Path("ccc/ddd")));
  EXPECT_FALSE(*result->affected_tree.GetValueAt(Path("ccc/eee")));
  EXPECT_TRUE(result->revert);
}

TEST(OperationDeathTest,
     OperationForChildAckUserWriteNonEmptyPathWithUnrelatedChild) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path("aaa"), true);
  affected_tree.SetValueAt(Path("bbb"), false);
  affected_tree.SetValueAt(Path("ccc/ddd"), true);
  affected_tree.SetValueAt(Path("ccc/eee"), false);
  Operation op =
      Operation::AckUserWrite(Path("A/B/C"), affected_tree, kAckRevert);

  // Cannot ack an unrelated path.
  EXPECT_DEATH(OperationForChild(op, "Z"), DEATHTEST_SIGABRT);
}

TEST(Operation, OperationForChildAckUserWriteEmptyPathHasValue) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path(), true);
  Operation op = Operation::AckUserWrite(Path(), affected_tree, kAckRevert);

  Optional<Operation> result = OperationForChild(op, "aaa");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeAckUserWrite);
  EXPECT_EQ(result->source.source, OperationSource::kSourceUser);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "");
  EXPECT_TRUE(result->affected_tree.value().value());
  EXPECT_TRUE(result->revert);
}

TEST(OperationDeathTest,
     OperationForChildAckUserWriteEmptyPathOverlappingChildren) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path(), false);
  affected_tree.SetValueAt(Path("aaa"), true);
  affected_tree.SetValueAt(Path("bbb"), false);
  affected_tree.SetValueAt(Path("ccc/ddd"), true);
  affected_tree.SetValueAt(Path("ccc/eee"), false);
  Operation op = Operation::AckUserWrite(Path(), affected_tree, kAckRevert);

  // The affected tree has a value at the root which overlaps the affected path.
  EXPECT_DEATH(OperationForChild(op, "ccc"), DEATHTEST_SIGABRT);
}

TEST(Operation, OperationForChildAckUserWriteEmptyPathDoesNotHasValue) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path("aaa"), true);
  affected_tree.SetValueAt(Path("bbb"), false);
  affected_tree.SetValueAt(Path("ccc/ddd"), true);
  affected_tree.SetValueAt(Path("ccc/eee"), false);
  Operation op = Operation::AckUserWrite(Path(), affected_tree, kAckRevert);

  Optional<Operation> result = OperationForChild(op, "ccc");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeAckUserWrite);
  EXPECT_EQ(result->source.source, OperationSource::kSourceUser);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "");
  EXPECT_TRUE(*result->affected_tree.GetValueAt(Path("ddd")));
  EXPECT_FALSE(*result->affected_tree.GetValueAt(Path("eee")));
  EXPECT_TRUE(result->revert);
}

TEST(Operation,
     OperationForChildAckUserWriteEmptyPathDoesNotHasValueAndNoAffectedChild) {
  Tree<bool> affected_tree;
  affected_tree.SetValueAt(Path("aaa"), true);
  affected_tree.SetValueAt(Path("bbb"), false);
  affected_tree.SetValueAt(Path("ccc/ddd"), true);
  affected_tree.SetValueAt(Path("ccc/eee"), false);
  Operation op = Operation::AckUserWrite(Path(), affected_tree, kAckRevert);

  Optional<Operation> result = OperationForChild(op, "zzz");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeAckUserWrite);
  EXPECT_EQ(result->source.source, OperationSource::kSourceUser);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "");
  EXPECT_TRUE(result->affected_tree.children().empty());
  EXPECT_FALSE(result->affected_tree.value().has_value());
  EXPECT_TRUE(result->revert);
}

TEST(Operation, OperationForChildListenCompleteEmptyPath) {
  Operation op = Operation::ListenComplete(OperationSource::kServer, Path());

  Optional<Operation> result = OperationForChild(op, "Z");

  // Should be identical to op.
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeListenComplete);
  EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "");
}

TEST(Operation, OperationForChildListenCompleteNonEmptyPath) {
  Operation op =
      Operation::ListenComplete(OperationSource::kServer, Path("A/B/C"));

  Optional<Operation> result = OperationForChild(op, "A");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->type, Operation::kTypeListenComplete);
  EXPECT_EQ(result->source.source, OperationSource::kSourceServer);
  EXPECT_FALSE(result->source.query_params.has_value());
  EXPECT_EQ(result->path.str(), "B/C");
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
