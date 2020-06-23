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

#include "database/src/desktop/core/compound_write.h"

#include <map>
#include <utility>

#include "app/src/variant_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::Eq;
using testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(CompoundWrite, CompoundWrite) {
  {
    CompoundWrite write;
    EXPECT_TRUE(write.IsEmpty());
    EXPECT_TRUE(write.write_tree().IsEmpty());
    EXPECT_FALSE(write.GetRootWrite().has_value());
  }
  {
    CompoundWrite write = CompoundWrite::EmptyWrite();
    EXPECT_TRUE(write.IsEmpty());
    EXPECT_TRUE(write.write_tree().IsEmpty());
    EXPECT_FALSE(write.GetRootWrite().has_value());
  }
}

TEST(CompoundWrite, FromChildMerge) {
  {
    const std::map<std::string, Variant>& merge{
        std::make_pair("", 0),
    };
    CompoundWrite write = CompoundWrite::FromChildMerge(merge);
    EXPECT_FALSE(write.IsEmpty());
    EXPECT_FALSE(write.write_tree().IsEmpty());
    EXPECT_EQ(*write.GetRootWrite(), 0);
  }
  {
    const std::map<std::string, Variant>& merge{
        std::make_pair("aaa", 1),
        std::make_pair("bbb", 2),
        std::make_pair("ccc/ddd", 3),
        std::make_pair("ccc/eee", 4),
    };
    CompoundWrite write = CompoundWrite::FromChildMerge(merge);
    EXPECT_FALSE(write.IsEmpty());
    EXPECT_FALSE(write.write_tree().IsEmpty());
    EXPECT_FALSE(write.write_tree().value().has_value());
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("aaa")), 1);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("bbb")), 2);
    EXPECT_EQ(write.write_tree().GetValueAt(Path("ccc")), nullptr);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("ccc/eee")), 4);
    EXPECT_EQ(write.write_tree().GetValueAt(Path("zzz")), nullptr);
  }
}

TEST(CompoundWrite, FromVariantMerge) {
  {
    Variant merge(std::map<Variant, Variant>{
        std::make_pair("", 0),
    });
    CompoundWrite write = CompoundWrite::FromVariantMerge(merge);
    EXPECT_FALSE(write.IsEmpty());
    EXPECT_FALSE(write.write_tree().IsEmpty());
    EXPECT_EQ(*write.GetRootWrite(), 0);
  }
  {
    Variant merge(std::map<Variant, Variant>{
        std::make_pair("aaa", 1),
        std::make_pair("bbb", 2),
        std::make_pair("ccc/ddd", 3),
        std::make_pair("ccc/eee", 4),
    });
    CompoundWrite write = CompoundWrite::FromVariantMerge(merge);
    EXPECT_FALSE(write.IsEmpty());
    EXPECT_FALSE(write.write_tree().IsEmpty());
    EXPECT_FALSE(write.write_tree().value().has_value());
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("aaa")), 1);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("bbb")), 2);
    EXPECT_EQ(write.write_tree().GetValueAt(Path("ccc")), nullptr);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("ccc/eee")), 4);
    EXPECT_EQ(write.write_tree().GetValueAt(Path("zzz")), nullptr);
  }
}

TEST(CompoundWrite, FromPathMerge) {
  {
    const std::map<Path, Variant>& merge{
        std::make_pair(Path(""), 0),
    };

    CompoundWrite write = CompoundWrite::FromPathMerge(merge);

    EXPECT_FALSE(write.IsEmpty());
    EXPECT_FALSE(write.write_tree().IsEmpty());
    EXPECT_EQ(*write.GetRootWrite(), 0);
  }
  {
    const std::map<Path, Variant>& merge{
        std::make_pair(Path("aaa"), 1),
        std::make_pair(Path("bbb"), 2),
        std::make_pair(Path("ccc/ddd"), 3),
        std::make_pair(Path("ccc/eee"), 4),
    };

    CompoundWrite write = CompoundWrite::FromPathMerge(merge);

    EXPECT_FALSE(write.IsEmpty());
    EXPECT_FALSE(write.write_tree().IsEmpty());
    EXPECT_FALSE(write.write_tree().value().has_value());
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("aaa")), 1);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("bbb")), 2);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
    EXPECT_EQ(*write.write_tree().GetValueAt(Path("ccc/eee")), 4);
    EXPECT_EQ(write.write_tree().GetValueAt(Path("zzz")), nullptr);
  }
}

// This just replicates the set up work done in the FromPathMerge test.
class CompoundWriteTest : public ::testing::Test {
  void SetUp() override {
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

    write_ = CompoundWrite::FromPathMerge(merge);
  }

