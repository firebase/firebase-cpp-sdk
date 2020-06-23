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

#include "database/src/desktop/view/event_generator.h"

#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/child_event_registration.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/util_desktop.h"

using testing::Eq;
using testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {

class EventGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    query_spec_.path = Path("prefix/path");
    data_cache_ = Variant(std::map<Variant, Variant>{
        std::make_pair("aaa", CombineValueAndPriority(100, 1)),
        std::make_pair("bbb", CombineValueAndPriority(200, 2)),
        std::make_pair("ccc", CombineValueAndPriority(300, 3)),
        std::make_pair("ddd", CombineValueAndPriority(400, 4)),
    });
    event_cache_ = IndexedVariant(data_cache_, query_spec_.params);
    value_registration_ =
        new ValueEventRegistration(nullptr, nullptr, QuerySpec());
    child_registration_ =
        new ChildEventRegistration(nullptr, nullptr, QuerySpec());
    event_registrations_ = std::vector<UniquePtr<EventRegistration>>{
        UniquePtr<EventRegistration>(value_registration_),
        UniquePtr<EventRegistration>(child_registration_),
    };
  }

 protected:
  QuerySpec query_spec_;
  Variant data_cache_;
  IndexedVariant event_cache_;
  ValueEventRegistration* value_registration_;
  ChildEventRegistration* child_registration_;
  std::vector<UniquePtr<EventRegistration>> event_registrations_;
};

class EventGeneratorDeathTest : public EventGeneratorTest {};

TEST_F(EventGeneratorTest, GenerateEventsForChangesAllAdded) {
  std::vector<Change> changes{
      ChildAddedChange("aaa", CombineValueAndPriority(100, 1)),
      ChildAddedChange("bbb", CombineValueAndPriority(200, 2)),
      ChildAddedChange("ccc", CombineValueAndPriority(300, 3)),
      ChildAddedChange("ddd", CombineValueAndPriority(400, 4)),
  };

  std::vector<Event> result = GenerateEventsForChanges(
      query_spec_, changes, event_cache_, event_registrations_);

  std::vector<Event> expected{
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(100, 1),
                                 QuerySpec(Path("prefix/path/aaa"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(200, 2),
                                 QuerySpec(Path("prefix/path/bbb"))),
            "aaa"),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(400, 4),
                                 QuerySpec(Path("prefix/path/ddd"))),
            "ccc"),
  };

  EXPECT_THAT(result, Pointwise(Eq(), expected));
}

TEST_F(EventGeneratorTest, GenerateEventsForChangesAllAddedReverseOrder) {
  std::vector<Change> changes{
      ChildAddedChange("ddd", CombineValueAndPriority(400, 4)),
      ChildAddedChange("ccc", CombineValueAndPriority(300, 3)),
      ChildAddedChange("bbb", CombineValueAndPriority(200, 2)),
      ChildAddedChange("aaa", CombineValueAndPriority(100, 1)),
  };

  std::vector<Event> result = GenerateEventsForChanges(
      query_spec_, changes, event_cache_, event_registrations_);

  // The events are sorted into order based on the query_spec's comparison
  // rules. In this case, based on priority.
  std::vector<Event> expected{
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(100, 1),
                                 QuerySpec(Path("prefix/path/aaa"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(200, 2),
                                 QuerySpec(Path("prefix/path/bbb"))),
            "aaa"),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(400, 4),
                                 QuerySpec(Path("prefix/path/ddd"))),
            "ccc"),
  };

  EXPECT_THAT(result, Pointwise(Eq(), expected));
}

TEST_F(EventGeneratorTest, GenerateEventsForChangesDifferentTypes) {
  std::vector<Change> changes{
      ChildAddedChange("aaa", CombineValueAndPriority(100, 1)),
      ChildChangedChange("ccc", CombineValueAndPriority(300, 3),
                         CombineValueAndPriority(300, 3)),
      ChildRemovedChange("eee", CombineValueAndPriority(500, 5)),
  };

  std::vector<Event> result = GenerateEventsForChanges(
      query_spec_, changes, event_cache_, event_registrations_);

  // The events are sorted into order based on the EventType.
  std::vector<Event> expected{
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(500, 5),
                                 QuerySpec(Path("prefix/path/eee"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(100, 1),
                                 QuerySpec(Path("prefix/path/aaa"))),
            ""),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
  };

  EXPECT_THAT(result, Pointwise(Eq(), expected));
}

TEST_F(EventGeneratorTest, GenerateEventsForChangesSomeDifferentTypes) {
  std::vector<Change> changes{
      ChildAddedChange("bbb", CombineValueAndPriority(200, 2)),
      ChildAddedChange("aaa", CombineValueAndPriority(100, 1)),
      ChildChangedChange("ddd", CombineValueAndPriority(400, 4),
                         CombineValueAndPriority(400, 4)),
      ChildChangedChange("ccc", CombineValueAndPriority(300, 3),
                         CombineValueAndPriority(300, 3)),
      ChildRemovedChange("fff", CombineValueAndPriority(600, 6)),
      ChildRemovedChange("eee", CombineValueAndPriority(500, 5)),
  };

  std::vector<Event> result = GenerateEventsForChanges(
      query_spec_, changes, event_cache_, event_registrations_);

  // The events are sorted into order based on the EventType and the
  // query_spec's comparison rules. In this case, based on priority.
  std::vector<Event> expected{
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(500, 5),
                                 QuerySpec(Path("prefix/path/eee"))),
            ""),
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(600, 6),
                                 QuerySpec(Path("prefix/path/fff"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(100, 1),
                                 QuerySpec(Path("prefix/path/aaa"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(200, 2),
                                 QuerySpec(Path("prefix/path/bbb"))),
            "aaa"),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(400, 4),
                                 QuerySpec(Path("prefix/path/ddd"))),
            "ccc"),
  };

  EXPECT_THAT(result, Pointwise(Eq(), expected));
}

