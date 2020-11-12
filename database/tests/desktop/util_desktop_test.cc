// Copyright 2017 Google LLC
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

#include "database/src/desktop/util_desktop.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <direct.h>
static const char* kPathSep = "\\";
#define unlink _unlink
#else
static const char* kPathSep = "//";
#endif

#include "app/src/filesystem.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

using ::testing::Eq;
using ::testing::Pair;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

TEST(UtilDesktopTest, IsPriorityKey) {
  EXPECT_FALSE(IsPriorityKey(""));
  EXPECT_FALSE(IsPriorityKey("A"));
  EXPECT_FALSE(IsPriorityKey(".priority_queue"));
  EXPECT_FALSE(IsPriorityKey(".priority "));
  EXPECT_FALSE(IsPriorityKey(" .priority"));
  EXPECT_TRUE(IsPriorityKey(".priority"));
}

TEST(UtilDesktopTest, StringStartsWith) {
  EXPECT_TRUE(StringStartsWith("abcde", ""));
  EXPECT_TRUE(StringStartsWith("abcde", "abc"));
  EXPECT_TRUE(StringStartsWith("abcde", "abcde"));

  EXPECT_FALSE(StringStartsWith("abcde", "zzzzz"));
  EXPECT_FALSE(StringStartsWith("abcde", "aaaaa"));
  EXPECT_FALSE(StringStartsWith("abcde", "cde"));
  EXPECT_FALSE(StringStartsWith("abcde", "abcdefghijklmnopqrstuvwxyz"));
}

TEST(UtilDesktopTest, MapGet) {
  std::map<std::string, int> string_map{
      std::make_pair("one", 1),
      std::make_pair("two", 2),
      std::make_pair("three", 3),
  };

  // Get a value that does exist, non-const.
  EXPECT_EQ(*MapGet(&string_map, "one"), 1);
  EXPECT_EQ(*MapGet(&string_map, std::string("one")), 1);
  // Get a value that does not exist, non-const.
  EXPECT_EQ(MapGet(&string_map, "zero"), nullptr);
  EXPECT_EQ(MapGet(&string_map, std::string("zero")), nullptr);
  // Get a value that does exist, const.
  EXPECT_EQ(*MapGet(&string_map, "two"), 2);
  EXPECT_EQ(*MapGet(&string_map, std::string("two")), 2);
  // Get a value that does not exist, const.
  EXPECT_EQ(MapGet(&string_map, "zero"), nullptr);
  EXPECT_EQ(MapGet(&string_map, std::string("zero")), nullptr);
}

TEST(UtilDesktopTest, Extend) {
  {
    std::vector<int> a{1, 2, 3, 4};
    std::vector<int> b{5, 6, 7, 8};

    Extend(&a, b);
    EXPECT_EQ(a, (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}));
  }
  {
    std::vector<int> a;
    std::vector<int> b{5, 6, 7, 8};

    Extend(&a, b);
    EXPECT_EQ(a, (std::vector<int>{5, 6, 7, 8}));
  }
  {
    std::vector<int> a{1, 2, 3, 4};
    std::vector<int> b;

    Extend(&a, b);
    EXPECT_EQ(a, (std::vector<int>{1, 2, 3, 4}));
  }
}

TEST(UtilDesktopTest, PatchVariant) {
  std::map<Variant, Variant> starting_map{
      std::make_pair("a", 1),
      std::make_pair("b", 2),
      std::make_pair("c", 3),
  };

  // Completely overlapping data.
  {
    std::map<Variant, Variant> patch_map{
        std::make_pair("a", 10),
        std::make_pair("b", 20),
        std::make_pair("c", 30),
    };
    Variant data(starting_map);
    Variant patch_data(patch_map);
    EXPECT_TRUE(PatchVariant(patch_data, &data));
    EXPECT_TRUE(data.is_map());
    EXPECT_THAT(data.map(), UnorderedElementsAre(Pair(Eq("a"), Eq(10)),
                                                 Pair(Eq("b"), Eq(20)),
                                                 Pair(Eq("c"), Eq(30))));
  }

  // Completely disjoint data.
  {
    std::map<Variant, Variant> patch_map{
        std::make_pair("d", 40),
        std::make_pair("e", 50),
        std::make_pair("f", 60),
    };
    Variant data(starting_map);
    Variant patch_data(patch_map);
    EXPECT_TRUE(PatchVariant(patch_data, &data));
    EXPECT_TRUE(data.is_map());
    EXPECT_THAT(data.map(), UnorderedElementsAre(
                                Pair(Eq("a"), Eq(1)), Pair(Eq("b"), Eq(2)),
                                Pair(Eq("c"), Eq(3)), Pair(Eq("d"), Eq(40)),
                                Pair(Eq("e"), Eq(50)), Pair(Eq("f"), Eq(60))));
  }

  // Partially overlapping data.
  {
    std::map<Variant, Variant> patch_map{
        std::make_pair("a", 100),
        std::make_pair("d", 400),
        std::make_pair("f", 600),
    };
    Variant data(starting_map);
    Variant patch_data(patch_map);
    EXPECT_TRUE(PatchVariant(patch_data, &data));
    EXPECT_TRUE(data.is_map());
    EXPECT_THAT(data.map(), UnorderedElementsAre(
                                Pair(Eq("a"), Eq(100)), Pair(Eq("b"), Eq(2)),
                                Pair(Eq("c"), Eq(3)), Pair(Eq("d"), Eq(400)),
                                Pair(Eq("f"), Eq(600))));
  }

  // Source data is not a map.
  {
    Variant data;
    std::map<Variant, Variant> patch_map{
        std::make_pair("a", 10),
        std::make_pair("b", 20),
        std::make_pair("c", 30),
    };
    Variant patch_data(patch_map);
    EXPECT_FALSE(PatchVariant(patch_data, &data));
  }
  // Patch data is not a map.
  {
    Variant data(starting_map);
    Variant patch_data;

    EXPECT_FALSE(PatchVariant(patch_data, &data));
  }

  // Neither source nor patch data is a map.
  {
    Variant data;
    Variant patch_data;
    EXPECT_FALSE(PatchVariant(patch_data, &data));
  }
}

TEST(UtilDesktopTest, VariantGetChild) {
  Variant null_variant;
  EXPECT_EQ(VariantGetChild(&null_variant, Path()), Variant::Null());
  EXPECT_EQ(VariantGetChild(&null_variant, Path("aaa")), Variant::Null());
  EXPECT_EQ(VariantGetChild(&null_variant, Path("aaa/bbb")), Variant::Null());

  Variant leaf_variant = 100;
  EXPECT_EQ(VariantGetChild(&leaf_variant, Path()), 100);
  EXPECT_EQ(VariantGetChild(&leaf_variant, Path("aaa")), Variant::Null());
  EXPECT_EQ(VariantGetChild(&leaf_variant, Path("aaa/bbb")), Variant::Null());

  Variant prioritized_leaf_variant = std::map<Variant, Variant>{
      std::make_pair(".priority", 10),
      std::make_pair(".value", 100),
  };
  EXPECT_EQ(VariantGetChild(&prioritized_leaf_variant, Path()),
            Variant(std::map<Variant, Variant>{
                std::make_pair(".priority", 10),
                std::make_pair(".value", 100),
            }));
  EXPECT_EQ(VariantGetChild(&prioritized_leaf_variant, Path("aaa")),
            Variant::Null());
  EXPECT_EQ(VariantGetChild(&prioritized_leaf_variant, Path("aaa/bbb")),
            Variant::Null());

  Variant map_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
      std::make_pair("fff",
                     std::map<Variant, Variant>{
                         std::make_pair("ggg", 500),
                         std::make_pair("hhh", 600),
                         std::make_pair("iii", 700),
                     }),
  });

  EXPECT_EQ(VariantGetChild(&map_variant, Path()), map_variant);
  EXPECT_EQ(VariantGetChild(&map_variant, Path("aaa")), 100);
  EXPECT_EQ(VariantGetChild(&map_variant, Path("aaa/bbb")), Variant::Null());
  EXPECT_EQ(VariantGetChild(&map_variant, Path("bbb")),
            Variant(std::map<Variant, Variant>{
                std::make_pair("ccc", 200),
                std::make_pair("ddd", 300),
                std::make_pair("eee", 400),
            }));
  EXPECT_EQ(VariantGetChild(&map_variant, Path("bbb/ccc")), Variant(200));

  Variant prioritized_map_variant(std::map<Variant, Variant>{
      std::make_pair(".priority", 1),
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair(".value", 100),
                         std::make_pair(".priority", 1),
                     }),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                         std::make_pair(".priority", 2),
                     }),
      std::make_pair("fff",
                     std::map<Variant, Variant>{
                         std::make_pair("ggg", 500),
                         std::make_pair("hhh", 600),
                         std::make_pair("iii", 700),
                         std::make_pair(".priority", 3),
                     }),
  });

  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, Path()),
            prioritized_map_variant);
  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, Path("aaa")),
            Variant(std::map<Variant, Variant>{
                std::make_pair(".value", 100),
                std::make_pair(".priority", 1),
            }));
  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, Path("aaa/bbb")),
            Variant::Null());
  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, Path("bbb")),
            Variant(std::map<Variant, Variant>{
                std::make_pair("ccc", 200),
                std::make_pair("ddd", 300),
                std::make_pair("eee", 400),
                std::make_pair(".priority", 2),
            }));
  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, Path("bbb/ccc")),
            Variant(200));
}

TEST(UtilDesktopTest, VariantGetImmediateChild) {
  Variant null_variant;
  EXPECT_EQ(VariantGetChild(&null_variant, "aaa"), Variant::Null());
  EXPECT_EQ(VariantGetChild(&null_variant, ".priority"), Variant::Null());

  Variant leaf_variant = 100;
  EXPECT_EQ(VariantGetChild(&leaf_variant, "aaa"), Variant::Null());

  Variant prioritized_leaf_variant = std::map<Variant, Variant>{
      std::make_pair(".priority", 10),
      std::make_pair(".value", 100),
  };
  EXPECT_EQ(VariantGetChild(&prioritized_leaf_variant, "aaa"), Variant::Null());

  Variant map_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
      std::make_pair("fff",
                     std::map<Variant, Variant>{
                         std::make_pair("ggg", 500),
                         std::make_pair("hhh", 600),
                         std::make_pair("iii", 700),
                     }),
  });

  EXPECT_EQ(VariantGetChild(&map_variant, "aaa"), 100);
  EXPECT_EQ(VariantGetChild(&map_variant, "bbb"),
            Variant(std::map<Variant, Variant>{
                std::make_pair("ccc", 200),
                std::make_pair("ddd", 300),
                std::make_pair("eee", 400),
            }));

  Variant prioritized_map_variant(std::map<Variant, Variant>{
      std::make_pair(".priority", 1),
      std::make_pair("aaa",
                     std::map<Variant, Variant>{
                         std::make_pair(".value", 100),
                         std::make_pair(".priority", 1),
                     }),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                         std::make_pair(".priority", 2),
                     }),
      std::make_pair("fff",
                     std::map<Variant, Variant>{
                         std::make_pair("ggg", 500),
                         std::make_pair("hhh", 600),
                         std::make_pair("iii", 700),
                         std::make_pair(".priority", 3),
                     }),
  });

  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, "aaa"),
            Variant(std::map<Variant, Variant>{
                std::make_pair(".value", 100),
                std::make_pair(".priority", 1),
            }));
  EXPECT_EQ(VariantGetChild(&prioritized_map_variant, "bbb"),
            Variant(std::map<Variant, Variant>{
                std::make_pair("ccc", 200),
                std::make_pair("ddd", 300),
                std::make_pair("eee", 400),
                std::make_pair(".priority", 2),
            }));
}

