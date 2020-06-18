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


#include "database/src/desktop/view/change.h"

#include "app/src/include/firebase/variant.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(Change, DefaultConstructor) {
  Change change;
  EXPECT_EQ(change.indexed_variant.variant(), Variant::Null());
  EXPECT_EQ(change.child_key, "");
  EXPECT_EQ(change.prev_name, "");
  EXPECT_EQ(change.old_indexed_variant.variant(), Variant::Null());
}

TEST(Change, CopyConstructor) {
  Change change;
  change.event_type = kEventTypeValue;
  change.indexed_variant = IndexedVariant(Variant("string variant"));
  change.child_key = "Hello";
  change.prev_name = "World";
  change.old_indexed_variant = IndexedVariant(Variant(1234567890));

  Change copy_constructed(change);
  EXPECT_EQ(copy_constructed.event_type, kEventTypeValue);
  EXPECT_EQ(copy_constructed.indexed_variant.variant(),
            IndexedVariant(Variant("string variant")).variant());
  EXPECT_EQ(copy_constructed.child_key, "Hello");
  EXPECT_EQ(copy_constructed.prev_name, "World");
  EXPECT_EQ(copy_constructed.old_indexed_variant.variant(),
            Variant(1234567890));

  Change copy_assigned;
  copy_assigned = change;
  EXPECT_EQ(copy_assigned.event_type, kEventTypeValue);
  EXPECT_EQ(copy_assigned.indexed_variant.variant(),
            IndexedVariant(Variant("string variant")).variant());
  EXPECT_EQ(copy_assigned.child_key, "Hello");
  EXPECT_EQ(copy_assigned.prev_name, "World");
  EXPECT_EQ(copy_assigned.old_indexed_variant.variant(), Variant(1234567890));
}

TEST(Change, MoveConstructor) {
  {
    Change change;
    change.event_type = kEventTypeValue;
    change.indexed_variant = IndexedVariant(Variant("string variant"));
    change.child_key = "Hello";
    change.prev_name = "World";
    change.old_indexed_variant = IndexedVariant(Variant(1234567890));

    Change move_constructed(std::move(change));
    EXPECT_EQ(move_constructed.event_type, kEventTypeValue);
    EXPECT_EQ(move_constructed.indexed_variant.variant(),
              IndexedVariant(Variant("string variant")).variant());
    EXPECT_EQ(move_constructed.child_key, "Hello");
    EXPECT_EQ(move_constructed.prev_name, "World");
    EXPECT_EQ(move_constructed.old_indexed_variant.variant(),
              Variant(1234567890));
  }

  {
    Change change;
    change.event_type = kEventTypeValue;
    change.indexed_variant = IndexedVariant(Variant("string variant"));
    change.child_key = "Hello";
    change.prev_name = "World";
    change.old_indexed_variant = IndexedVariant(Variant(1234567890));

    Change move_assigned;
    move_assigned = change;
    EXPECT_EQ(move_assigned.event_type, kEventTypeValue);
    EXPECT_EQ(move_assigned.indexed_variant.variant(),
              IndexedVariant(Variant("string variant")).variant());
    EXPECT_EQ(move_assigned.child_key, "Hello");
    EXPECT_EQ(move_assigned.prev_name, "World");
    EXPECT_EQ(move_assigned.old_indexed_variant.variant(), Variant(1234567890));
  }
}

