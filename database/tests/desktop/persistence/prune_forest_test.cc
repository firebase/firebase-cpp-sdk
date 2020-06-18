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

#include "database/src/desktop/persistence/prune_forest.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(PruneForestTest, Equality) {
  PruneForest forest;
  forest.SetValueAt(Path("true"), true);
  forest.SetValueAt(Path("false"), false);

  PruneForest identical_forest;
  identical_forest.SetValueAt(Path("true"), true);
  identical_forest.SetValueAt(Path("false"), false);

  PruneForest different_forest;
  different_forest.SetValueAt(Path("true"), false);
  different_forest.SetValueAt(Path("false"), true);

  PruneForestRef ref(&forest);
  PruneForestRef same_ref(&forest);
  PruneForestRef identical_ref(&identical_forest);
  PruneForestRef different_ref(&different_forest);
  PruneForestRef null_ref(nullptr);
  PruneForestRef another_null_ref(nullptr);

  EXPECT_EQ(ref, ref);
  EXPECT_EQ(ref, same_ref);
  EXPECT_EQ(ref, identical_ref);
  EXPECT_NE(ref, different_ref);
  EXPECT_NE(ref, null_ref);

  EXPECT_EQ(null_ref, null_ref);
  EXPECT_EQ(null_ref, another_null_ref);
  EXPECT_EQ(another_null_ref, null_ref);
}

TEST(PruneForestTest, PrunesAnything) {
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    EXPECT_FALSE(ref.PrunesAnything());
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Prune(Path("foo"));
    EXPECT_TRUE(ref.PrunesAnything());
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Prune(Path("foo/bar/baz"));
    EXPECT_TRUE(ref.PrunesAnything());
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Keep(Path("foo"));
    EXPECT_FALSE(ref.PrunesAnything());
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Keep(Path("foo/bar/baz"));
    EXPECT_FALSE(ref.PrunesAnything());
  }
}