TEST_F(EventGeneratorTest, GenerateEventsForChangesWithDifferentPriorities) {
  std::vector<Change> changes{
      ChildAddedChange("bbb", CombineValueAndPriority(200, 2)),
      ChildAddedChange("aaa", CombineValueAndPriority(100, 1)),
      // The priorities of ccc and ddd are reversed in the old snapshot.
      ChildChangedChange("ddd", CombineValueAndPriority(400, 4),
                         CombineValueAndPriority(400, 3)),
      ChildChangedChange("ccc", CombineValueAndPriority(300, 3),
                         CombineValueAndPriority(300, 4)),
      ChildRemovedChange("fff", CombineValueAndPriority(600, 6)),
      ChildRemovedChange("eee", CombineValueAndPriority(500, 5)),
  };

  std::vector<Event> result = GenerateEventsForChanges(
      query_spec_, changes, event_cache_, event_registrations_);

  // The events are sorted into order based on the EventType and the
  // query_spec's comparison rules. In this case, based on priority.
  std::vector<Event> expected{
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(500, 5),
                                 QuerySpec(Path("prefix/path/eee"))),
            ""),
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(600, 6),
                                 QuerySpec(Path("prefix/path/fff"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(100, 1),
                                 QuerySpec(Path("prefix/path/aaa"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(200, 2),
                                 QuerySpec(Path("prefix/path/bbb"))),
            "aaa"),
      // Moving the priority generated both move and change events.
      Event(kEventTypeChildMoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
      Event(kEventTypeChildMoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(400, 4),
                                 QuerySpec(Path("prefix/path/ddd"))),
            "ccc"),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(400, 4),
                                 QuerySpec(Path("prefix/path/ddd"))),
            "ccc"),
  };

  EXPECT_THAT(result, Pointwise(Eq(), expected));
}

TEST_F(EventGeneratorTest, GenerateEventsForChangesWithDifferentQuerySpec) {
  std::vector<Change> changes{
      ChildAddedChange("bbb", CombineValueAndPriority(200, 2)),
      ChildAddedChange("aaa", CombineValueAndPriority(100, 1)),
      ChildChangedChange("ddd", CombineValueAndPriority(400, 4),
                         CombineValueAndPriority(400, 3)),
      ChildChangedChange("ccc", CombineValueAndPriority(300, 3),
                         CombineValueAndPriority(300, 4)),
      ChildRemovedChange("fff", CombineValueAndPriority(600, 6)),
      ChildRemovedChange("eee", CombineValueAndPriority(500, 5)),
  };

  // Changing the priority doesn't matter when the QuerySpec does not consider
  // priority (e.g., when it orders the elements by value).
  QuerySpec value_query_spec = query_spec_;
  value_query_spec.params.order_by = QueryParams::kOrderByValue;

  std::vector<Event> result = GenerateEventsForChanges(
      value_query_spec, changes, event_cache_, event_registrations_);

  // No move events this time around even though the priorities changed because
  // the QuerySpec isn't ordered by priority, it's ordered by value.
  std::vector<Event> expected{
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(500, 5),
                                 QuerySpec(Path("prefix/path/eee"))),
            ""),
      Event(kEventTypeChildRemoved, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(600, 6),
                                 QuerySpec(Path("prefix/path/fff"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(100, 1),
                                 QuerySpec(Path("prefix/path/aaa"))),
            ""),
      Event(kEventTypeChildAdded, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(200, 2),
                                 QuerySpec(Path("prefix/path/bbb"))),
            "aaa"),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(300, 3),
                                 QuerySpec(Path("prefix/path/ccc"))),
            "bbb"),
      Event(kEventTypeChildChanged, child_registration_,
            DataSnapshotInternal(nullptr, CombineValueAndPriority(400, 4),
                                 QuerySpec(Path("prefix/path/ddd"))),
            "ccc"),
  };

  EXPECT_THAT(result, Pointwise(Eq(), expected));
}

TEST_F(EventGeneratorDeathTest, MissingChildName) {
  std::vector<Change> changes{
      ChildAddedChange("", CombineValueAndPriority(100, 1)),
  };
  // All child changes are expected to have a key. Missing a key means we have a
  // malformed Change object.
  EXPECT_DEATH(GenerateEventsForChanges(QuerySpec(), changes, event_cache_,
                                        event_registrations_),
               DEATHTEST_SIGABRT);
}

TEST_F(EventGeneratorDeathTest, MultipleValueChanges) {
  std::vector<Change> changes{
      ValueChange(IndexedVariant(Variant("aaa"))),
      ValueChange(IndexedVariant(Variant("bbb"))),
  };
  // Value changes only occur one at a time, so if we have two something has
  // gone wrong at the call site.
  EXPECT_DEATH(GenerateEventsForChanges(QuerySpec(), changes, event_cache_,
                                        event_registrations_),
               DEATHTEST_SIGABRT);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