  void TearDown() override {}

 protected:
  CompoundWrite write_;
};

TEST_F(CompoundWriteTest, EmptyWrite) {
  CompoundWrite empty = CompoundWrite::EmptyWrite();
  EXPECT_TRUE(empty.IsEmpty());
}

TEST_F(CompoundWriteTest, AddWriteEmptyPath) {
  CompoundWrite new_write = write_.AddWrite(Path(), Optional<Variant>(100));

  // New write should just be the root value.
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("bbb")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc/ddd")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc/eee")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc/fff")), nullptr);
  EXPECT_TRUE(new_write.write_tree().value().has_value());
  EXPECT_EQ(new_write.write_tree().value().value(), 100);
}

TEST_F(CompoundWriteTest, AddWriteInlineEmptyPath) {
  write_.AddWriteInline(Path(), Optional<Variant>(100));
  CompoundWrite& new_write = write_;

  // New write should just be the root value.
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("bbb")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc/ddd")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc/eee")), nullptr);
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("ccc/fff")), nullptr);
  EXPECT_TRUE(new_write.write_tree().value().has_value());
  EXPECT_EQ(new_write.write_tree().value().value(), 100);
}

TEST_F(CompoundWriteTest, AddWritePriorityWrite) {
  {
    CompoundWrite new_write =
        write_.AddWrite(Path("ccc/.priority"), Optional<Variant>(100));

    // Everything should be the same, but with an additional .priority field.
    EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("aaa")), 1);
    EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("bbb")), 2);
    EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
    EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
    EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/fff")),
              Variant(std::map<Variant, Variant>{
                  std::make_pair("ggg", 5),
                  std::make_pair("hhh", 6),
              }));
    EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/.priority")), 100);
  }

  {
    CompoundWrite new_write =
        write_.AddWrite(Path("aaa/bad_path/.priority"), Optional<Variant>(100));

    // New write should be identical to the old write.
    EXPECT_EQ(new_write, write_);
  }
}

TEST_F(CompoundWriteTest, AddWriteThatDoesNotOverwrite) {
  CompoundWrite new_write =
      write_.AddWrite(Path("iii/jjj"), Optional<Variant>(100));

  // New write should have the new value alongside old values.
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/fff")),
            Variant(std::map<Variant, Variant>{
                std::make_pair("ggg", 5),
                std::make_pair("hhh", 6),
            }));
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("iii/jjj")), 100);
}

TEST_F(CompoundWriteTest, AddWriteThatShadowsExistingData) {
  CompoundWrite new_write =
      write_.AddWrite(Path("ccc/fff/ggg"), Optional<Variant>(100));

  // Values being shadowed are still part of the CompoundWrite.
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/fff")),
            Variant(std::map<Variant, Variant>{
                std::make_pair("ggg", 100),
                std::make_pair("hhh", 6),
            }));
}

TEST_F(CompoundWriteTest, AddWrites) {
  const std::map<Path, Variant>& second_merge{
      std::make_pair(Path("zzz"), -1),
      std::make_pair(Path("yyy"), -2),
      std::make_pair(Path("xxx/www"), -3),
      std::make_pair(Path("xxx/vvv"), -4),
  };
  CompoundWrite second_write = CompoundWrite::FromPathMerge(second_merge);

  const std::map<Path, Variant>& third_merge{
      std::make_pair(Path("apple"), 1111),
      std::make_pair(Path("banana"), 2222),
      std::make_pair(Path("carrot/date"), 3333),
      std::make_pair(Path("carrot/eggplant"), 4444),
  };
  CompoundWrite third_write = CompoundWrite::FromPathMerge(third_merge);

  CompoundWrite updated_write = write_.AddWrites(Path(), second_write);

  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("zzz")), -1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("yyy")), -2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/www")), -3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/vvv")), -4);

  updated_write = updated_write.AddWrites(Path("ccc"), third_write);

  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/apple")), 1111);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/banana")), 2222);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/carrot/date")),
            3333);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/carrot/eggplant")),
            4444);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("zzz")), -1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("yyy")), -2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/www")), -3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/vvv")), -4);
}

