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

#include "database/src/desktop/core/sparse_snapshot_tree.h"

#include "app/src/variant_util.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::StrictMock;

namespace firebase {
namespace database {
namespace internal {
namespace {

class Visitor {
 public:
  virtual ~Visitor() {}
  virtual void Visit(const Path& path, const Variant& variant) = 0;
};

class MockVisitor : public Visitor {
 public:
  ~MockVisitor() override {}
  MOCK_METHOD(void, Visit, (const Path& path, const Variant& variant),
              (override));
};

TEST(SparseSnapshotTreeTest, RememberSimple) {
  SparseSnapshotTree tree;
  tree.Remember(Path(), 100);
  MockVisitor visitor;

  EXPECT_CALL(visitor, Visit(Path(), Variant(100)));

  tree.ForEachTree(Path(),
                   [&visitor](const Path& path, const Variant& variant) {
                     visitor.Visit(path, variant);
                   });
}

TEST(SparseSnapshotTreeTest, RememberTree) {
  SparseSnapshotTree tree;
  tree.Remember(Path(), std::map<Variant, Variant>{std::make_pair("aaa", 100)});
  tree.Remember(Path("bbb"), 200);
  tree.Remember(Path("bbb/ccc"), 300);
  tree.Remember(Path("eee"), 400);
  MockVisitor visitor;

  EXPECT_CALL(visitor,
              Visit(Path(), Variant(std::map<Variant, Variant>{
                                std::make_pair("aaa", 100),
                                std::make_pair("bbb",
                                               std::map<Variant, Variant>{
                                                   std::make_pair("ccc", 300),
                                               }),
                                std::make_pair("eee", 400),
                            })));

  tree.ForEachTree(Path(),
                   [&visitor](const Path& path, const Variant& variant) {
                     visitor.Visit(path, variant);
                   });
}

TEST(SparseSnapshotTreeTest, Forget) {
  SparseSnapshotTree tree;
  tree.Remember(Path(), std::map<Variant, Variant>{std::make_pair("aaa", 100)});
  tree.Remember(Path("bbb"), 200);
  tree.Remember(Path("bbb/ccc"), 300);
  tree.Remember(Path("eee"), 400);
  tree.Forget(Path("aaa"));
  tree.Forget(Path("bbb"));
  MockVisitor visitor;

  EXPECT_CALL(visitor, Visit(Path("eee"), Variant(400)));

  tree.ForEachTree(Path(),
                   [&visitor](const Path& path, const Variant& variant) {
                     visitor.Visit(path, variant);
                   });
}

TEST(SparseSnapshotTreeTest, Clear) {
  SparseSnapshotTree tree;
  tree.Remember(Path(), std::map<Variant, Variant>{std::make_pair("aaa", 100)});
  tree.Remember(Path("bbb"), 200);
  tree.Remember(Path("bbb/ccc"), 300);
  tree.Clear();

  // Expect no calls to this visitor.
  StrictMock<MockVisitor> visitor;

  tree.ForEachTree(Path(),
                   [&visitor](const Path& path, const Variant& variant) {
                     visitor.Visit(path, variant);
                   });
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