TEST(UtilDesktopTest, VariantUpdateChild_NullVariant) {
  Variant null_variant;

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path(), Variant::Null());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path(), 100);
  EXPECT_EQ(null_variant, Variant(100));

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa"), Variant::Null());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa/bbb"), Variant::Null());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa"), Variant::EmptyMap());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa/bbb"), Variant::EmptyMap());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path(".priority"), 100);
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa/.priority"), 100);
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa"), 100);
  EXPECT_EQ(null_variant,
            Variant(std::map<Variant, Variant>{std::make_pair("aaa", 100)}));

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa/bbb"), 1234);
  EXPECT_EQ(null_variant, Variant(std::map<Variant, Variant>{
                              std::make_pair("aaa",
                                             std::map<Variant, Variant>{
                                                 std::make_pair("bbb", 1234),
                                             }),
                          }));
}

TEST(UtilDesktopTest, VariantUpdateChild_LeafVariant) {
  Variant leaf_variant = 100;

  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path(), Variant::Null());
  EXPECT_EQ(leaf_variant, Variant::Null());

  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path(), Variant(1234));
  EXPECT_EQ(leaf_variant, Variant(1234));

  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path("aaa"), Variant::Null());
  EXPECT_EQ(leaf_variant, Variant(100));

  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path("aaa/bbb"), Variant::Null());
  EXPECT_EQ(leaf_variant, Variant(100));

  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path("aaa/bbb"), Variant::EmptyMap());
  EXPECT_EQ(leaf_variant, Variant(100));

  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path(".priority"), 999);
  EXPECT_EQ(leaf_variant, Variant(std::map<Variant, Variant>{
                              std::make_pair(".priority", 999),
                              std::make_pair(".value", 100),
                          }));
  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path("aaa"), 1234);
  EXPECT_EQ(leaf_variant,
            Variant(std::map<Variant, Variant>{std::make_pair("aaa", 1234)}));
  leaf_variant = 100;
  VariantUpdateChild(&leaf_variant, Path("aaa/bbb"), 1234);
  EXPECT_EQ(leaf_variant, Variant(std::map<Variant, Variant>{
                              std::make_pair("aaa",
                                             std::map<Variant, Variant>{
                                                 std::make_pair("bbb", 1234),
                                             }),
                          }));

  const Variant original_prioritized_leaf_variant = std::map<Variant, Variant>{
      std::make_pair(".priority", 10),
      std::make_pair(".value", 100),
  };
  Variant prioritized_leaf_variant;

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path(), Variant::Null());
  EXPECT_EQ(prioritized_leaf_variant, Variant::Null());

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path(), Variant(1234));
  EXPECT_EQ(prioritized_leaf_variant, Variant(1234));

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path("aaa"), Variant::Null());
  EXPECT_EQ(prioritized_leaf_variant, original_prioritized_leaf_variant);

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path("aaa/bbb"),
                     Variant::Null());
  EXPECT_EQ(prioritized_leaf_variant, original_prioritized_leaf_variant);

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path("aaa/bbb"),
                     Variant::EmptyMap());
  EXPECT_EQ(prioritized_leaf_variant, original_prioritized_leaf_variant);

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path(".priority"), 999);
  EXPECT_EQ(prioritized_leaf_variant, Variant(std::map<Variant, Variant>{
                                          std::make_pair(".priority", 999),
                                          std::make_pair(".value", 100),
                                      }));

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path("aaa"), 1234);
  EXPECT_EQ(prioritized_leaf_variant, Variant(std::map<Variant, Variant>{
                                          std::make_pair(".priority", 10),
                                          std::make_pair("aaa", 1234),
                                      }));

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, Path("aaa/bbb"), 1234);
  EXPECT_EQ(prioritized_leaf_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa",
                               std::map<Variant, Variant>{
                                   std::make_pair("bbb", 1234),
                               }),
                std::make_pair(".priority", 10),
            }));
}

TEST(UtilDesktopTest, VariantUpdateChild_MapVariant) {
  Variant original_map_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
  });
  Variant map_variant = original_map_variant;

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path(), Variant::Null());
  EXPECT_EQ(map_variant, Variant::Null());

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path(), Variant(9999));
  EXPECT_EQ(map_variant, Variant(9999));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path(".priority"), Variant::Null());
  EXPECT_EQ(map_variant, original_map_variant);

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path(".priority"), Variant(9999));
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                             std::make_pair(".priority", 9999),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path("aaa"), Variant::Null());
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path("aaa"), Variant(9999));
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 9999),
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path("bbb"), Variant::Null());
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path("bbb"), Variant(9999));
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                             std::make_pair("bbb", 9999),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path("ccc"), Variant::Null());
  EXPECT_EQ(map_variant, original_map_variant);

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, Path("ccc"), Variant(9999));
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                             std::make_pair("ccc", 9999),
                         }));

  Variant original_prioritized_map_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
      std::make_pair(".priority", 1234),
  });
  Variant prioritized_map_variant;

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path(), Variant::Null());
  EXPECT_EQ(prioritized_map_variant, Variant::Null());

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path(), Variant(9999));
  EXPECT_EQ(prioritized_map_variant, Variant(9999));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path(".priority"),
                     Variant::Null());
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 100),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
            }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path(".priority"),
                     Variant(9999));
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 100),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair(".priority", 9999),
            }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path("aaa"), Variant::Null());
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair(".priority", 1234),
            }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path("aaa"), Variant(9999));
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 9999),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair(".priority", 1234),
            }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path("bbb"), Variant::Null());
  EXPECT_EQ(prioritized_map_variant, Variant(std::map<Variant, Variant>{
                                         std::make_pair("aaa", 100),
                                         std::make_pair(".priority", 1234),
                                     }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path("bbb"), Variant(9999));
  EXPECT_EQ(prioritized_map_variant, Variant(std::map<Variant, Variant>{
                                         std::make_pair("aaa", 100),
                                         std::make_pair("bbb", 9999),
                                         std::make_pair(".priority", 1234),
                                     }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path("ccc"), Variant::Null());
  EXPECT_EQ(prioritized_map_variant, original_prioritized_map_variant);

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, Path("ccc"), Variant(9999));
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 100),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair("ccc", 9999),
                std::make_pair(".priority", 1234),
            }));
}

TEST(UtilDesktopTest, VariantUpdateImmediateChild_NullVariant) {
  Variant null_variant;

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa"), Variant::Null());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa"), Variant::EmptyMap());
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path(".priority"), 100);
  EXPECT_EQ(null_variant, Variant::Null());

  null_variant = Variant::Null();
  VariantUpdateChild(&null_variant, Path("aaa"), 100);
  EXPECT_EQ(null_variant,
            Variant(std::map<Variant, Variant>{std::make_pair("aaa", 100)}));
}

TEST(UtilDesktopTest, VariantUpdateImmediateChild_LeafVariant) {
  const Variant original_leaf_variant = 100;
  Variant leaf_variant;

  leaf_variant = original_leaf_variant;
  VariantUpdateChild(&leaf_variant, "aaa", Variant::Null());
  EXPECT_EQ(leaf_variant, Variant(100));

  leaf_variant = original_leaf_variant;
  VariantUpdateChild(&leaf_variant, "aaa", Variant::EmptyMap());
  EXPECT_EQ(leaf_variant, Variant(100));

  leaf_variant = original_leaf_variant;
  VariantUpdateChild(&leaf_variant, ".priority", 999);
  EXPECT_EQ(leaf_variant, Variant(std::map<Variant, Variant>{
                              std::make_pair(".priority", 999),
                              std::make_pair(".value", 100),
                          }));

  leaf_variant = original_leaf_variant;
  VariantUpdateChild(&leaf_variant, "aaa", 1234);
  EXPECT_EQ(leaf_variant,
            Variant(std::map<Variant, Variant>{std::make_pair("aaa", 1234)}));

  const Variant original_prioritized_leaf_variant = std::map<Variant, Variant>{
      std::make_pair(".priority", 10),
      std::make_pair(".value", 100),
  };
  Variant prioritized_leaf_variant;

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, "aaa", Variant::Null());
  EXPECT_EQ(prioritized_leaf_variant, original_prioritized_leaf_variant);

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, "aaa", Variant::EmptyMap());
  EXPECT_EQ(prioritized_leaf_variant, original_prioritized_leaf_variant);

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, ".priority", 999);
  EXPECT_EQ(prioritized_leaf_variant, Variant(std::map<Variant, Variant>{
                                          std::make_pair(".priority", 999),
                                          std::make_pair(".value", 100),
                                      }));

  prioritized_leaf_variant = original_prioritized_leaf_variant;
  VariantUpdateChild(&prioritized_leaf_variant, "aaa", 1234);
  EXPECT_EQ(prioritized_leaf_variant, Variant(std::map<Variant, Variant>{
                                          std::make_pair(".priority", 10),
                                          std::make_pair("aaa", 1234),
                                      }));
}

TEST(UtilDesktopTest, VariantUpdateImmediateChild_MapVariant) {
  Variant original_map_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
  });
  Variant map_variant;

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, ".priority", 9999);
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                             std::make_pair(".priority", 9999),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, "aaa", 9999);
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 9999),
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, "bbb", 9999);
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                             std::make_pair("bbb", 9999),
                         }));

  map_variant = original_map_variant;
  VariantUpdateChild(&map_variant, "ccc", 9999);
  EXPECT_EQ(map_variant, Variant(std::map<Variant, Variant>{
                             std::make_pair("aaa", 100),
                             std::make_pair("bbb",
                                            std::map<Variant, Variant>{
                                                std::make_pair("ccc", 200),
                                                std::make_pair("ddd", 300),
                                                std::make_pair("eee", 400),
                                            }),
                             std::make_pair("ccc", 9999),
                         }));

  Variant original_prioritized_map_variant(std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb",
                     std::map<Variant, Variant>{
                         std::make_pair("ccc", 200),
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
      std::make_pair(".priority", 1234),
  });
  Variant prioritized_map_variant;

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, ".priority", 9999);
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 100),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair(".priority", 9999),
            }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, "aaa", 9999);
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 9999),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair(".priority", 1234),
            }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, "bbb", 9999);
  EXPECT_EQ(prioritized_map_variant, Variant(std::map<Variant, Variant>{
                                         std::make_pair("aaa", 100),
                                         std::make_pair("bbb", 9999),
                                         std::make_pair(".priority", 1234),
                                     }));

  prioritized_map_variant = original_prioritized_map_variant;
  VariantUpdateChild(&prioritized_map_variant, "ccc", 9999);
  EXPECT_EQ(prioritized_map_variant,
            Variant(std::map<Variant, Variant>{
                std::make_pair("aaa", 100),
                std::make_pair("bbb",
                               std::map<Variant, Variant>{
                                   std::make_pair("ccc", 200),
                                   std::make_pair("ddd", 300),
                                   std::make_pair("eee", 400),
                               }),
                std::make_pair("ccc", 9999),
                std::make_pair(".priority", 1234),
            }));
}