TEST(Change, Constructors) {
  Change type_variant(kEventTypeValue,
                      IndexedVariant(Variant("abcdefghijklmnopqrstuvwxyz")));

  EXPECT_EQ(type_variant.event_type, kEventTypeValue);
  EXPECT_EQ(type_variant.indexed_variant.variant(),
            Variant("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_EQ(type_variant.child_key, "");
  EXPECT_EQ(type_variant.prev_name, "");
  EXPECT_EQ(type_variant.old_indexed_variant.variant(), Variant::Null());

  Change type_variant_string(
      kEventTypeChildChanged,
      IndexedVariant(Variant("zyxwvutsrqponmlkjihgfedcba")), "child_key");
  EXPECT_EQ(type_variant_string.event_type, kEventTypeChildChanged);
  EXPECT_EQ(type_variant_string.indexed_variant.variant(),
            Variant("zyxwvutsrqponmlkjihgfedcba"));
  EXPECT_EQ(type_variant_string.child_key, "child_key");
  EXPECT_EQ(type_variant_string.prev_name, "");
  EXPECT_EQ(type_variant_string.old_indexed_variant.variant(), Variant::Null());

  Change all_values_set(kEventTypeChildRemoved,
                        IndexedVariant(Variant("ABCDEFGHIJKLMNOPQRSTUVWXYZ")),
                        "another_child_key", "previous_child",
                        IndexedVariant(Variant("ZYXWVUSTRQPONMLKJIHGFEDCBA")));
  EXPECT_EQ(all_values_set.event_type, kEventTypeChildRemoved);
  EXPECT_EQ(all_values_set.indexed_variant.variant(),
            Variant("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  EXPECT_EQ(all_values_set.child_key, "another_child_key");
  EXPECT_EQ(all_values_set.prev_name, "previous_child");
  EXPECT_EQ(all_values_set.old_indexed_variant.variant(),
            Variant("ZYXWVUSTRQPONMLKJIHGFEDCBA"));
}

TEST(Change, ValueChange) {
  Change change = ValueChange(IndexedVariant(Variant("ValueChanged!")));

  EXPECT_EQ(change.event_type, kEventTypeValue);
  EXPECT_EQ(change.indexed_variant.variant(),
            IndexedVariant(Variant("ValueChanged!")).variant());
  EXPECT_EQ(change.child_key, "");
  EXPECT_EQ(change.prev_name, "");
  EXPECT_EQ(change.old_indexed_variant.variant(), Variant::Null());
}

TEST(Change, ChildAddedChange) {
  Change change =
      ChildAddedChange("child_key", IndexedVariant(Variant("ValueChanged!")));

  EXPECT_EQ(change.event_type, kEventTypeChildAdded);
  EXPECT_EQ(change.indexed_variant.variant(), Variant("ValueChanged!"));
  EXPECT_EQ(change.child_key, "child_key");
  EXPECT_EQ(change.prev_name, "");
  EXPECT_EQ(change.old_indexed_variant.variant(), Variant::Null());

  Change another_change =
      ChildAddedChange("another_child_key", Variant("!ChangedValue"));

  EXPECT_EQ(another_change.event_type, kEventTypeChildAdded);
  EXPECT_EQ(another_change.indexed_variant.variant(), Variant("!ChangedValue"));
  EXPECT_EQ(another_change.child_key, "another_child_key");
  EXPECT_EQ(another_change.prev_name, "");
  EXPECT_EQ(another_change.old_indexed_variant.variant(), Variant::Null());
}

TEST(Change, ChildRemovedChange) {
  Change change =
      ChildRemovedChange("child_key", IndexedVariant(Variant("ChildRemoved!")));

  EXPECT_EQ(change.event_type, kEventTypeChildRemoved);
  EXPECT_EQ(change.indexed_variant.variant(), Variant("ChildRemoved!"));
  EXPECT_EQ(change.child_key, "child_key");
  EXPECT_EQ(change.prev_name, "");
  EXPECT_EQ(change.old_indexed_variant.variant(), Variant::Null());

  Change another_change =
      ChildRemovedChange("another_child_key", Variant("!RemovedChild"));

  EXPECT_EQ(another_change.event_type, kEventTypeChildRemoved);
  EXPECT_EQ(another_change.indexed_variant.variant(), Variant("!RemovedChild"));
  EXPECT_EQ(another_change.child_key, "another_child_key");
  EXPECT_EQ(another_change.prev_name, "");
  EXPECT_EQ(another_change.old_indexed_variant.variant(), Variant::Null());
}

TEST(Change, ChildChangedChange) {
  Change change =
      ChildChangedChange("child_key", IndexedVariant(Variant("ChildChanged!")),
                         IndexedVariant(Variant("old value")));

  EXPECT_EQ(change.event_type, kEventTypeChildChanged);
  EXPECT_EQ(change.indexed_variant.variant(), Variant("ChildChanged!"));
  EXPECT_EQ(change.child_key, "child_key");
  EXPECT_EQ(change.prev_name, "");
  EXPECT_EQ(change.old_indexed_variant.variant(), Variant("old value"));

  Change another_change = ChildChangedChange(
      "another_child_key", Variant("!ChangedChild"), Variant("previous value"));

  EXPECT_EQ(another_change.event_type, kEventTypeChildChanged);
  EXPECT_EQ(another_change.indexed_variant.variant(), Variant("!ChangedChild"));
  EXPECT_EQ(another_change.child_key, "another_child_key");
  EXPECT_EQ(another_change.prev_name, "");
  EXPECT_EQ(another_change.old_indexed_variant.variant(),
            Variant("previous value"));
}

TEST(Change, ChildMovedChange) {
  Change change =
      ChildMovedChange("child_key", IndexedVariant(Variant("ChildChanged!")));

  EXPECT_EQ(change.event_type, kEventTypeChildMoved);
  EXPECT_EQ(change.indexed_variant.variant(), Variant("ChildChanged!"));
  EXPECT_EQ(change.child_key, "child_key");
  EXPECT_EQ(change.prev_name, "");
  EXPECT_EQ(change.old_indexed_variant.variant(), Variant::Null());

  Change another_change =
      ChildMovedChange("another_child_key", Variant("!ChangedChild"));

  EXPECT_EQ(another_change.event_type, kEventTypeChildMoved);
  EXPECT_EQ(another_change.indexed_variant.variant(), Variant("!ChangedChild"));
  EXPECT_EQ(another_change.child_key, "another_child_key");
  EXPECT_EQ(another_change.prev_name, "");
  EXPECT_EQ(another_change.old_indexed_variant.variant(), Variant::Null());
}

TEST(Change, ChangeWithPrevName) {
  Change change;
  change.event_type = kEventTypeValue;
  change.indexed_variant = IndexedVariant(Variant("value"));
  change.child_key = "child_key";
  change.prev_name = "";
  change.old_indexed_variant = IndexedVariant(Variant(1234567890));

  Change result = ChangeWithPrevName(change, "prev_name");

  EXPECT_EQ(result.event_type, kEventTypeValue);
  EXPECT_EQ(result.indexed_variant.variant(),
            IndexedVariant("value").variant());
  EXPECT_EQ(result.child_key, "child_key");
  EXPECT_EQ(result.prev_name, "prev_name");
  EXPECT_EQ(result.old_indexed_variant.variant(), Variant(1234567890));
}

TEST(Change, EqualityOperatorSame) {
  Change change;
  change.event_type = kEventTypeValue;
  change.indexed_variant = IndexedVariant(Variant("value"));
  change.child_key = "child_key";
  change.prev_name = "prev_name";
  change.old_indexed_variant = IndexedVariant(Variant(1234567890));

  Change identical_change;
  identical_change.event_type = kEventTypeValue;
  identical_change.indexed_variant = IndexedVariant(Variant("value"));
  identical_change.child_key = "child_key";
  identical_change.prev_name = "prev_name";
  identical_change.old_indexed_variant = IndexedVariant(Variant(1234567890));

  // Verify the == and != operators return the expected result.
  // Check equality with self.
  EXPECT_TRUE(change == change);
  EXPECT_FALSE(change != change);

  // Check equality with an identical change.
  EXPECT_TRUE(change == identical_change);
  EXPECT_FALSE(change != identical_change);
}

TEST(Change, EqualityOperatorDifferent) {
  Change change;
  change.event_type = kEventTypeValue;
  change.indexed_variant = IndexedVariant(Variant("value"));
  change.child_key = "child_key";
  change.prev_name = "prev_name";
  change.old_indexed_variant = IndexedVariant(Variant(1234567890));

  Change change_different_type;
  change_different_type.event_type = kEventTypeChildAdded;
  change_different_type.indexed_variant = IndexedVariant(Variant("value"));
  change_different_type.child_key = "child_key";
  change_different_type.prev_name = "prev_name";
  change_different_type.old_indexed_variant =
      IndexedVariant(Variant(1234567890));

  Change change_different_indexed_variant;
  change_different_indexed_variant.event_type = kEventTypeValue;
  change_different_indexed_variant.indexed_variant =
      IndexedVariant(Variant("aeluv"));
  change_different_indexed_variant.child_key = "child_key";
  change_different_indexed_variant.prev_name = "prev_name";
  change_different_indexed_variant.old_indexed_variant =
      IndexedVariant(Variant(1234567890));

  Change change_different_child_key;
  change_different_child_key.event_type = kEventTypeValue;
  change_different_child_key.indexed_variant = IndexedVariant(Variant("value"));
  change_different_child_key.child_key = "cousin_key";
  change_different_child_key.prev_name = "prev_name";
  change_different_child_key.old_indexed_variant =
      IndexedVariant(Variant(1234567890));

  Change change_different_prev_name;
  change_different_prev_name.event_type = kEventTypeValue;
  change_different_prev_name.indexed_variant = IndexedVariant(Variant("value"));
  change_different_prev_name.child_key = "child_key";
  change_different_prev_name.prev_name = "next_name";
  change_different_prev_name.old_indexed_variant =
      IndexedVariant(Variant(1234567890));

  Change change_different_old_indexed_variant;
  change_different_old_indexed_variant.event_type = kEventTypeValue;
  change_different_old_indexed_variant.indexed_variant =
      IndexedVariant(Variant("value"));
  change_different_old_indexed_variant.child_key = "child_key";
  change_different_old_indexed_variant.prev_name = "prev_name";
  change_different_old_indexed_variant.old_indexed_variant =
      IndexedVariant(Variant(int64_t(9876543210)));

  // Verify the == and != operators return the expected result.
  EXPECT_FALSE(change == change_different_type);
  EXPECT_TRUE(change != change_different_type);

  EXPECT_FALSE(change == change_different_indexed_variant);
  EXPECT_TRUE(change != change_different_indexed_variant);

  EXPECT_FALSE(change == change_different_child_key);
  EXPECT_TRUE(change != change_different_child_key);

  EXPECT_FALSE(change == change_different_prev_name);
  EXPECT_TRUE(change != change_different_prev_name);

  EXPECT_FALSE(change == change_different_old_indexed_variant);
  EXPECT_TRUE(change != change_different_old_indexed_variant);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