TEST_F(CompoundWriteTest, AddWritesInline) {
  const std::map<Path, Variant>& second_merge{
      std::make_pair(Path("zzz"), -1),
      std::make_pair(Path("yyy"), -2),
      std::make_pair(Path("xxx/www"), -3),
      std::make_pair(Path("xxx/vvv"), -4),
  };
  CompoundWrite second_write = CompoundWrite::FromPathMerge(second_merge);

  const std::map<Path, Variant>& third_merge{
      std::make_pair(Path("apple"), 1111),
      std::make_pair(Path("banana"), 2222),
      std::make_pair(Path("carrot/date"), 3333),
      std::make_pair(Path("carrot/eggplant"), 4444),
  };
  CompoundWrite third_write = CompoundWrite::FromPathMerge(third_merge);

  write_.AddWritesInline(Path(), second_write);
  CompoundWrite& updated_write = write_;

  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("zzz")), -1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("yyy")), -2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/www")), -3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/vvv")), -4);

  updated_write.AddWritesInline(Path("ccc"), third_write);

  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/apple")), 1111);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/banana")), 2222);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/carrot/date")),
            3333);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("ccc/carrot/eggplant")),
            4444);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("zzz")), -1);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("yyy")), -2);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/www")), -3);
  EXPECT_EQ(*updated_write.write_tree().GetValueAt(Path("xxx/vvv")), -4);
}

TEST_F(CompoundWriteTest, RemoveWrite) {
  CompoundWrite new_write = write_.RemoveWrite(Path("aaa"));

  // New write should be missing aaa
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
}

TEST_F(CompoundWriteTest, RemoveWriteInline) {
  write_.RemoveWriteInline(Path("aaa"));
  CompoundWrite& new_write = write_;

  // New write should be missing aaa
  EXPECT_EQ(new_write.write_tree().GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*new_write.write_tree().GetValueAt(Path("ccc/eee")), 4);
}

TEST_F(CompoundWriteTest, HasCompleteWrite) {
  EXPECT_TRUE(write_.HasCompleteWrite(Path("aaa")));
  EXPECT_TRUE(write_.HasCompleteWrite(Path("bbb")));
  EXPECT_FALSE(write_.HasCompleteWrite(Path("ccc")));
  EXPECT_TRUE(write_.HasCompleteWrite(Path("ccc/ddd")));
  EXPECT_TRUE(write_.HasCompleteWrite(Path("ccc/eee")));
  EXPECT_FALSE(write_.HasCompleteWrite(Path("zzz")));
}

TEST_F(CompoundWriteTest, GetRootWriteEmpty) {
  Optional<Variant> root = write_.GetRootWrite();
  EXPECT_FALSE(root.has_value());
}

TEST(CompoundWrite, GetRootWritePopulated) {
  const std::map<Path, Variant>& merge{
      std::make_pair(Path(""), "One billion"),
  };
  CompoundWrite write = CompoundWrite::FromPathMerge(merge);

  Optional<Variant> root = write.GetRootWrite();
  EXPECT_TRUE(root.has_value());
  EXPECT_EQ(root.value(), "One billion");
}

TEST_F(CompoundWriteTest, GetCompleteVariant) {
  EXPECT_FALSE(write_.GetCompleteVariant(Path()).has_value());
  EXPECT_EQ(write_.GetCompleteVariant(Path("aaa")).value(), 1);
  EXPECT_EQ(write_.GetCompleteVariant(Path("bbb")).value(), 2);
  EXPECT_TRUE(write_.GetCompleteVariant(Path("ccc/ddd")).has_value());
  EXPECT_EQ(write_.GetCompleteVariant(Path("ccc/ddd")).value(), 3);
  EXPECT_TRUE(write_.GetCompleteVariant(Path("ccc/eee")).has_value());
  EXPECT_EQ(write_.GetCompleteVariant(Path("ccc/eee")).value(), 4);
  EXPECT_TRUE(write_.GetCompleteVariant(Path("ccc/fff/ggg")).has_value());
  EXPECT_EQ(write_.GetCompleteVariant(Path("ccc/fff/ggg")).value(), 5);
  EXPECT_TRUE(write_.GetCompleteVariant(Path("ccc/fff/ggg")).has_value());
  EXPECT_EQ(write_.GetCompleteVariant(Path("ccc/fff/hhh")).value(), 6);
  EXPECT_TRUE(write_.GetCompleteVariant(Path("ccc/fff/iii")).has_value());
  EXPECT_EQ(write_.GetCompleteVariant(Path("ccc/fff/iii")).value(),
            Variant::Null());
  EXPECT_FALSE(write_.GetCompleteVariant(Path("zzz")).has_value());
}

TEST_F(CompoundWriteTest, GetCompleteChildren) {
  std::vector<std::pair<Variant, Variant>> children =
      write_.GetCompleteChildren();

  std::vector<std::pair<Variant, Variant>> expected_children = {
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
  };

  EXPECT_THAT(children, Pointwise(Eq(), expected_children));
}