TEST(UtilDesktopTest, GetVariantAtPath) {
  std::map<Variant, Variant> candy{};
  std::map<Variant, Variant> fruits{
      std::make_pair("apple", "red"),
      std::make_pair("banana", "yellow"),
      std::make_pair("grape", "purple"),
  };
  std::map<Variant, Variant> vegetables{
      std::make_pair(".value", std::map<Variant, Variant>{
                                   std::make_pair("broccoli", "green"),
                                   std::make_pair("carrot", "orange"),
                                   std::make_pair("cauliflower", "white"),
                               })};
  std::map<Variant, Variant> healthy_food_map{
      std::make_pair("candy", candy),
      std::make_pair("fruits", fruits),
      std::make_pair("vegetables", vegetables),
  };

  // Get root value.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = GetInternalVariant(&healthy_food, Path::GetRoot());
    EXPECT_EQ(result, &healthy_food);
  }

  // Get valid child.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = GetInternalVariant(&healthy_food, Path("fruits"));
    EXPECT_EQ(result, &healthy_food.map()["fruits"]);
  }

  // Get valid grandchild.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result =
        GetInternalVariant(&healthy_food, Path("vegetables/carrot"));
    EXPECT_EQ(
        result,
        &healthy_food.map()["vegetables"].map()[".value"].map()["carrot"]);
  }

  // Get invalid child.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = GetInternalVariant(&healthy_food, Path("cereal"));
    EXPECT_EQ(result, nullptr);
  }

  // Get invalid grandchild.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result =
        GetInternalVariant(&healthy_food, Path("candy/marshmallows"));
    EXPECT_EQ(result, nullptr);
  }

  // Attempt to retrieve something from a non-map.
  {
    Variant not_a_map(100);
    Variant* result = GetInternalVariant(&not_a_map, Path("fruits"));
    EXPECT_EQ(result, nullptr);
  }
}

TEST(UtilDesktopTest, GetVariantAtKey) {
  std::map<Variant, Variant> candy{};
  std::map<Variant, Variant> fruits{
      std::make_pair("apple", "red"),
      std::make_pair("banana", "yellow"),
      std::make_pair("grape", "purple"),
  };
  std::map<Variant, Variant> vegetables{
      std::make_pair("broccoli", "green"),
      std::make_pair("carrot", "orange"),
      std::make_pair("cauliflower", "white"),
  };
  std::map<Variant, Variant> healthy_food_map{
      std::make_pair(".value",
                     std::map<Variant, Variant>{
                         std::make_pair("candy", candy),
                         std::make_pair("fruits", fruits),
                         std::make_pair("vegetables", vegetables),
                     }),
  };

  // Get valid child.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = GetInternalVariant(&healthy_food, "fruits");
    EXPECT_EQ(result, &healthy_food.map()[".value"].map()["fruits"]);
  }

  // Try and fail to get a grandchild.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = GetInternalVariant(&healthy_food, "vegetables/carrot");
    EXPECT_EQ(result, nullptr);
  }

  // Get invalid child.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = GetInternalVariant(&healthy_food, "cereal");
    EXPECT_EQ(result, nullptr);
  }

  // Attempt to retrieve something from a non-map.
  {
    Variant not_a_map(100);
    Variant* result = GetInternalVariant(&not_a_map, "fruits");
    EXPECT_EQ(result, nullptr);
  }
}

TEST(UtilDesktopTest, MakeVariantAtPath) {
  std::map<Variant, Variant> healthy_food_map{
      std::make_pair("candy", std::map<Variant, Variant>{}),
      std::make_pair("fruits",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("banana", "yellow"),
                         std::make_pair("grape", "purple"),
                     }),
      std::make_pair("vegetables",
                     std::map<Variant, Variant>{
                         std::make_pair("broccoli", "green"),
                         std::make_pair("carrot", "orange"),
                         std::make_pair("cauliflower",
                                        std::map<Variant, Variant>{
                                            std::make_pair(".value", "white"),
                                            std::make_pair(".priority", 100),
                                        }),
                     }),
  };

  // Get root value.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = MakeVariantAtPath(&healthy_food, Path::GetRoot());
    EXPECT_EQ(*result, healthy_food);
  }

  // Get valid child.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = MakeVariantAtPath(&healthy_food, Path("fruits"));
    EXPECT_EQ(result, &healthy_food.map()["fruits"]);
  }

  // Get valid grandchild.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result =
        MakeVariantAtPath(&healthy_food, Path("vegetables/carrot"));
    EXPECT_EQ(result, &healthy_food.map()["vegetables"].map()["carrot"]);
  }

  // Get invalid child.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result = MakeVariantAtPath(&healthy_food, Path("cereal"));
    EXPECT_EQ(result, &healthy_food.map()["cereal"]);
    EXPECT_TRUE(healthy_food.map()["candy"].is_map());
    EXPECT_EQ(healthy_food.map()["candy"].map().size(), 0);
  }

  // Get invalid grandchild.
  {
    Variant healthy_food(healthy_food_map);
    Variant* result =
        MakeVariantAtPath(&healthy_food, Path("candy/marshmallows"));
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(healthy_food.is_map());
    EXPECT_TRUE(healthy_food.map()["candy"].is_map());
    EXPECT_NE(healthy_food.map()["candy"].map().find("marshmallows"),
              healthy_food.map()["candy"].map().end());
    EXPECT_EQ(result, &healthy_food.map()["candy"].map()["marshmallows"]);
  }

  // Attempt to retrieve something from a non-map.
  {
    Variant not_a_map(100);
    Variant* result = MakeVariantAtPath(&not_a_map, Path("fruits"));
    EXPECT_TRUE(not_a_map.is_map());
    EXPECT_EQ(result, &not_a_map.map()["fruits"]);
    EXPECT_NE(not_a_map.map().find("fruits"), not_a_map.map().end());
  }

  // Attempt to retrieve a node with a ".value".
  {
    Variant healthy_food(healthy_food_map);
    Variant* result =
        MakeVariantAtPath(&healthy_food, Path("vegetables/cauliflower"));
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(*result, Variant(std::map<Variant, Variant>{
                           std::make_pair(".value", "white"),
                           std::make_pair(".priority", 100),
                       }));
  }

  // Attempt to retrieve a node past a ".value".
  {
    Variant healthy_food(healthy_food_map);
    Variant* result =
        MakeVariantAtPath(&healthy_food, Path("vegetables/cauliflower/new"));
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(
        result,
        &healthy_food.map()["vegetables"].map()["cauliflower"].map()["new"]);
    EXPECT_EQ(healthy_food.map()["vegetables"]
                  .map()["cauliflower"]
                  .map()[".priority"],
              100);
    EXPECT_EQ(*result, Variant::Null());
  }
}

TEST(UtilDesktopTest, SetVariantAtPath) {
  Variant initial = std::map<Variant, Variant>{
      std::make_pair(
          "aaa",
          std::map<Variant, Variant>{
              std::make_pair("bbb", 100),
              std::make_pair("ccc", 200),
              std::make_pair("ddd",
                             std::map<Variant, Variant>{
                                 std::make_pair(".value", 300),
                                 std::make_pair(".priority", 999),
                             }),
          }),
  };

  // Change existing value
  {
    Variant variant = initial;
    SetVariantAtPath(&variant, Path("aaa/bbb"), 1000);

    Variant expected = std::map<Variant, Variant>{
        std::make_pair(
            "aaa",
            std::map<Variant, Variant>{
                std::make_pair("bbb", 1000),
                std::make_pair("ccc", 200),
                std::make_pair("ddd",
                               std::map<Variant, Variant>{
                                   std::make_pair(".value", 300),
                                   std::make_pair(".priority", 999),
                               }),
            }),
    };
    EXPECT_EQ(variant, expected);
  }

  // Change existing value inside of a .value key.
  {
    Variant variant = initial;
    SetVariantAtPath(&variant, Path("aaa/ddd"), 3000);

    Variant expected = std::map<Variant, Variant>{
        std::make_pair(
            "aaa",
            std::map<Variant, Variant>{
                std::make_pair("bbb", 100),
                std::make_pair("ccc", 200),
                std::make_pair("ddd",
                               std::map<Variant, Variant>{
                                   std::make_pair(".value", 3000),
                                   std::make_pair(".priority", 999),
                               }),
            }),
    };
    EXPECT_EQ(variant, expected);
  }

  // Add a new value.
  {
    Variant variant = initial;
    SetVariantAtPath(&variant, Path("aaa/eee"), 4000);

    Variant expected = std::map<Variant, Variant>{
        std::make_pair(
            "aaa",
            std::map<Variant, Variant>{
                std::make_pair("bbb", 100),
                std::make_pair("ccc", 200),
                std::make_pair("ddd",
                               std::map<Variant, Variant>{
                                   std::make_pair(".value", 300),
                                   std::make_pair(".priority", 999),
                               }),
                std::make_pair("eee", 4000),
            }),
    };
    EXPECT_EQ(variant, expected);
  }

  // Add map at a location with a .value
  {
    Variant variant = initial;
    SetVariantAtPath(&variant, Path("aaa/ddd"),
                     std::map<Variant, Variant>{
                         std::make_pair("zzz", 999),
                         std::make_pair("yyy", 888),
                     });

    Variant expected = std::map<Variant, Variant>{
        std::make_pair(
            "aaa",
            std::map<Variant, Variant>{
                std::make_pair("bbb", 100),
                std::make_pair("ccc", 200),
                std::make_pair("ddd",
                               std::map<Variant, Variant>{
                                   std::make_pair("zzz", 999),
                                   std::make_pair("yyy", 888),
                                   std::make_pair(".priority", 999),
                               }),
            }),
    };
    EXPECT_EQ(variant, expected);
  }
}

