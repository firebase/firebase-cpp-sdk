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

#include "database/src/desktop/view/indexed_filter.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/indexed_variant.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;
using ::testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(IndexedFilter, UpdateChild_SameValue) {
  QueryParams params;
  IndexedFilter filter(params);

  Variant old_child(std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair("bbb",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ccc", 100),
                                        }),
                     }),
  });

  IndexedVariant indexed_variant((Variant(old_child)));
  std::string key("aaa");
  Variant new_child(std::map<Variant, Variant>{
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 100),
                     }),
  });
  Path affected_path("bbb/ccc");
  CompleteChildSource* source = nullptr;
  ChildChangeAccumulator change_accumulator;

  IndexedVariant expected_result(old_child);
  EXPECT_EQ(filter.UpdateChild(indexed_variant, key, new_child, affected_path,
                               source, &change_accumulator),
            expected_result);
  // Expect no changes
  EXPECT_EQ(change_accumulator, ChildChangeAccumulator());
}

TEST(IndexedFilter, UpdateChild_ChangedValue) {
  QueryParams params;
  IndexedFilter filter(params);

  Variant old_child(std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair("bbb",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ccc", 100),
                                        }),
                     }),
  });

  IndexedVariant indexed_variant((Variant(old_child)));
  std::string key("aaa");
  Variant new_child(std::map<Variant, Variant>{
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                     }),
  });
  Path affected_path("bbb/ccc");
  CompleteChildSource* source = nullptr;
  ChildChangeAccumulator change_accumulator;

  IndexedVariant expected_result(std::map<Variant, Variant>{
      std::make_pair("aaa", new_child),
  });
  ChildChangeAccumulator expected_changes{
      std::make_pair(
          "aaa",
          ChildChangedChange("aaa", new_child,
                             Variant(std::map<Variant, Variant>{
                                 std::make_pair("bbb",
                                                std::map<Variant, Variant>{
                                                    std::make_pair("ccc", 100),
                                                }),
                             }))),
  };

  EXPECT_EQ(filter.UpdateChild(indexed_variant, key, new_child, affected_path,
                               source, &change_accumulator),
            expected_result);
  EXPECT_EQ(change_accumulator, expected_changes);
}

TEST(IndexedFilter, UpdateChild_AddedValue) {
  QueryParams params;
  IndexedFilter filter(params);

  Variant old_child(std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair("bbb",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ccc", 100),
                                        }),
                     }),
  });

  IndexedVariant indexed_variant((Variant(old_child)));
  std::string key("ddd");
  Variant new_child(std::map<Variant, Variant>{
      std::make_pair("eee", 200),
  });
  Path affected_path;
  CompleteChildSource* source = nullptr;
  ChildChangeAccumulator change_accumulator;

  IndexedVariant expected_result(std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair("bbb",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ccc", 100),
                                        }),
                     }),
      std::make_pair("ddd",
                     std::map<Variant, Variant>{
                         std::make_pair("eee", 200),
                     }),
  });
  ChildChangeAccumulator expected_changes{
      std::make_pair("ddd", ChildAddedChange("ddd", new_child)),
  };
  EXPECT_EQ(filter.UpdateChild(indexed_variant, key, new_child, affected_path,
                               source, &change_accumulator),
            expected_result);
  EXPECT_THAT(change_accumulator, Pointwise(Eq(), expected_changes));
}

TEST(IndexedFilter, UpdateChild_RemovedValue) {
  QueryParams params;
  IndexedFilter filter(params);

  Variant old_child(std::map<Variant, Variant>{
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair("bbb",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ccc", 100),
                                        }),
                     }),
  });

  IndexedVariant indexed_variant((Variant(old_child)));
  std::string key("aaa");
  Variant new_child = Variant::Null();
  Path affected_path;
  CompleteChildSource* source = nullptr;
  ChildChangeAccumulator change_accumulator;

  IndexedVariant expected_result(Variant::EmptyMap());
  ChildChangeAccumulator expected_changes{
      std::make_pair(
          "aaa",
          ChildRemovedChange("aaa",
                             std::map<Variant, Variant>{
                                 std::make_pair("bbb",
                                                std::map<Variant, Variant>{
                                                    std::make_pair("ccc", 100),
                                                }),
                             })),
  };
  EXPECT_EQ(filter.UpdateChild(indexed_variant, key, new_child, affected_path,
                               source, &change_accumulator),
            expected_result);
  EXPECT_THAT(change_accumulator, Pointwise(Eq(), expected_changes));
}

TEST(IndexedFilterDeathTest, UpdateChild_OrderByMismatch) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  IndexedFilter filter(params);

  IndexedVariant good_snap(Variant(), params);
  IndexedVariant bad_snap;

  // Should be fine.
  filter.UpdateChild(good_snap, "irrelevant_key", Variant("irrelevant variant"),
                     Path("irrelevant/path"), nullptr, nullptr);

  // Should die.
  EXPECT_DEATH(filter.UpdateChild(bad_snap, "irrelevant_key",
                                  Variant("irrelevant variant"),
                                  Path("irrelevant/path"), nullptr, nullptr),
               DEATHTEST_SIGABRT);
}