TEST(PruneForestTest, ShouldPruneUnkeptDescendants) {
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    EXPECT_FALSE(ref.ShouldPruneUnkeptDescendants(Path()));
    EXPECT_FALSE(ref.ShouldPruneUnkeptDescendants(Path("aaa")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    forest.SetValueAt(Path("aaa"), true);
    forest.SetValueAt(Path("bbb"), false);

    EXPECT_FALSE(ref.ShouldPruneUnkeptDescendants(Path()));
    EXPECT_TRUE(ref.ShouldPruneUnkeptDescendants(Path("aaa")));
    EXPECT_FALSE(ref.ShouldPruneUnkeptDescendants(Path("bbb")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    forest.SetValueAt(Path("aaa"), false);
    forest.SetValueAt(Path("aaa/bbb"), true);
    forest.SetValueAt(Path("aaa/bbb/ccc"), false);

    EXPECT_FALSE(ref.ShouldPruneUnkeptDescendants(Path("aaa")));
    EXPECT_TRUE(ref.ShouldPruneUnkeptDescendants(Path("aaa/bbb")));
    EXPECT_FALSE(ref.ShouldPruneUnkeptDescendants(Path("aaa/bbb/ccc")));
  }
}

TEST(PruneForestTest, ShouldKeep) {
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    EXPECT_FALSE(ref.ShouldKeep(Path()));
    EXPECT_FALSE(ref.ShouldKeep(Path("aaa")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    forest.SetValueAt(Path("aaa"), true);
    forest.SetValueAt(Path("bbb"), false);

    EXPECT_FALSE(ref.ShouldKeep(Path()));
    EXPECT_FALSE(ref.ShouldKeep(Path("aaa")));
    EXPECT_TRUE(ref.ShouldKeep(Path("bbb")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    forest.SetValueAt(Path("aaa"), true);
    forest.SetValueAt(Path("aaa/bbb"), false);

    EXPECT_FALSE(ref.ShouldKeep(Path("aaa")));
    EXPECT_TRUE(ref.ShouldKeep(Path("aaa/bbb")));
  }
}

TEST(PruneForestTest, AffectsPath) {
  {
    PruneForest forest;
    PruneForestRef ref(&forest);

    EXPECT_FALSE(ref.AffectsPath(Path()));
    EXPECT_FALSE(ref.AffectsPath(Path("foo")));
    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar")));

    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/baz")));
    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/baz/quux")));

    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/buzz")));
    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/buzz/quuz")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Prune(Path("foo"));

    EXPECT_TRUE(ref.AffectsPath(Path()));
    EXPECT_TRUE(ref.AffectsPath(Path("foo")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz/quux")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/buzz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/buzz/quuz")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Prune(Path("foo/bar/baz"));

    EXPECT_TRUE(ref.AffectsPath(Path()));
    EXPECT_TRUE(ref.AffectsPath(Path("foo")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz/quux")));

    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/buzz")));
    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/buzz/quuz")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Keep(Path("foo"));
    EXPECT_TRUE(ref.AffectsPath(Path()));
    EXPECT_TRUE(ref.AffectsPath(Path("foo")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz/quux")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/buzz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/buzz/quuz")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Keep(Path("foo/bar/baz"));
    EXPECT_TRUE(ref.AffectsPath(Path()));
    EXPECT_TRUE(ref.AffectsPath(Path("foo")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz/quux")));

    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/buzz")));
    EXPECT_FALSE(ref.AffectsPath(Path("foo/bar/buzz/quuz")));
  }
  {
    PruneForest forest;
    PruneForestRef ref(&forest);
    ref.Prune(Path("foo"));
    ref.Keep(Path("foo/bar/baz"));
    EXPECT_TRUE(ref.AffectsPath(Path()));
    EXPECT_TRUE(ref.AffectsPath(Path("foo")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/baz/quux")));

    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/buzz")));
    EXPECT_TRUE(ref.AffectsPath(Path("foo/bar/buzz/quuz")));
  }
}

TEST(PruneForestTest, GetChild) {
  PruneForest forest;
  PruneForestRef ref(&forest);

  forest.SetValueAt(Path("aaa"), true);
  forest.SetValueAt(Path("aaa/bbb"), true);
  forest.SetValueAt(Path("aaa/bbb/ccc"), true);
  forest.SetValueAt(Path("zzz"), false);
  forest.SetValueAt(Path("zzz/yyy"), false);
  forest.SetValueAt(Path("zzz/yyy/xxx"), false);

  PruneForest* child_aaa = forest.GetChild(Path("aaa"));
  PruneForest* child_aaa_bbb = forest.GetChild(Path("aaa/bbb"));
  PruneForest* child_aaa_bbb_ccc = forest.GetChild(Path("aaa/bbb/ccc"));
  PruneForest* child_zzz = forest.GetChild(Path("zzz"));
  PruneForest* child_zzz_yyy = forest.GetChild(Path("zzz/yyy"));
  PruneForest* child_zzz_yyy_xxx = forest.GetChild(Path("zzz/yyy/xxx"));

  EXPECT_EQ(ref.GetChild(Path("aaa")), PruneForestRef(child_aaa));
  EXPECT_EQ(ref.GetChild(Path("aaa/bbb")), PruneForestRef(child_aaa_bbb));
  EXPECT_EQ(ref.GetChild(Path("aaa/bbb/ccc")),
            PruneForestRef(child_aaa_bbb_ccc));
  EXPECT_EQ(ref.GetChild(Path("zzz")), PruneForestRef(child_zzz));
  EXPECT_EQ(ref.GetChild(Path("zzz/yyy")), PruneForestRef(child_zzz_yyy));
  EXPECT_EQ(ref.GetChild(Path("zzz/yyy/xxx")),
            PruneForestRef(child_zzz_yyy_xxx));
}

TEST(PruneForestTest, Prune) {
  PruneForest forest;
  PruneForestRef ref(&forest);

  ref.Prune(Path("aaa/bbb/ccc"));
  EXPECT_EQ(forest.GetValueAt(Path()), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb")), nullptr);
  EXPECT_TRUE(*forest.GetValueAt(Path("aaa/bbb/ccc")));

  ref.Prune(Path("aaa/bbb"));
  EXPECT_EQ(forest.GetValueAt(Path()), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa")), nullptr);
  EXPECT_TRUE(*forest.GetValueAt(Path("aaa/bbb")));
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb/ccc")), nullptr);

  ref.Prune(Path("aaa"));
  EXPECT_EQ(forest.GetValueAt(Path()), nullptr);
  EXPECT_TRUE(*forest.GetValueAt(Path("aaa")));
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb/ccc")), nullptr);

  ref.Prune(Path());
  EXPECT_TRUE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb/ccc")), nullptr);

  ref.Prune(Path("zzz"));
  EXPECT_TRUE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("zzz")), nullptr);

  ref.Prune(Path("zzz/yyy"));
  EXPECT_TRUE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("zzz")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("zzz/yyy")), nullptr);

  ref.Prune(Path("zzz/yyy/xxx"));
  EXPECT_TRUE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("zzz")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("zzz/yyy")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("zzz/yyy/xxx")), nullptr);
}

TEST(PruneForestTest, Keep) {
  PruneForest forest;
  PruneForestRef ref(&forest);

  ref.Keep(Path("aaa/bbb/ccc"));
  EXPECT_EQ(forest.GetValueAt(Path()), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb")), nullptr);
  EXPECT_FALSE(*forest.GetValueAt(Path("aaa/bbb/ccc")));

  ref.Keep(Path("aaa/bbb"));
  EXPECT_EQ(forest.GetValueAt(Path()), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa")), nullptr);
  EXPECT_FALSE(*forest.GetValueAt(Path("aaa/bbb")));
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb/ccc")), nullptr);

  ref.Keep(Path("aaa"));
  EXPECT_EQ(forest.GetValueAt(Path()), nullptr);
  EXPECT_FALSE(*forest.GetValueAt(Path("aaa")));
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb/ccc")), nullptr);

  ref.Keep(Path());
  EXPECT_FALSE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("aaa")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("aaa/bbb/ccc")), nullptr);

  ref.Keep(Path("zzz"));
  EXPECT_FALSE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("zzz")), nullptr);

  ref.Keep(Path("zzz/yyy"));
  EXPECT_FALSE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("zzz")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("zzz/yyy")), nullptr);

  ref.Keep(Path("zzz/yyy/xxx"));
  EXPECT_FALSE(*forest.GetValueAt(Path()));
  EXPECT_EQ(forest.GetValueAt(Path("zzz")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("zzz/yyy")), nullptr);
  EXPECT_EQ(forest.GetValueAt(Path("zzz/yyy/xxx")), nullptr);
}

TEST(PruneForestTest, KeepAll) {
  // Set up a test case.
  PruneForest default_forest;
  default_forest.SetValueAt(Path("aaa/111"), true);
  default_forest.SetValueAt(Path("aaa/222"), false);

  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.KeepAll(Path("aaa"), std::set<std::string>({std::string("111")}));

    // Only 111 should be affected, and it should now be false.
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa/222")));
  }
  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.KeepAll(Path("aaa"), std::set<std::string>({std::string("222")}));

    // Only 222 should be affected, but it was already false so it should not
    // change.
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa/222")));
  }
  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.KeepAll(Path("aaa"), std::set<std::string>(
                                 {std::string("111"), std::string("222")}));

    // Both children should now be false.
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa/222")));
  }
  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.KeepAll(Path(), std::set<std::string>({std::string("aaa")}));

    // aaa should now be false, and all children of it should be eliminated.
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa")));

    // Children are now eliminated.
    EXPECT_EQ(forest.GetValueAt(Path("aaa/111")), nullptr);
    EXPECT_EQ(forest.GetValueAt(Path("aaa/222")), nullptr);
  }
}

TEST(PruneForestTest, PruneAll) {
  // Set up a test case.
  PruneForest default_forest;
  default_forest.SetValueAt(Path("aaa/111"), true);
  default_forest.SetValueAt(Path("aaa/222"), false);
  default_forest.SetValueAt(Path("bbb/111"), true);
  default_forest.SetValueAt(Path("bbb/222"), false);

  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.PruneAll(Path("aaa"), std::set<std::string>({std::string("111")}));

    // Only 111 should be affected, but it was already true so it should not
    // change.
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("aaa/222")));

    // Should remain untouched.
    EXPECT_TRUE(*forest.GetValueAt(Path("bbb/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("bbb/222")));
  }
  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.PruneAll(Path("aaa"), std::set<std::string>({std::string("222")}));

    // Only 222 should be affected, and it should now be true
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa/111")));
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa/222")));

    // Should remain untouched.
    EXPECT_TRUE(*forest.GetValueAt(Path("bbb/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("bbb/222")));
  }
  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.PruneAll(Path("aaa"), std::set<std::string>(
                                  {std::string("111"), std::string("222")}));

    // Both children should now be true.
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa/111")));
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa/222")));

    // Should remain untouched.
    EXPECT_TRUE(*forest.GetValueAt(Path("bbb/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("bbb/222")));
  }
  {
    PruneForest forest = default_forest;
    PruneForestRef ref(&forest);

    ref.PruneAll(Path(), std::set<std::string>({std::string("aaa")}));

    // aaa should now be true, and all children of it should be eliminated.
    EXPECT_TRUE(*forest.GetValueAt(Path("aaa")));

    // Children are now eliminated.
    EXPECT_EQ(forest.GetValueAt(Path("aaa/111")), nullptr);
    EXPECT_EQ(forest.GetValueAt(Path("aaa/222")), nullptr);

    // Should remain untouched.
    EXPECT_TRUE(*forest.GetValueAt(Path("bbb/111")));
    EXPECT_FALSE(*forest.GetValueAt(Path("bbb/222")));
  }
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