TEST(UtilDesktopTest, ParseUrlSupportCases) {
  // Without Path
  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("https://test.firebaseio.com/"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("https://test.firebaseio.com"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("https://test-123.firebaseio.com"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test-123.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test-123");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("http://test.firebaseio.com"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_FALSE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("test.firebaseio.com"), ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("test.firebaseio.com/"), ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("test.firebaseio.com:80"), ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com:80");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("test.firebaseio.com:8080/"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com:8080");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "");
  }

  // With path
  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("https://test.firebaseio.com/path/to/key"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "path/to/key");
  }

  {
    ParseUrl parse_util;
    EXPECT_EQ(parse_util.Parse("https://test.firebaseio.com/path/to/key/"),
              ParseUrl::kParseOk);
    EXPECT_EQ(parse_util.hostname, "test.firebaseio.com");
    EXPECT_EQ(parse_util.ns, "test");
    EXPECT_TRUE(parse_util.secure);
    EXPECT_EQ(parse_util.path, "path/to/key/");
  }
}

TEST(UtilDesktopTest, ParseUrlErrorCases) {
  // Test Wrong Protocol
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("://"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("://test.firebaseio.com"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("ws://test.firebaseio.com"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("ftp://test.firebaseio.com"),
              ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("https:/test.firebaseio.com"),
              ParseUrl::kParseOk);
  }

  // Test wrong port
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("test.firebaseio.com:"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("test.firebaseio.com:44a"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("test.firebaseio.com:a"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("test.firebaseio.com:a43"), ParseUrl::kParseOk);
  }

  // Test Wrong hostname/namespace
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse(""), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("test"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("http://"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("http:///"), ParseUrl::kParseOk);  // NOTYPO
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("http://./"), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("http://a."), ParseUrl::kParseOk);
  }
  {
    ParseUrl parse_util;
    EXPECT_NE(parse_util.Parse("http://a....../"), ParseUrl::kParseOk);
  }
}

TEST(UtilDesktopTest, CountChildren_Fundamental_Type) {
  Variant simple_value = 10;
  EXPECT_EQ(CountEffectiveChildren(simple_value), 0);

  std::map<Variant, const Variant*> children;
  std::map<Variant, const Variant*> expect_children;
  EXPECT_THAT(GetEffectiveChildren(simple_value, &children), Eq(0));
  EXPECT_EQ(children, expect_children);
}

TEST(UtilDesktopTest, CountChildren_FundamentalTypeWithPriority) {
  Variant high_priority_food = std::map<Variant, Variant>{
      std::make_pair(".value", "milk chocolate"),
      std::make_pair(".priority", 10000),
  };
  EXPECT_EQ(CountEffectiveChildren(high_priority_food), 0);

  std::map<Variant, const Variant*> children;
  std::map<Variant, const Variant*> expect_children;
  EXPECT_THAT(GetEffectiveChildren(high_priority_food, &children), Eq(0));
  EXPECT_EQ(children, expect_children);
}

TEST(UtilDesktopTest, CountChildren_MapWithPriority) {
  // Remove priority field.
  Variant worst_foods_with_priority = std::map<Variant, Variant>{
      std::make_pair("bad", "peas"),
      std::make_pair("badder", "asparagus"),
      std::make_pair("baddest", "brussel sprouts"),
      std::make_pair(".priority", -100000),
  };
  EXPECT_EQ(CountEffectiveChildren(worst_foods_with_priority), 3);

  std::map<Variant, const Variant*> children;
  std::map<Variant, const Variant*> expect_children = {
      std::make_pair("bad", &worst_foods_with_priority.map()["bad"]),
      std::make_pair("badder", &worst_foods_with_priority.map()["badder"]),
      std::make_pair("baddest", &worst_foods_with_priority.map()["baddest"]),
  };
  EXPECT_THAT(GetEffectiveChildren(worst_foods_with_priority, &children),
              Eq(3));
  EXPECT_EQ(children, expect_children);
}

TEST(UtilDesktopTest, CountChildren_MapWithoutPriority) {
  // Remove priority field.
  Variant worst_foods = std::map<Variant, Variant>{
      std::make_pair("bad", "peas"),
      std::make_pair("badder", "asparagus"),
      std::make_pair("baddest", "brussel sprouts"),
  };
  EXPECT_EQ(CountEffectiveChildren(worst_foods), 3);

  std::map<Variant, const Variant*> children;
  std::map<Variant, const Variant*> expect_children = {
      std::make_pair("bad", &worst_foods.map()["bad"]),
      std::make_pair("badder", &worst_foods.map()["badder"]),
      std::make_pair("baddest", &worst_foods.map()["baddest"]),
  };
  EXPECT_THAT(GetEffectiveChildren(worst_foods, &children), Eq(3));
  EXPECT_EQ(children, expect_children);
}

TEST(UtilDesktopTest, HasVector) {
  EXPECT_FALSE(HasVector(Variant(10)));
  EXPECT_FALSE(HasVector(Variant("A")));
  EXPECT_FALSE(HasVector(util::JsonToVariant("{\"A\":1}")));
  EXPECT_TRUE(HasVector(util::JsonToVariant("[1,2,3]")));
  EXPECT_TRUE(HasVector(util::JsonToVariant("{\"A\":[1,2,3]}")));
}

TEST(UtilDesktopTest, ParseInteger) {
  int64_t number = 0;
  EXPECT_TRUE(ParseInteger("0", &number));
  EXPECT_EQ(number, 0);
  EXPECT_TRUE(ParseInteger("1", &number));
  EXPECT_EQ(number, 1);
  EXPECT_TRUE(ParseInteger("-1", &number));
  EXPECT_EQ(number, -1);
  EXPECT_TRUE(ParseInteger("+1", &number));
  EXPECT_EQ(number, 1);
  EXPECT_TRUE(ParseInteger("1234", &number));
  EXPECT_EQ(number, 1234);

  EXPECT_TRUE(ParseInteger("00", &number));
  EXPECT_EQ(number, 0);
  EXPECT_TRUE(ParseInteger("01", &number));
  EXPECT_EQ(number, 1);
  EXPECT_TRUE(ParseInteger("-01", &number));
  EXPECT_EQ(number, -1);

  EXPECT_FALSE(ParseInteger("1234.1", &number));
  EXPECT_FALSE(ParseInteger("1 2 3", &number));
  EXPECT_FALSE(ParseInteger("ABC", &number));
  EXPECT_FALSE(ParseInteger("1B3", &number));
  EXPECT_FALSE(ParseInteger("123.A", &number));
}

TEST(UtilDesktopTest, PrunePrioritiesAndConvertVector) {
  {
    // 10 => 10
    Variant value = 10;
    Variant expect = value;
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {".value":10, ".priority":1} => 10
    Variant value = util::JsonToVariant("{\".value\":10,\".priority\":1}");
    Variant expect = 10;
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"A":10, ".priority":1} => {"A":10}
    Variant value = util::JsonToVariant("{\"A\":10,\".priority\":1}");
    Variant expect = util::JsonToVariant("{\"A\":10}");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"A":{"B":10,".priority":2},".priority":1} => {"A":{"B":10}}
    Variant value = util::JsonToVariant(
        "{\"A\":{\"B\":10,\".priority\":2},\".priority\":1}");
    Variant expect = util::JsonToVariant("{\"A\":{\"B\":10}}");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"0":0,"1":1,"2":2} => [0,1,2]
    Variant value = util::JsonToVariant("{\"0\":0,\"1\":1,\"2\":2}");
    Variant expect = util::JsonToVariant("[0,1,2]");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"000000":0,"000001":1,"000002":2} => {"000000":0,"000001":1,"000002":2}
    Variant value =
        util::JsonToVariant("{\"000000\":0,\"000001\":1,\"000002\":2}");
    Variant expect =
        util::JsonToVariant("{\"000000\":0,\"000001\":1,\"000002\":2}");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"0":0,"2":2} => [0,null,2]
    Variant value = util::JsonToVariant("{\"0\":0,\"2\":2}");
    Variant expect = util::JsonToVariant("[0,null,2]");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // No change because more than half of the keys are missing (1, 2, 3)
    // {"3":3} => {"3":3}
    Variant value = util::JsonToVariant("{\"3\":3}");
    Variant expect = value;
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // Change because less or equal to half of the keys are missing (0, 2)
    // {"1":1,"3":3} => [null,1,null,3]
    Variant value = util::JsonToVariant("{\"1\":1,\"3\":3}");
    Variant expect = util::JsonToVariant("[null,1,null,3]");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"0":0,"1":1,"A":"2"} => {"0":0,"1":1,"A":"2"}
    Variant value = util::JsonToVariant("{\"0\":0,\"1\":1,\"A\":\"2\"}");
    Variant expect = value;
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"0":0,"1":1,".priority":1} => [0,1]
    Variant value = util::JsonToVariant("{\"0\":0,\"1\":1,\".priority\":1}");
    Variant expect = util::JsonToVariant("[0,1]");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"0":{"0":0,".priority":1},"1":1,".priority":1} => [[0],1]
    Variant value = util::JsonToVariant(
        "{\"0\":{\"0\":0,\".priority\":1},\"1\":1,\".priority\":1}");
    Variant expect = util::JsonToVariant("[[0],1]");
    PrunePrioritiesAndConvertVector(&value);
    EXPECT_EQ(value, expect);
  }
}

TEST(UtilDesktopTest, PruneNullsRecursively) {
  Variant value = std::map<Variant, Variant>{
      std::make_pair("null", Variant::Null()),
      std::make_pair("bool", false),
      std::make_pair("int", 100),
      std::make_pair("string", "I'm a string!"),
      std::make_pair("float", 3.1415926),
      std::make_pair(
          "map",
          std::map<Variant, Variant>{
              std::make_pair("another_null", Variant::Null()),
              std::make_pair("another_bool", true),
              std::make_pair("another_int", 0),
              std::make_pair("another_string", ""),
              std::make_pair("another_float", 0.0),
              std::make_pair("another_empty_map", Variant::EmptyMap()),
          }),
      std::make_pair("empty_map", Variant::EmptyMap()),
  };

  PruneNulls(&value, true);

  Variant expected = std::map<Variant, Variant>{
      std::make_pair("bool", false),
      std::make_pair("int", 100),
      std::make_pair("string", "I'm a string!"),
      std::make_pair("float", 3.1415926),
      std::make_pair("map",
                     std::map<Variant, Variant>{
                         std::make_pair("another_bool", true),
                         std::make_pair("another_int", 0),
                         std::make_pair("another_string", ""),
                         std::make_pair("another_float", 0.0),
                     }),
  };

  EXPECT_EQ(value, expected);
}

TEST(UtilDesktopTest, PruneNullsNonRecursively) {
  Variant value = std::map<Variant, Variant>{
      std::make_pair("null", Variant::Null()),
      std::make_pair("bool", false),
      std::make_pair("int", 100),
      std::make_pair("string", "I'm a string!"),
      std::make_pair("float", 3.1415926),
      std::make_pair(
          "map",
          std::map<Variant, Variant>{
              std::make_pair("another_null", Variant::Null()),
              std::make_pair("another_bool", true),
              std::make_pair("another_int", 0),
              std::make_pair("another_string", ""),
              std::make_pair("another_float", 0.0),
              std::make_pair("another_empty_map", Variant::EmptyMap()),
          }),
      std::make_pair("empty_map", Variant::EmptyMap()),
  };

  PruneNulls(&value, false);

  Variant expected = std::map<Variant, Variant>{
      std::make_pair("bool", false),
      std::make_pair("int", 100),
      std::make_pair("string", "I'm a string!"),
      std::make_pair("float", 3.1415926),
      std::make_pair(
          "map",
          std::map<Variant, Variant>{
              std::make_pair("another_null", Variant::Null()),
              std::make_pair("another_bool", true),
              std::make_pair("another_int", 0),
              std::make_pair("another_string", ""),
              std::make_pair("another_float", 0.0),
              std::make_pair("another_empty_map", Variant::EmptyMap()),
          }),
  };

  EXPECT_EQ(value, expected);
}