TEST_F(CompoundWriteTest, ChildCompoundWriteEmptyPath) {
  CompoundWrite child = write_.ChildCompoundWrite(Path());

  // Should be exactly the same as write_.
  EXPECT_FALSE(child.IsEmpty());
  EXPECT_FALSE(child.write_tree().IsEmpty());
  EXPECT_FALSE(child.write_tree().value().has_value());
  EXPECT_EQ(*child.write_tree().GetValueAt(Path("aaa")), 1);
  EXPECT_EQ(*child.write_tree().GetValueAt(Path("bbb")), 2);
  EXPECT_EQ(child.write_tree().GetValueAt(Path("ccc")), nullptr);
  EXPECT_EQ(*child.write_tree().GetValueAt(Path("ccc/ddd")), 3);
  EXPECT_EQ(*child.write_tree().GetValueAt(Path("ccc/eee")), 4);
  EXPECT_EQ(child.write_tree().GetValueAt(Path("zzz")), nullptr);
}

TEST(CompoundWrite, ChildCompoundWriteShadowingWrite) {
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),     std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc"), -9999), std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite write = CompoundWrite::FromPathMerge(merge);
  CompoundWrite child = write.ChildCompoundWrite(Path("ccc"));
  EXPECT_EQ(child.GetRootWrite().value(), -9999);
}

TEST_F(CompoundWriteTest, ChildCompoundWriteNonShadowingWrite) {
  CompoundWrite child = write_.ChildCompoundWrite(Path("ccc"));

  EXPECT_FALSE(child.IsEmpty());
  EXPECT_FALSE(child.write_tree().IsEmpty());
  EXPECT_FALSE(child.write_tree().value().has_value());
  EXPECT_EQ(child.write_tree().GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(child.write_tree().GetValueAt(Path("bbb")), nullptr);
  EXPECT_EQ(*child.write_tree().GetValueAt(Path("ddd")), 3);
  EXPECT_EQ(*child.write_tree().GetValueAt(Path("eee")), 4);
  EXPECT_EQ(child.write_tree().GetValueAt(Path("zzz")), nullptr);
}

TEST_F(CompoundWriteTest, ChildCompoundWrites) {
  std::map<std::string, CompoundWrite> writes = write_.ChildCompoundWrites();

  CompoundWrite& aaa = writes["aaa"];
  CompoundWrite& bbb = writes["bbb"];
  CompoundWrite& ccc = writes["ccc"];

  EXPECT_EQ(writes.size(), 3);
  EXPECT_EQ(aaa.write_tree().value().value(), 1);
  EXPECT_EQ(bbb.write_tree().value().value(), 2);
  EXPECT_EQ(*ccc.write_tree().GetValueAt(Path("ddd")), 3);
  EXPECT_EQ(*ccc.write_tree().GetValueAt(Path("eee")), 4);
}

TEST_F(CompoundWriteTest, IsEmpty) {
  CompoundWrite compound_write;
  EXPECT_TRUE(compound_write.IsEmpty());

  CompoundWrite empty = CompoundWrite::EmptyWrite();
  EXPECT_TRUE(empty.IsEmpty());

  CompoundWrite add_write = compound_write.AddWrite(Path(), 100);
  EXPECT_TRUE(compound_write.IsEmpty());
  EXPECT_FALSE(add_write.IsEmpty());
}

TEST_F(CompoundWriteTest, Apply) {
  Variant expected_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                         std::make_pair("fff",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ggg", 5),
                                            std::make_pair("hhh", 6),
                                        }),
                     }),
      std::make_pair("zzz", 100),
  });
  Variant variant_to_apply(std::map<Variant, Variant>{
      std::make_pair("zzz", 100),
  });

  EXPECT_EQ(write_.Apply(variant_to_apply), expected_variant);
}

TEST_F(CompoundWriteTest, Equality) {
  const std::map<Path, Variant>& same_merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
      std::make_pair(Path("ccc/fff"), Variant(std::map<Variant, Variant>{
                                          std::make_pair("ggg", 5),
                                          std::make_pair("hhh", 6),
                                      })),
  };
  CompoundWrite same_write = CompoundWrite::FromPathMerge(same_merge);
  const std::map<Path, Variant>& different_merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
      std::make_pair(Path("ccc/fff"), Variant(std::map<Variant, Variant>{
                                          std::make_pair("ggg", 5),
                                          std::make_pair("hhh", 100),
                                      })),
  };
  CompoundWrite different_write = CompoundWrite::FromPathMerge(different_merge);

  EXPECT_EQ(write_, same_write);
  EXPECT_NE(write_, different_write);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
