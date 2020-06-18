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


#include "database/src/desktop/view/child_change_accumulator.h"
#include "database/src/desktop/view/change.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

// Test to add new change data to the accumulator.
TEST(ChildChangeAccumulator, TrackChildChangeNew) {
  // Add single ChildAdded change to the accumulator.
  {
    ChildChangeAccumulator accumulator;
    Change change = ChildAddedChange("ChildAdd", 1);
    TrackChildChange(change, &accumulator);

    auto it = accumulator.find("ChildAdd");
    ASSERT_NE(it, accumulator.end());

    EXPECT_EQ(it->second, change);
  }
  // Add single ChildChanged change to the accumulator.
  {
    ChildChangeAccumulator accumulator;
    Change change = ChildChangedChange("ChildChange", "new", "old");
    TrackChildChange(change, &accumulator);

    auto it = accumulator.find("ChildChange");
    ASSERT_NE(it, accumulator.end());
    EXPECT_EQ(it->second, change);
  }
  // Add single ChildRemoved change to the accumulator.
  {
    ChildChangeAccumulator accumulator;
    Change change = ChildRemovedChange("ChildRemove", true);
    TrackChildChange(change, &accumulator);

    auto it = accumulator.find("ChildRemove");
    ASSERT_NE(it, accumulator.end());
    EXPECT_EQ(it->second, change);
  }
  // Add all ChildAdded, ChildChanged, ChildRemoved change with different child
  // key to the accumulator.
  {
    ChildChangeAccumulator accumulator;
    Change change_add = ChildAddedChange("ChildAdd", 1);
    TrackChildChange(change_add, &accumulator);

    Change change_change = ChildChangedChange("ChildChange", "new", "old");
    TrackChildChange(change_change, &accumulator);

    Change change_remove = ChildRemovedChange("ChildRemove", true);
    TrackChildChange(change_remove, &accumulator);

    // Verify child "ChildAdd"
    auto it_add = accumulator.find("ChildAdd");
    ASSERT_NE(it_add, accumulator.end());
    EXPECT_EQ(it_add->second, change_add);

    // Verify child "ChildChange"
    auto it_change = accumulator.find("ChildChange");
    ASSERT_NE(it_change, accumulator.end());
    EXPECT_EQ(it_change->second, change_change);

    // Verify child "ChildRemove"
    auto it_remove = accumulator.find("ChildRemove");
    ASSERT_NE(it_remove, accumulator.end());
    EXPECT_EQ(it_remove->second, change_remove);
  }
}

// Test to resolve ChildRemoved change and ChildAdded change with the same key
// in sequence.
TEST(ChildChangeAccumulator, TrackChildChangeRemovedThenAdded) {
  ChildChangeAccumulator accumulator;
  TrackChildChange(ChildRemovedChange("ChildRemoveThenAdd", "old"),
                   &accumulator);
  TrackChildChange(ChildAddedChange("ChildRemoveThenAdd", "new"), &accumulator);

  // Expected result should be a ChildChanged change from "old" to "new"
  Change expected = ChildChangedChange("ChildRemoveThenAdd", "new", "old");

  auto it = accumulator.find("ChildRemoveThenAdd");
  ASSERT_NE(it, accumulator.end());
  EXPECT_EQ(it->second, expected);
}

// Test to resolve ChildAdded change and ChildRemoved change with the same key
// in sequence.
TEST(ChildChangeAccumulator, TrackChildChangeAddedThenRemoved) {
  ChildChangeAccumulator accumulator;
  TrackChildChange(ChildAddedChange("ChildAddThenRemove", 1), &accumulator);
  // Note: the removed value "true" does not need to match the value "1" added
  //       previously.
  TrackChildChange(ChildRemovedChange("ChildAddThenRemove", true),
                   &accumulator);

  // Expect the child data to be removed
  auto it = accumulator.find("ChildAddAndRemove");
  ASSERT_EQ(it, accumulator.end());
}

// Test to resolve ChildChanged change and ChildRemoved change with the same key
// in sequence.
TEST(ChildChangeAccumulator, TrackChildChangeChangedThenRemoved) {
  ChildChangeAccumulator accumulator;
  TrackChildChange(ChildChangedChange("ChildChangeThenRemove", "old", "order"),
                   &accumulator);
  // Note: the removed value "new" does not need to match the value "old"
  //       changed previously.
  TrackChildChange(ChildRemovedChange("ChildChangeThenRemove", "new"),
                   &accumulator);

  // Expected result should be a ChildRemoved change from "old" value
  Change expected = ChildRemovedChange("ChildChangeThenRemove", "old");

  auto it = accumulator.find("ChildChangeThenRemove");
  ASSERT_NE(it, accumulator.end());
  EXPECT_EQ(it->second, expected);
}

// Test to resolve ChildAdded change and ChildChanged change with the same key
// in sequence.
TEST(ChildChangeAccumulator, TrackChildChangeAddedThenChanged) {
  ChildChangeAccumulator accumulator;
  TrackChildChange(ChildAddedChange("ChildAddThenChange", "old"), &accumulator);
  // Note: the old value "something else" does not need to match the value "old"
  //       added previously.
  TrackChildChange(
      ChildChangedChange("ChildAddThenChange", "new", "something else"),
      &accumulator);

  // Expected result should be a ChildAdded change with "new" value
  Change expected = ChildAddedChange("ChildAddThenChange", "new");

  auto it = accumulator.find("ChildAddThenChange");
  ASSERT_NE(it, accumulator.end());
  EXPECT_EQ(it->second, expected);
}

// Test to resolve ChildChanged change and ChildChanged change with the same key
// in sequence.
TEST(ChildChangeAccumulator, TrackChildChangeChangedThenChanged) {
  ChildChangeAccumulator accumulator;
  TrackChildChange(ChildChangedChange("ChildChangeThenChange", "old", "older"),
                   &accumulator);
  // Note: the old value "something else" does not need to match the value "old"
  //       changed previously.
  TrackChildChange(
      ChildChangedChange("ChildChangeThenChange", "new", "something else"),
      &accumulator);

  // Expected result should be a ChildChanged change from "older" to "new".
  Change expected = ChildChangedChange("ChildChangeThenChange", "new", "older");

  auto it = accumulator.find("ChildChangeThenChange");
  ASSERT_NE(it, accumulator.end());
  EXPECT_EQ(it->second, expected);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