TEST(UtilDesktopTest, ConvertVectorToMap) {
  {
    // 10 => 10
    Variant value = 10;
    Variant expect = value;
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {".value":10, ".priority":1} => {".value":10, ".priority":1}
    Variant value = util::JsonToVariant("{\".value\":10,\".priority\":1}");
    Variant expect = value;
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"A":10, ".priority":1} => {"A":10, ".priority":1}
    Variant value = util::JsonToVariant("{\"A\":10,\".priority\":1}");
    Variant expect = value;
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"A":{"B":10,".priority":2},".priority":1} =>
    // {"A":{"B":10,".priority":2},".priority":1}
    Variant value = util::JsonToVariant(
        "{\"A\":{\"B\":10,\".priority\":2},\".priority\":1}");
    Variant expect = value;
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // [0,1,2] => {"0":0,"1":1,"2":2}
    Variant value = util::JsonToVariant("[0,1,2]");
    Variant expect = util::JsonToVariant("{\"0\":0,\"1\":1,\"2\":2}");
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // [[0,1],1,2] => {"0":{"0":0,"1":1},"1":1,"2":2}
    Variant value = util::JsonToVariant("[[0,1],1,2]");
    Variant expect =
        util::JsonToVariant("{\"0\":{\"0\":0,\"1\":1},\"1\":1,\"2\":2}");
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {"0":[0,1],".priority":1} => {"0":{"0":0,"1":1},".priority":1}
    Variant value = util::JsonToVariant("{\"0\":[0,1],\".priority\":1}");
    Variant expect =
        util::JsonToVariant("{\"0\":{\"0\":0,\"1\":1},\".priority\":1}");
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // {".value":[0,1,2],".priority":1} => {"0":0,"1":1,"2":2,".priority":1}
    Variant value = util::JsonToVariant("{\".value\":[0,1,2],\".priority\":1}");
    Variant expect =
        util::JsonToVariant("{\"0\":0,\"1\":1,\"2\":2,\".priority\":1}");
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }

  {
    // Test for sanity
    // {".value":[{".value":[0,1],".priority":3},1,2],".priority":1} =>
    // {"0":{"0":0,"1":1,".priority":3},"1":1,"2":2,".priority":1}
    Variant value = util::JsonToVariant(
        "{\".value\":[{\".value\":[0,1],\".priority\":3},1,2],\".priority\":"
        "1}");
    Variant expect = util::JsonToVariant(
        "{\"0\":{\"0\":0,\"1\":1,\".priority\":3},\"1\":1,\"2\":2,\"."
        "priority\":1}");
    ConvertVectorToMap(&value);
    EXPECT_EQ(value, expect);
  }
}

TEST(UtilDesktopTest, PrunePriorities_FundamentalType) {
  // Ensure nothing happens.
  Variant simple_value = 10;
  Variant simple_value_copy = simple_value;
  PrunePriorities(&simple_value);
  EXPECT_EQ(simple_value, simple_value_copy);
}

TEST(UtilDesktopTest, PrunePriorities_FundamentalTypeWithPriority) {
  // Collapse the value/priority pair into just a value.
  Variant high_priority_food = std::map<Variant, Variant>{
      std::make_pair(".value", "pizza"),
      std::make_pair(".priority", 10000),
  };
  PrunePriorities(&high_priority_food);
  EXPECT_THAT(high_priority_food.string_value(), StrEq("pizza"));
}

TEST(UtilDesktopTest, PrunePriorities_MapWithPriority) {
  // Remove priority field.
  Variant worst_foods_with_priority = std::map<Variant, Variant>{
      std::make_pair("bad", "peas"),
      std::make_pair("badder", "asparagus"),
      std::make_pair("baddest", "brussel sprouts"),
      std::make_pair(".priority", -100000),
  };
  Variant worst_foods = std::map<Variant, Variant>{
      std::make_pair("bad", "peas"),
      std::make_pair("badder", "asparagus"),
      std::make_pair("baddest", "brussel sprouts"),
  };
  PrunePriorities(&worst_foods_with_priority);
  EXPECT_EQ(worst_foods_with_priority, worst_foods);
}

TEST(UtilDesktopTest, PrunePriorities_NestedMaps) {
  // Correctly handle recursive maps.
  Variant nested_map = std::map<Variant, Variant>{
      std::make_pair("simple_value", 1),
      std::make_pair("prioritized_value",
                     std::map<Variant, Variant>{
                         std::make_pair(".value", "pizza"),
                         std::make_pair(".priority", 10000),
                     }),
      std::make_pair("prioritized_map",
                     std::map<Variant, Variant>{
                         std::make_pair("bad", "peas"),
                         std::make_pair("badder", "asparagus"),
                         std::make_pair("baddest", "brussel sprouts"),
                         std::make_pair(".priority", -100000),
                     }),
  };
  Variant nested_map_expectation = std::map<Variant, Variant>{
      std::make_pair("simple_value", 1),
      std::make_pair("prioritized_value", "pizza"),
      std::make_pair("prioritized_map",
                     std::map<Variant, Variant>{
                         std::make_pair("bad", "peas"),
                         std::make_pair("badder", "asparagus"),
                         std::make_pair("baddest", "brussel sprouts"),
                     }),
  };
  PrunePriorities(&nested_map);
  EXPECT_EQ(nested_map, nested_map_expectation);
}

TEST(UtilDesktopTest, GetVariantValueAndGetVariantPriority) {
  // Test with Null priority
  {
    // Pairs of value and expected result
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"", ""},  // Variant::Null()
        {"123", "123"},
        {"123.456", "123.456"},
        {"'string'", "'string'"},
        {"true", "true"},
        {"false", "false"},
        {"[1,2,3]", "[1,2,3]"},
        {"{'A':1,'B':'b','C':true}", "{'A':1,'B':'b','C':true}"},
        {"{'A':1,'B':{'.value':'b','.priority':100},'C':true}",
         "{'A':1,'B':{'.value':'b','.priority':100},'C':true}"},
    };

    for (auto& test : test_cases) {
      // Replace all \' to \" to match Json format.  Used \' for readability
      std::replace(test.first.begin(), test.first.end(), '\'', '\"');
      std::replace(test.second.begin(), test.second.end(), '\'', '\"');

      Variant original_variant = util::JsonToVariant(test.first.c_str());
      Variant expected = util::JsonToVariant(test.second.c_str());

      const Variant* value_ptr = GetVariantValue(&original_variant);
      const Variant priority = GetVariantPriority(original_variant);

      EXPECT_NE(value_ptr, nullptr);
      EXPECT_EQ(value_ptr, &original_variant);
      EXPECT_EQ(*value_ptr, expected);

      EXPECT_EQ(priority, Variant::Null());
    }
  }

  // Test with priority
  {
    // Pairs of value and expected result
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"{'.value':123,'.priority':100}", "123"},
        {"{'.value':123.456,'.priority':100}", "123.456"},
        {"{'.value':'string','.priority':100}", "'string'"},
        {"{'.value':true,'.priority':100}", "true"},
        {"{'.value':false,'.priority':100}", "false"},
        {"{'.value':[1,2,3],'.priority':100}", "[1,2,3]"},
        {"{'A':1,'B':'b','C':true,'.priority':100}",
         "{'A':1,'B':'b','C':true,'.priority':100}"},
        {"{'A':1,'B':{'.value':'b','.priority':100},'C':true,'.priority':100}",
         "{'A':1,'B':{'.value':'b','.priority':100},'C':true,'.priority':100}"},
    };

    for (auto& test : test_cases) {
      // Replace all \' to \" to match Json format.  Used \' for readability
      std::replace(test.first.begin(), test.first.end(), '\'', '\"');
      std::replace(test.second.begin(), test.second.end(), '\'', '\"');

      Variant original_variant = util::JsonToVariant(test.first.c_str());
      Variant expected = util::JsonToVariant(test.second.c_str());

      const Variant* value_ptr = GetVariantValue(&original_variant);
      const Variant& priority = GetVariantPriority(original_variant);

      EXPECT_TRUE(value_ptr != nullptr);
      switch (value_ptr->type()) {
        case Variant::kTypeNull:
        case Variant::kTypeMap:
          EXPECT_EQ(value_ptr, &original_variant);
          break;
        default:
          EXPECT_EQ(value_ptr, &original_variant.map()[".value"]);
          break;
      }
      EXPECT_EQ(*value_ptr, expected);

      EXPECT_EQ(priority, original_variant.map()[".priority"]);
      EXPECT_EQ(priority, Variant::FromInt64(100));
    }
  }
}

TEST(UtilDesktopTest, CombineValueAndPriority) {
  // Test with Null priority
  {
    Variant priority = Variant::Null();
    // Pairs of value and expected result.
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"", ""},  // Variant::Null()
        {"123", "123"},
        {"123.456", "123.456"},
        {"'string'", "'string'"},
        {"true", "true"},
        {"false", "false"},
        {"[1,2,3]", "[1,2,3]"},
        {"{'A':1,'B':'b','C':true}", "{'A':1,'B':'b','C':true}"},
        {"{'A':1,'B':{'.value':'b','.priority':100},'C':true}",
         "{'A':1,'B':{'.value':'b','.priority':100},'C':true}"},
    };

    for (auto& test : test_cases) {
      // Replace all \' to \" to match Json format.  Used \' for readability
      std::replace(test.first.begin(), test.first.end(), '\'', '\"');
      std::replace(test.second.begin(), test.second.end(), '\'', '\"');

      Variant value = util::JsonToVariant(test.first.c_str());
      Variant expected = util::JsonToVariant(test.second.c_str());
      EXPECT_THAT(CombineValueAndPriority(value, priority), Eq(expected));
    }
  }

  // Test with priority
  {
    Variant priority = Variant::FromInt64(100);
    // Pairs of value and expected result
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"", ""},  // Variant::Null()
        {"123", "{'.value':123,'.priority':100}"},
        {"123.456", "{'.value':123.456,'.priority':100}"},
        {"'string'", "{'.value':'string','.priority':100}"},
        {"true", "{'.value':true,'.priority':100}"},
        {"false", "{'.value':false,'.priority':100}"},
        {"[1,2,3]", "{'.value':[1,2,3],'.priority':100}"},
        {"{'A':1,'B':'b','C':true}",
         "{'A':1,'B':'b','C':true,'.priority':100}"},
        {"{'A':1,'B':{'.value':'b','.priority':100},'C':true}",
         "{'A':1,'B':{'.value':'b','.priority':100},'C':true,'.priority':100}"},
    };

    for (auto& test : test_cases) {
      // Replace all \' to \" to match Json format.  Used \' for readability
      std::replace(test.first.begin(), test.first.end(), '\'', '\"');
      std::replace(test.second.begin(), test.second.end(), '\'', '\"');

      Variant value = util::JsonToVariant(test.first.c_str());
      Variant expected = util::JsonToVariant(test.second.c_str());
      EXPECT_THAT(CombineValueAndPriority(value, priority), Eq(expected));
    }
  }
}

TEST(UtilDesktopTest, VariantIsLeaf) {
  // Pairs of value and expected result
  std::vector<std::pair<std::string, bool>> test_cases = {
      {"", true},
      {"123", true},
      {"123.456", true},
      {"'string'", true},
      {"true", true},
      {"false", true},
      {"[1,2,3]", false},
      {"{'A':1,'B':'b','C':true}", false},
      {"{'A':1,'B':{'.value':'b','.priority':100},'C':true}", false},
      {"{'.value':123,'.priority':100}", true},
      {"{'.value':123.456,'.priority':100}", true},
      {"{'.value':'string','.priority':100}", true},
      {"{'.value':true,'.priority':100}", true},
      {"{'.value':false,'.priority':100}", true},
      {"{'.value':[1,2,3],'.priority':100}", false},
      {"{'A':1,'B':'b','C':true,'.priority':100}", false},
      {"{'A':1,'B':{'.value':'b','.priority':100},'C':true,'.priority':100}",
       false},
  };

  for (auto& test : test_cases) {
    // Replace all \' to \" to match Json format.  Used \' for readability
    std::replace(test.first.begin(), test.first.end(), '\'', '\"');

    Variant original_variant = util::JsonToVariant(test.first.c_str());

    EXPECT_THAT(VariantIsLeaf(original_variant), test.second);
  }
}