TEST(IndexedFilter, UpdateFullVariant) {
  {
    QueryParams params;
    IndexedFilter filter(params);

    IndexedVariant old_snap(Variant(std::map<Variant, Variant>{
        std::make_pair(".value",
                       std::map<Variant, Variant>{
                           std::make_pair("to_be_changed", 100),
                           std::make_pair("to_be_removed", 200),
                           std::make_pair("unchanged", 300),
                       }),
    }));
    IndexedVariant new_snap(Variant(std::map<Variant, Variant>{
        std::make_pair(".value",
                       std::map<Variant, Variant>{
                           std::make_pair("to_be_changed", 400),
                           std::make_pair("unchanged", 300),
                           std::make_pair("was_added", 500),
                       }),
    }));
    ChildChangeAccumulator change_accumulator;

    ChildChangeAccumulator expected_changes{
        std::make_pair("to_be_changed",
                       ChildChangedChange("to_be_changed", 400, 100)),
        std::make_pair("to_be_removed",
                       ChildRemovedChange("to_be_removed", 200)),
        std::make_pair("was_added", ChildAddedChange("was_added", 500)),
    };

    EXPECT_EQ(filter.UpdateFullVariant(old_snap, new_snap, &change_accumulator),
              new_snap);
    EXPECT_THAT(change_accumulator, Pointwise(Eq(), expected_changes));
  }
  {
    QueryParams params;
    IndexedFilter filter(params);

    IndexedVariant old_snap(Variant(std::map<Variant, Variant>{
        std::make_pair("to_be_changed", 100),
        std::make_pair("to_be_removed", 200),
        std::make_pair("unchanged", 300),
    }));
    IndexedVariant new_snap(Variant(std::map<Variant, Variant>{
        std::make_pair(".value",
                       std::map<Variant, Variant>{
                           std::make_pair("to_be_changed", 400),
                           std::make_pair("unchanged", 300),
                           std::make_pair("was_added", 500),
                       }),
    }));
    ChildChangeAccumulator change_accumulator;

    ChildChangeAccumulator expected_changes{
        std::make_pair("to_be_changed",
                       ChildChangedChange("to_be_changed", 400, 100)),
        std::make_pair("to_be_removed",
                       ChildRemovedChange("to_be_removed", 200)),
        std::make_pair("was_added", ChildAddedChange("was_added", 500)),
    };

    EXPECT_EQ(filter.UpdateFullVariant(old_snap, new_snap, &change_accumulator),
              new_snap);
    EXPECT_THAT(change_accumulator, Pointwise(Eq(), expected_changes));
  }
  {
    QueryParams params;
    IndexedFilter filter(params);

    IndexedVariant old_snap(Variant(std::map<Variant, Variant>{
        std::make_pair(".value",
                       std::map<Variant, Variant>{
                           std::make_pair("to_be_changed", 100),
                           std::make_pair("to_be_removed", 200),
                           std::make_pair("unchanged", 300),
                       }),
    }));
    IndexedVariant new_snap(Variant(std::map<Variant, Variant>{
        std::make_pair("to_be_changed", 400),
        std::make_pair("unchanged", 300),
        std::make_pair("was_added", 500),
    }));
    ChildChangeAccumulator change_accumulator;

    ChildChangeAccumulator expected_changes{
        std::make_pair("to_be_changed",
                       ChildChangedChange("to_be_changed", 400, 100)),
        std::make_pair("to_be_removed",
                       ChildRemovedChange("to_be_removed", 200)),
        std::make_pair("was_added", ChildAddedChange("was_added", 500)),
    };

    EXPECT_EQ(filter.UpdateFullVariant(old_snap, new_snap, &change_accumulator),
              new_snap);
    EXPECT_THAT(change_accumulator, Pointwise(Eq(), expected_changes));
  }
}

TEST(IndexedFilterDeathTest, UpdateFullVariant_OrderByMismatch) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  IndexedFilter filter(params);

  IndexedVariant irrelevant_snap;
  IndexedVariant good_new_snap(Variant(), params);
  IndexedVariant bad_new_snap;

  // Should not die.
  filter.UpdateFullVariant(irrelevant_snap, good_new_snap, nullptr);

  // Should die.
  EXPECT_DEATH(filter.UpdateFullVariant(irrelevant_snap, bad_new_snap, nullptr),
               DEATHTEST_SIGABRT);
}

TEST(IndexedFilter, UpdatePriority_Null) {
  QueryParams params;
  IndexedFilter filter(params);
  IndexedVariant old_snap(Variant::Null());
  Variant new_priority = 100;
  IndexedVariant result = filter.UpdatePriority(old_snap, new_priority);
  EXPECT_EQ(result.variant(), Variant::Null());
}

TEST(IndexedFilter, UpdatePriority_FundamentalType) {
  QueryParams params;
  IndexedFilter filter(params);
  IndexedVariant old_snap(Variant(100));
  Variant new_priority = "priority";
  IndexedVariant result = filter.UpdatePriority(old_snap, new_priority);
  EXPECT_EQ(result.variant(), Variant(std::map<Variant, Variant>{
                                  std::make_pair(".value", 100),
                                  std::make_pair(".priority", "priority"),
                              }));
}

TEST(IndexedFilter, UpdatePriority_Map) {
  QueryParams params;
  IndexedFilter filter(params);
  IndexedVariant old_snap(Variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 111),
      std::make_pair("bbb", 222),
      std::make_pair("ccc", 333),
  }));
  Variant new_priority = "banana";
  IndexedVariant result = filter.UpdatePriority(old_snap, new_priority);
  EXPECT_EQ(result.variant(), Variant(std::map<Variant, Variant>{
                                  std::make_pair("aaa", 111),
                                  std::make_pair("bbb", 222),
                                  std::make_pair("ccc", 333),
                                  std::make_pair(".priority", "banana"),
                              }));
}

TEST(IndexedFilter, FiltersVariants) {
  QueryParams params;
  IndexedFilter filter(params);
  EXPECT_FALSE(filter.FiltersVariants());
}

TEST(IndexedFilter, GetIndexedFilter) {
  QueryParams params;
  IndexedFilter filter(params);
  EXPECT_EQ(filter.GetIndexedFilter(), &filter);
}

TEST(IndexedFilter, query_spec) {
  QueryParams params;
  IndexedFilter filter(params);
  EXPECT_EQ(filter.query_params(), params);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