TEST(UtilDesktopTest, VariantIsEmpty) {
  EXPECT_TRUE(VariantIsEmpty(Variant::Null()));
  EXPECT_TRUE(VariantIsEmpty(Variant::EmptyMap()));
  EXPECT_TRUE(VariantIsEmpty(Variant::EmptyVector()));

  EXPECT_FALSE(VariantIsEmpty(Variant::FromBool(false)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromBool(true)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromInt64(0)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromInt64(9999)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromDouble(0.0)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromDouble(1234.0)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromMutableString("")));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromMutableString("lorem ipsum")));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromStaticString("")));
  EXPECT_FALSE(VariantIsEmpty(
      Variant(std::map<Variant, Variant>{std::make_pair("test", 10)})));
  EXPECT_FALSE(VariantIsEmpty(Variant(std::vector<Variant>{1, 2, 3})));
  const char blob[] = {72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100};
  EXPECT_FALSE(VariantIsEmpty(Variant::FromMutableBlob(nullptr, 0)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromMutableBlob(blob, sizeof(blob))));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromStaticBlob(nullptr, 0)));
  EXPECT_FALSE(VariantIsEmpty(Variant::FromStaticBlob(blob, sizeof(blob))));
}

TEST(UtilDesktopTest, VariantsAreEquivalent) {
  // All of the regular comparisons should behave as expected.
  EXPECT_TRUE(VariantsAreEquivalent(Variant::Null(), Variant::Null()));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromBool(false),
                                    Variant::FromBool(false)));
  EXPECT_TRUE(
      VariantsAreEquivalent(Variant::FromBool(true), Variant::FromBool(true)));
  EXPECT_TRUE(
      VariantsAreEquivalent(Variant::FromInt64(100), Variant::FromInt64(100)));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromInt64(100),
                                    Variant::FromDouble(100.0f)));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromMutableString("Hi"),
                                    Variant::FromMutableString("Hi")));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromStaticString("Hi"),
                                    Variant::FromStaticString("Hi")));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromStaticString("Hi"),
                                    Variant::FromMutableString("Hi")));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromMutableString("Hi"),
                                    Variant::FromStaticString("Hi")));

  // Double to Int comparison should result in equal values despite different
  // types.
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromDouble(100.0f),
                                    Variant::FromInt64(100)));
  EXPECT_TRUE(VariantsAreEquivalent(Variant::FromInt64(100),
                                    Variant::FromDouble(100.0f)));

  EXPECT_FALSE(VariantsAreEquivalent(Variant::FromDouble(1000.0f),
                                     Variant::FromInt64(100)));
  EXPECT_FALSE(
      VariantsAreEquivalent(Variant::FromDouble(3.14f), Variant::FromInt64(3)));

  // Maps should recursively check if children are also equivlanet.
  Variant map_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  Variant equal_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  Variant equivalent_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100.0),
      std::make_pair("bbb", 200.0),
      std::make_pair("ccc", 300.0),
  };
  Variant priority_variant = std::map<Variant, Variant>{
      std::make_pair(".priority", 1),
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc", 300),
  };
  EXPECT_TRUE(VariantsAreEquivalent(map_variant, equal_variant));
  EXPECT_TRUE(VariantsAreEquivalent(map_variant, equivalent_variant));
  EXPECT_FALSE(VariantsAreEquivalent(map_variant, priority_variant));

  // Strings are not the same as ints to the database
  Variant bad_string_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", "100"),
      std::make_pair("bbb", "200"),
      std::make_pair("ccc", "300"),
  };
  // Variants that have too many elements should not compare equal, even if
  // the elements they share are the same.
  Variant too_long_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", "100"),
      std::make_pair("bbb", "200"),
      std::make_pair("ccc", "300"),
      std::make_pair("ddd", "400"),
  };
  EXPECT_FALSE(VariantsAreEquivalent(map_variant, bad_string_variant));
  EXPECT_FALSE(VariantsAreEquivalent(map_variant, too_long_variant));

  // Same rules should apply to nested variants.
  Variant nested_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
  };
  Variant equal_nested_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 300),
                         std::make_pair("eee", 400),
                     }),
  };
  Variant equivalent_nested_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 300.0),
                         std::make_pair("eee", 400.0),
                     }),
  };

  EXPECT_TRUE(VariantsAreEquivalent(nested_variant, equal_nested_variant));
  EXPECT_TRUE(VariantsAreEquivalent(nested_variant, equivalent_nested_variant));

  Variant bad_nested_variant = std::map<Variant, Variant>{
      std::make_pair("aaa", 100),
      std::make_pair("bbb", 200),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 300.0),
                         std::make_pair("eee", 400.0),
                         std::make_pair("fff", 500.0),
                     }),
  };
  EXPECT_FALSE(VariantsAreEquivalent(nested_variant, bad_nested_variant));
}

TEST(UtilDesktopTest, GetBase64SHA1) {
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"", "2jmj7l5rSw0yVb/vlWAYkK/YBwk="},
      {"i", "BC3EUS+j05HFFwzzqmHmpjj4Q0I="},
      {"ii", "ORg3PPVVnFS1LHBmQo9sQRjTHCM="},
      {"iii", "Ql/8FCLcTzJSi9n9WvNV/bXJYZI="},
      {"iiii", "MFMcKIXOYbOF3IHSo3X2vvgGB9U="},
      {"αβγωΑΒΓΩ", "WtUIYTivR0gge33nOEyQiBZGkmM="},
  };

  std::string encoded;
  for (auto& test : test_cases) {
    EXPECT_THAT(GetBase64SHA1(test.first, &encoded), Eq(test.second));
  }
}

TEST(UtilDesktopTest, ChildKeyCompareTo) {
  // Expect left is equal to right
  EXPECT_EQ(ChildKeyCompareTo(Variant("0"), Variant("0")), 0);
  EXPECT_EQ(ChildKeyCompareTo(Variant("1"), Variant("1")), 0);
  EXPECT_EQ(ChildKeyCompareTo(Variant("10"), Variant("10")), 0);
  EXPECT_EQ(ChildKeyCompareTo(Variant("A"), Variant("A")), 0);
  EXPECT_EQ(ChildKeyCompareTo(Variant("1A"), Variant("1A")), 0);
  EXPECT_EQ(ChildKeyCompareTo(Variant("[MIN_KEY]"), Variant("[MIN_KEY]")), 0);
  EXPECT_EQ(ChildKeyCompareTo(Variant("[MAX_KEY]"), Variant("[MAX_KEY]")), 0);

  // Expect left is greater than right
  EXPECT_GT(ChildKeyCompareTo(Variant("1"), Variant("0")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("0"), Variant("-1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("1"), Variant("-1")), 0);
  // "001" is equivalant to "1" in int value
  EXPECT_GT(ChildKeyCompareTo(Variant("001"), Variant("-1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("1"), Variant("-001")), 0);
  // "001" is equivalant to "1" in int value but has longer length as a string
  EXPECT_GT(ChildKeyCompareTo(Variant("001"), Variant("1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("-001"), Variant("-1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("001"), Variant("-001")), 0);
  // String is always greater than int
  EXPECT_GT(ChildKeyCompareTo(Variant("A"), Variant("1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("1A"), Variant("10")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("-1A"), Variant("10")), 0);
  // "-" is a string
  EXPECT_GT(ChildKeyCompareTo(Variant("-"), Variant("10")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("-"), Variant("-1")), 0);
  // "1.1" is not an int, therefore treated as a string
  EXPECT_GT(ChildKeyCompareTo(Variant("1.1"), Variant("10")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("1.1"), Variant("0")), 0);
  // Floating point is treated as string for comparison.
  EXPECT_GT(ChildKeyCompareTo(Variant("11.1"), Variant("1.1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("1.1"), Variant("-1.1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("-11.1"), Variant("-1.1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("A"), Variant("1.1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("A1"), Variant("A")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("A2"), Variant("A1")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("AA"), Variant("A")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("AA"), Variant("A1")), 0);
  // "[MIN_KEY]" is less than anything
  EXPECT_GT(ChildKeyCompareTo(Variant("0"), Variant("[MIN_KEY]")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("-100000"), Variant("[MIN_KEY]")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("100000"), Variant("[MIN_KEY]")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("A"), Variant("[MIN_KEY]")), 0);
  // "[MAX_KEY]" is greater than anything
  EXPECT_GT(ChildKeyCompareTo(Variant("[MAX_KEY]"), Variant("0")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("[MAX_KEY]"), Variant("1000000")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("[MAX_KEY]"), Variant("-1000000")), 0);
  EXPECT_GT(ChildKeyCompareTo(Variant("[MAX_KEY]"), Variant("A")), 0);

  // Expect left is less than right
  EXPECT_LT(ChildKeyCompareTo(Variant("0"), Variant("1")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-1"), Variant("0")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-1"), Variant("1")), 0);
  // "001" is equivalant to "1" in int value
  EXPECT_LT(ChildKeyCompareTo(Variant("-1"), Variant("001")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-001"), Variant("1")), 0);
  // "001" is equivalant to "1" in int value but has longer length as a string
  EXPECT_LT(ChildKeyCompareTo(Variant("1"), Variant("001")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-1"), Variant("-001")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-001"), Variant("001")), 0);
  // String is always greater than int
  EXPECT_LT(ChildKeyCompareTo(Variant("1"), Variant("A")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("10"), Variant("1A")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("10"), Variant("-1A")), 0);
  // "-" is a string
  EXPECT_LT(ChildKeyCompareTo(Variant("10"), Variant("-")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-1"), Variant("-")), 0);
  // "1.1" is not an int, therefore treated as a string
  EXPECT_LT(ChildKeyCompareTo(Variant("10"), Variant("1.1")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("0"), Variant("1.1")), 0);
  // Floating point is treated as string for comparison.
  EXPECT_LT(ChildKeyCompareTo(Variant("1.1"), Variant("11.1")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-1.1"), Variant("1.1")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-1.1"), Variant("-11.1")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("1.1"), Variant("A")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("A"), Variant("A1")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("A1"), Variant("A2")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("A"), Variant("AA")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("A1"), Variant("AA")), 0);
  // "[MIN_KEY]" is less than anything
  EXPECT_LT(ChildKeyCompareTo(Variant("[MIN_KEY]"), Variant("0")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("[MIN_KEY]"), Variant("-100000")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("[MIN_KEY]"), Variant("100000")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("[MIN_KEY]"), Variant("A")), 0);
  // "[MAX_KEY]" is greater than anything
  EXPECT_LT(ChildKeyCompareTo(Variant("0"), Variant("[MAX_KEY]")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("100000"), Variant("[MAX_KEY]")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("-100000"), Variant("[MAX_KEY]")), 0);
  EXPECT_LT(ChildKeyCompareTo(Variant("A"), Variant("[MAX_KEY]")), 0);
}

TEST(UtilDesktopTest, GetHashRepresentation) {
  std::vector<std::pair<Variant, std::string>> test_cases = {
      // Null
      {Variant::Null(), ""},
      // Int64
      {Variant(0), "number:0000000000000000"},
      {Variant(1), "number:3ff0000000000000"},
      {Variant::FromInt64(INT64_MIN), "number:c3e0000000000000"},
      // Double
      {Variant(0.1), "number:3fb999999999999a"},
      {Variant(1.2345678901234567), "number:3ff3c0ca428c59fb"},
      {Variant(12345.678901234567), "number:40c81cd6e63c53d7"},
      {Variant(1234567890123456.5), "number:43118b54f22aeb02"},
      // Boolean
      {Variant(true), "boolean:true"},
      {Variant(false), "boolean:false"},
      // String
      {Variant("i"), "string:i"},
      {Variant("ii"), "string:ii"},
      {Variant("iii"), "string:iii"},
      {Variant("iiii"), "string:iiii"},
      // UTF-8 String
      {Variant("αβγωΑΒΓΩ"), "string:αβγωΑΒΓΩ"},
      // Basic Map
      {util::JsonToVariant("{\"B2\":2,\"B1\":1}"),
       ":B1:YPVfR2bXt/lcDjiQZ8pOkAd3qkQ=:B2:WtSt2Xo3L0JtPuArzQHofPrZOuU="},
      // Map with priority
      {util::JsonToVariant(
           "{\"B1\":{\".value\":1,\".priority\":2.0},\"B2\":{\".value\":2,"
           "\".priority\":1.0},\"B3\":3}"),
       ":B3:3tYODYzGXwaGnXNech4jb4T9las=:B2:iiz9CIvYWkKdETTpjVFBJNx1SiI="
       ":B1:FvGzv2x5RbRTIc6uhMwY3pMW2oU="},
      // Array
      {util::JsonToVariant("[1, 2, 3]"),
       ":0:YPVfR2bXt/lcDjiQZ8pOkAd3qkQ=:1:WtSt2Xo3L0JtPuArzQHofPrZOuU="
       ":2:3tYODYzGXwaGnXNech4jb4T9las="},
      // Map in representation of an array
      {util::JsonToVariant("{\"0\":1, \"1\":2, \"2\":3}"),
       ":0:YPVfR2bXt/lcDjiQZ8pOkAd3qkQ=:1:WtSt2Xo3L0JtPuArzQHofPrZOuU="
       ":2:3tYODYzGXwaGnXNech4jb4T9las="},
      // Array more than 10 elements
      {util::JsonToVariant("[7, 2, 3, 9, 5, 6, 1, 4, 8, 10, 11]"),
       ":0:7wQgMram7RVqVIg/xRZWPfygGx0=:1:WtSt2Xo3L0JtPuArzQHofPrZOuU="
       ":2:3tYODYzGXwaGnXNech4jb4T9las=:3:M7Kyw8zsPkNHRw35uJ1vdPacr90="
       ":4:w28swksk9+tXf5jEdS9R5oSFAv8=:5:qb1N9GrUXfC3JyZPF8EXiNYcv4I="
       ":6:YPVfR2bXt/lcDjiQZ8pOkAd3qkQ=:7:eVih19a6ZDz3NL32uVBtg9KSgQY="
       ":8:pITK737CVleu2Q4bHJTdQ4dJnCg=:9:+r5aI9HvKKagELki8SYKBk0q7D4="
       ":10:+aUUrIPmWZcSiV4ocCSLYRSFawE="},
      // Map in representation of an array more than 10 elements
      {util::JsonToVariant(
           "{\"0\":7, \"1\":2, \"2\":3, \"3\":9, \"4\":5, \"5\":6, \"6\":1, "
           "\"7\":4, \"8\":8, \"9\":10, \"10\":11}"),
       ":0:7wQgMram7RVqVIg/xRZWPfygGx0=:1:WtSt2Xo3L0JtPuArzQHofPrZOuU="
       ":2:3tYODYzGXwaGnXNech4jb4T9las=:3:M7Kyw8zsPkNHRw35uJ1vdPacr90="
       ":4:w28swksk9+tXf5jEdS9R5oSFAv8=:5:qb1N9GrUXfC3JyZPF8EXiNYcv4I="
       ":6:YPVfR2bXt/lcDjiQZ8pOkAd3qkQ=:7:eVih19a6ZDz3NL32uVBtg9KSgQY="
       ":8:pITK737CVleu2Q4bHJTdQ4dJnCg=:9:+r5aI9HvKKagELki8SYKBk0q7D4="
       ":10:+aUUrIPmWZcSiV4ocCSLYRSFawE="},
      // Array with priority of different types
      {util::JsonToVariant(
           "[1,{\".value\":2,\".priority\":\"1\"},{\".value\":3,\".priority\":"
           "1.1},{\".value\":4,\".priority\":1}]"),
       ":0:YPVfR2bXt/lcDjiQZ8pOkAd3qkQ=:3:MTfbusV7VkrLc1KUkR7t8903AO0="
       ":2:McRf84Bik6f4pUV86mpvDCk7CIY=:1:xJPtZCG4C1Z2dsXLdmD4nuEeJWg="},
      // Map with mixed numeric and alphanumeric keys
      {util::JsonToVariant("{\"1\":10, \"01\":7, \"001\":8, \"10\":20, "
                           "\"11\":29, \"12\":25, \"A\":15}"),
       ":1:+r5aI9HvKKagELki8SYKBk0q7D4=:01:7wQgMram7RVqVIg/xRZWPfygGx0="
       ":001:pITK737CVleu2Q4bHJTdQ4dJnCg=:10:KAU+hDgZHcHeW8Ejndss7NJXOts="
       ":11:6+iMnJRA9k8I9jMianUFkJUZ2as=:12:EBgCJ72ufYyBZo/vQcusywSQr0k="
       ":A:o0Z01FiFkcaCNvXrl/rO9/d+zjk="},
      // LeafNode with priority
      {util::JsonToVariant("{\".value\":2,\".priority\":1.0}"),
       "priority:number:3ff0000000000000:number:4000000000000000"},
      // Map with priority
      {util::JsonToVariant("{\".priority\":2.0,\"A\":2}"),
       "priority:number:4000000000000000::A:WtSt2Xo3L0JtPuArzQHofPrZOuU="},
      // Nested priority
      {util::JsonToVariant(
           "{\".priority\":3.0,\"A\":{\".value\":2,\".priority\":1.0}}"),
       "priority:number:4008000000000000::A:iiz9CIvYWkKdETTpjVFBJNx1SiI="},
  };

  std::string hash_rep;
  for (const auto& test : test_cases) {
    EXPECT_THAT(GetHashRepresentation(test.first, &hash_rep), Eq(test.second));
  }
}

TEST(UtilDesktopTest, GetHash) {
  std::vector<std::pair<Variant, std::string>> test_cases = {
      // Null
      {Variant::Null(), ""},
      // Int64
      {Variant(0), "7ysMph9WPitGP7poMnMHMVPtUlI="},
      {Variant(1), "YPVfR2bXt/lcDjiQZ8pOkAd3qkQ="},
      {Variant::FromInt64(INT64_MIN), "t8Zsu6QlM7Q4staTHVsgiTYxyUs="},
      // Double
      {Variant(0.1), "wtQjBi5TBE+ZcdekL6INiSeCSQI="},
      {Variant(1.2345678901234567), "xy9cBNnU0nPSZZ/ZhBUrD5JZHqI="},
      {Variant(12345.678901234567), "dY5swb32BtBwcxLG0QSzKrxF4Ek="},
      {Variant(1234567890123456.5), "TnvxroHDDUski72FbjG9s1opR2U="},
      // Boolean
      {Variant(true), "E5z61QM0lN/U2WsOnusszCTkR8M="},
      {Variant(false), "aSSNoqcS4oQwJ2xxH20rvpp3zP0="},
      // String
      {Variant("i"), "DeH+bYeyNKPWpoASovNpeBOhCLU="},
      {Variant("ii"), "bzF9bn9qYLhJmuc33tDqMMVtgkY="},
      {Variant("iii"), "vHKAStiyuxaQKEElU3MxAxJ+Pjk="},
      {Variant("iiii"), "vX9ogm9I6wB/x0t3LY9jfsgwRhs="},
      // UTF-8 String
      {Variant("αβγωΑΒΓΩ"), "7VgSkcL0RRqd5MecDe/uvdDP/LM="},
      // Basic Map
      {util::JsonToVariant("{\"B2\":2,\"B1\":1}"),
       "saXm0YMzvotwh2WvsZFatveeAZk="},
      // Map with priority
      {util::JsonToVariant(
           "{\"B1\":{\".value\":1,\".priority\":2.0},\"B2\":{\".value\":2,"
           "\".priority\":1.0},\"B3\":3}"),
       "9q4+gOobE1ozTZyb85m/iDxoYzY="},
      // Array
      {util::JsonToVariant("[1, 2, 3]"), "h6XOC3OcidJlNC1Velmi3gphgQk="},
      // Map in representation of an array.
      {util::JsonToVariant("{\"0\":1, \"1\":2, \"2\":3}"),
       "h6XOC3OcidJlNC1Velmi3gphgQk="},
      // Array more than 10 elements
      {util::JsonToVariant("[7, 2, 3, 9, 5, 6, 1, 4, 8, 10, 11]"),
       "0iPsE+86XkEMyhTUqK19iX0O+/E="},
      // Map in representation of an array more than 10 elements
      {util::JsonToVariant(
           "{\"0\":7, \"1\":2, \"2\":3, \"3\":9, \"4\":5, \"5\":6, \"6\":1, "
           "\"7\":4, \"8\":8, \"9\":10, \"10\":11}"),
       "0iPsE+86XkEMyhTUqK19iX0O+/E="},
      // Array with priority of different types
      {util::JsonToVariant(
           "[1,{\".value\":2,\".priority\":\"1\"},{\".value\":3,\".priority\":"
           "1.1},{\".value\":4,\".priority\":1}]"),
       "PfCbiYP2e75wAxeBx078Rpag/as="},
      // Map with mixed numeric and alphanumeric keys
      {util::JsonToVariant("{\"1\":10, \"01\":7, \"001\":8, \"10\":20, "
                           "\"11\":29, \"12\":25, \"A\":15}"),
       "fYENO1aD55oc6I6f+FM+cv1Y1yc="},
      // LeafNode with priority
      {util::JsonToVariant("{\".value\":2,\".priority\":1.0}"),
       "iiz9CIvYWkKdETTpjVFBJNx1SiI="},
      // Map with priority
      {util::JsonToVariant("{\".priority\":2.0,\"A\":2}"),
       "1xHri2Z3/K1NzjMObwiYwEfgo18="},
      // Nested priority
      {util::JsonToVariant(
           "{\".priority\":3.0,\"A\":{\".value\":2,\".priority\":1.0}}"),
       "YpFTODg262pl4OnB8L9w0QdeZpM="},
  };

  std::string hash;
  for (const auto& test : test_cases) {
    EXPECT_THAT(GetHash(test.first, &hash), Eq(test.second));
  }
}

TEST(UtilDesktopTest, QuerySpecLoadsAllData) {
  QuerySpec spec_default;
  EXPECT_TRUE(QuerySpecLoadsAllData(spec_default));

  QuerySpec spec_order_by_key;
  spec_order_by_key.params.order_by = QueryParams::kOrderByKey;
  EXPECT_TRUE(QuerySpecLoadsAllData(spec_order_by_key));

  QuerySpec spec_order_by_value;
  spec_order_by_value.params.order_by = QueryParams::kOrderByValue;
  EXPECT_TRUE(QuerySpecLoadsAllData(spec_order_by_value));

  QuerySpec spec_order_by_child;
  spec_order_by_child.params.order_by = QueryParams::kOrderByChild;
  spec_order_by_child.params.order_by_child = "baby_mario";
  EXPECT_TRUE(QuerySpecLoadsAllData(spec_order_by_child));

  QuerySpec spec_start_at_value;
  spec_start_at_value.params.start_at_value = 0;
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_start_at_value));

  QuerySpec spec_start_at_child_key;
  spec_start_at_child_key.params.start_at_child_key = "a";
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_start_at_child_key));

  QuerySpec spec_end_at_value;
  spec_end_at_value.params.end_at_value = 9999;
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_end_at_value));

  QuerySpec spec_end_at_child_key;
  spec_end_at_child_key.params.end_at_child_key = "z";
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_end_at_child_key));

  QuerySpec spec_equal_to_value;
  spec_equal_to_value.params.equal_to_value = 5000;
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_equal_to_value));

  QuerySpec spec_equal_to_child_key;
  spec_equal_to_child_key.params.equal_to_child_key = "mn";
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_equal_to_child_key));

  QuerySpec spec_limit_first;
  spec_limit_first.params.limit_first = 10;
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_limit_first));

  QuerySpec spec_limit_last;
  spec_limit_last.params.limit_last = 20;
  EXPECT_FALSE(QuerySpecLoadsAllData(spec_limit_last));
}

TEST(UtilDesktopTest, QuerySpecIsDefault) {
  QuerySpec spec_default;
  EXPECT_TRUE(QuerySpecIsDefault(spec_default));

  QuerySpec spec_order_by_key;
  spec_order_by_key.params.order_by = QueryParams::kOrderByKey;
  EXPECT_FALSE(QuerySpecIsDefault(spec_order_by_key));

  QuerySpec spec_order_by_value;
  spec_order_by_value.params.order_by = QueryParams::kOrderByValue;
  EXPECT_FALSE(QuerySpecIsDefault(spec_order_by_value));

  QuerySpec spec_order_by_child;
  spec_order_by_child.params.order_by = QueryParams::kOrderByChild;
  spec_order_by_child.params.order_by_child = "baby_mario";
  EXPECT_FALSE(QuerySpecIsDefault(spec_order_by_child));

  QuerySpec spec_start_at_value;
  spec_start_at_value.params.start_at_value = 0;
  EXPECT_FALSE(QuerySpecIsDefault(spec_start_at_value));

  QuerySpec spec_start_at_child_key;
  spec_start_at_child_key.params.start_at_child_key = "a";
  EXPECT_FALSE(QuerySpecIsDefault(spec_start_at_child_key));

  QuerySpec spec_end_at_value;
  spec_end_at_value.params.end_at_value = 9999;
  EXPECT_FALSE(QuerySpecIsDefault(spec_end_at_value));

  QuerySpec spec_end_at_child_key;
  spec_end_at_child_key.params.end_at_child_key = "z";
  EXPECT_FALSE(QuerySpecIsDefault(spec_end_at_child_key));

  QuerySpec spec_equal_to_value;
  spec_equal_to_value.params.equal_to_value = 5000;
  EXPECT_FALSE(QuerySpecIsDefault(spec_equal_to_value));

  QuerySpec spec_equal_to_child_key;
  spec_equal_to_child_key.params.equal_to_child_key = "mn";
  EXPECT_FALSE(QuerySpecIsDefault(spec_equal_to_child_key));

  QuerySpec spec_limit_first;
  spec_limit_first.params.limit_first = 10;
  EXPECT_FALSE(QuerySpecIsDefault(spec_limit_first));

  QuerySpec spec_limit_last;
  spec_limit_last.params.limit_last = 20;
  EXPECT_FALSE(QuerySpecIsDefault(spec_limit_last));
}

TEST(UtilDesktopTest, MakeDefaultQuerySpec) {
  QuerySpec spec_default;
  spec_default.path = Path("this/value/should/not/change");
  QuerySpec default_result = MakeDefaultQuerySpec(spec_default);
  EXPECT_TRUE(QuerySpecIsDefault(default_result));
  EXPECT_EQ(default_result, spec_default);

  QuerySpec spec_featureful;
  spec_featureful.path = Path("this/value/should/not/change");
  spec_featureful.params.order_by = QueryParams::kOrderByChild;
  spec_featureful.params.order_by_child = "baby_mario";
  spec_featureful.params.start_at_value = 0;
  spec_featureful.params.start_at_child_key = "a";
  spec_featureful.params.end_at_value = 9999;
  spec_featureful.params.end_at_child_key = "z";
  spec_featureful.params.limit_first = 10;
  spec_featureful.params.limit_last = 20;
  QuerySpec featureful_result = MakeDefaultQuerySpec(spec_featureful);
  EXPECT_TRUE(QuerySpecIsDefault(featureful_result));
  EXPECT_EQ(featureful_result, spec_default);
}

TEST(UtilDesktopTest, WireProtocolPathToString) {
  EXPECT_EQ(WireProtocolPathToString(Path()), "/");
  EXPECT_EQ(WireProtocolPathToString(Path("")), "/");
  EXPECT_EQ(WireProtocolPathToString(Path("/")), "/");
  EXPECT_EQ(WireProtocolPathToString(Path("///")), "/");

  EXPECT_EQ(WireProtocolPathToString(Path("A")), "A");
  EXPECT_EQ(WireProtocolPathToString(Path("/A")), "A");
  EXPECT_EQ(WireProtocolPathToString(Path("A/")), "A");
  EXPECT_EQ(WireProtocolPathToString(Path("/A/")), "A");

  EXPECT_EQ(WireProtocolPathToString(Path("A/B")), "A/B");
  EXPECT_EQ(WireProtocolPathToString(Path("/A/B")), "A/B");
  EXPECT_EQ(WireProtocolPathToString(Path("A/B/")), "A/B");
  EXPECT_EQ(WireProtocolPathToString(Path("/A/B/")), "A/B");
}

TEST(UtilDesktopTest, GetWireProtocolParams) {
  {
    QueryParams params_default;
    EXPECT_EQ(GetWireProtocolParams(params_default), Variant::EmptyMap());
  }

  {
    QueryParams params;
    params.start_at_value = "0";

    Variant expected(std::map<Variant, Variant>{
        {"sp", "0"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.start_at_value = 0;
    params.start_at_child_key = "0010";

    Variant expected(std::map<Variant, Variant>{
        {"sp", 0},
        {"sn", "0010"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.end_at_value = "0";

    Variant expected(std::map<Variant, Variant>{
        {"ep", "0"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.end_at_value = 0;
    params.end_at_child_key = "0010";

    Variant expected(std::map<Variant, Variant>{
        {"ep", 0},
        {"en", "0010"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.equal_to_value = 3.14;

    Variant expected(std::map<Variant, Variant>{
        {"sp", 3.14},
        {"ep", 3.14},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.equal_to_value = 3.14;
    params.equal_to_child_key = "A";

    Variant expected(std::map<Variant, Variant>{
        {"sp", 3.14},
        {"sn", "A"},
        {"ep", 3.14},
        {"en", "A"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.limit_first = 10;

    Variant expected(std::map<Variant, Variant>{
        {"l", 10},
        {"vf", "l"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.limit_last = 20;

    Variant expected(std::map<Variant, Variant>{
        {"l", 20},
        {"vf", "r"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByKey;
    params.start_at_value = "A";

    Variant expected(std::map<Variant, Variant>{
        {"i", ".key"},
        {"sp", "A"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByValue;
    params.end_at_value = "Z";

    Variant expected(std::map<Variant, Variant>{
        {"i", ".value"},
        {"ep", "Z"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.order_by_child = "";
    params.limit_first = 10;

    Variant expected(std::map<Variant, Variant>{
        {"i", "/"},
        {"l", 10},
        {"vf", "l"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }

  {
    QueryParams params;
    params.order_by = QueryParams::kOrderByChild;
    params.order_by_child = "/A/B/C/";
    params.limit_last = 20;

    Variant expected(std::map<Variant, Variant>{
        {"i", "A/B/C"},
        {"l", 20},
        {"vf", "r"},
    });

    EXPECT_EQ(GetWireProtocolParams(params), expected);
  }
}

TEST(UtilDesktopTest, TestGetAppDataPath) {
  // Make sure we get a path string.
  EXPECT_NE(AppDataDir("testapp0"), "");

  // Make sure we get 2 different paths for 2 different apps.
  EXPECT_NE(AppDataDir("testapp1"), AppDataDir("testapp2"));

  // Make sure we get the same path if we are calling twice with the same app.
  EXPECT_EQ(AppDataDir("testapp3"), AppDataDir("testapp3"));

  // Make sure the path string refers to a directory that is available.
  // Testing app paths with and without subdirectory.
  std::vector<std::string> test_app_paths = {"testapp4",
                                             "testproject/testapp4"};
  // Make sure the path string refers to a directory that is available.
  // app_name can also have path separators in them.
  for (const std::string& test_app_path : test_app_paths) {
    std::string path = AppDataDir(test_app_path.c_str(), true);
    struct stat s;
    ASSERT_EQ(stat(path.c_str(), &s), 0)
        << "stat failed on '" << path << "': " << strerror(errno);
    EXPECT_TRUE(s.st_mode & S_IFDIR) << path << " is not a directory!";

    // Write random data to a randomly generated filename.
    std::string test_data =
        std::string("Hello, world! ") + std::to_string(rand());  // NOLINT
    std::string test_path = path + kPathSep + "test_file_" +
                            std::to_string(rand()) + ".txt";  // NOLINT

    // Ensure that we can save files in this directory.
    FILE* out = fopen(test_path.c_str(), "w");
    EXPECT_NE(out, nullptr)
        << "Couldn't open test file for writing: " << strerror(errno);
    EXPECT_GE(fputs(test_data.c_str(), out), 0) << strerror(errno);
    EXPECT_EQ(fclose(out), 0) << strerror(errno);

    FILE* in = fopen(test_path.c_str(), "r");
    EXPECT_NE(in, nullptr) << "Couldn't open test file for reading: "
                           << strerror(errno);
    char buf[256];
    EXPECT_NE(fgets(buf, sizeof(buf), in), nullptr) << strerror(errno);
    EXPECT_STREQ(buf, test_data.c_str());
    EXPECT_EQ(fclose(in), 0) << strerror(errno);

    // Delete the file.
    EXPECT_EQ(unlink(test_path.c_str()), 0) << strerror(errno);
  }
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
