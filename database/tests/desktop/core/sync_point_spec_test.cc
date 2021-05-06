// Copyright 2021 Google LLC
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

#include <iostream>
#include <memory>
#include <set>

#include "app/src/include/firebase/variant.h"
#include "app/src/logger.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/core/child_event_registration.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/sync_point_spec_generated.h"
#include "database/src/desktop/core/sync_tree.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/persistence/noop_persistence_manager.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/persistence/persistence_manager_interface.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/query_desktop.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/common.h"
#include "flatbuffers/util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Eq;
using ::testing::Not;
using ::testing::Pointwise;
using ::testing::Test;
using ::testing::UnorderedPointwise;

namespace firebase {

using util::FlexbufferToVariant;

namespace database {
namespace internal {

// Compare all fields of Events except the event registration pointer.
MATCHER(EventEq, "EventEq") {
  const Event& event_a = std::get<0>(arg);
  const Event& event_b = std::get<1>(arg);
  if (event_a.snapshot.has_value() != event_b.snapshot.has_value()) {
    return false;
  } else if (event_a.snapshot.has_value()) {
    const DataSnapshotInternal& snapshot_a = *event_a.snapshot;
    const DataSnapshotInternal& snapshot_b = *event_b.snapshot;
    if (snapshot_a.GetKeyString() != snapshot_b.GetKeyString() ||
        snapshot_a.GetValue() != snapshot_b.GetValue() ||
        snapshot_a.GetPriority() != snapshot_b.GetPriority()) {
      return false;
    }
  }
  return event_a.prev_name == event_b.prev_name && event_a.path == event_b.path;
}

namespace {

class FakeListenProvider : public ListenProvider {
 public:
  FakeListenProvider(LoggerBase* logger) : logger_(logger), listens_() {}

  ~FakeListenProvider() override {}

  void StartListening(const QuerySpec& query_spec, const Tag& tag,
                      const View* view) override {
    const Path& path = query_spec.path;
    logger_->LogDebug(
        "Listening at %s for Tag %s", path.c_str(),
        tag.has_value() ? std::to_string(*tag).c_str() : "<None>");
    assert(listens_.count(query_spec) == 0);
    listens_.insert(query_spec);
  }

  void StopListening(const QuerySpec& query_spec, const Tag& tag) override {
    const Path& path = query_spec.path;
    logger_->LogDebug(
        "Stop listening at %s for Tag %s", path.c_str(),
        tag.has_value() ? std::to_string(*tag).c_str() : "<None>");
    assert(listens_.count(query_spec) > 0);
    listens_.erase(query_spec);
  }

 private:
  LoggerBase* logger_;
  std::set<QuerySpec> listens_;
};

class SyncTreeTest : public Test {
 public:
  SyncTreeTest() {
    const char* kTestDataFile = "sync_point_spec.bin";
    flatbuffers::LoadFile(kTestDataFile, true, &buffer_);
    test_suite_ = test_data::GetTestSuite(buffer_.data());
  }

  void SetUp() override {
    sync_tree_ = new SyncTree(MakeUnique<WriteTree>(),
                              MakeUnique<NoopPersistenceManager>(),
                              MakeUnique<FakeListenProvider>(&logger_));
  }

  void TearDown() override { delete sync_tree_; }
  void RunOne(const char* name);
  void RunTest(const test_data::TestCase* test_spec, Path base_path);

 protected:
  std::string buffer_;
  const test_data::TestSuite* test_suite_;

  SystemLogger logger_;
  SyncTree* sync_tree_;
};

class TestEventRegistration : public EventRegistration {
 public:
  TestEventRegistration(QuerySpec query_spec) : EventRegistration(query_spec) {}

  bool RespondsTo(EventType event_type) override { return true; }

  Event GenerateEvent(const Change& change,
                      const QuerySpec& query_spec) override {
    if (change.event_type == kEventTypeValue) {
      return Event(kEventTypeValue, this,
                   DataSnapshotInternal(
                       nullptr, change.indexed_variant.variant(),
                       QuerySpec(query_spec.path.GetChild(change.child_key),
                                 change.indexed_variant.query_params())));
    } else {
      return Event(change.event_type, this,
                   DataSnapshotInternal(
                       nullptr, change.indexed_variant.variant(),
                       QuerySpec(query_spec.path.GetChild(change.child_key),
                                 change.indexed_variant.query_params())),
                   change.prev_name);
    }
  }

  void FireEvent(const Event& event) override {
    EXPECT_TRUE(false) << "Can't raise test events!";
  }

  void FireCancelEvent(Error error) override {
    EXPECT_TRUE(false) << "Can't raise test events!";
  }

  bool MatchesListener(const void* listener_ptr) const override {
    return static_cast<const void*>(this) == listener_ptr;
  }
};

UniquePtr<QueryInternal> ParseQuery(
    UniquePtr<QueryInternal> query,
    const test_data::QueryParams* query_params) {
  EXPECT_NE(query_params->tag(), 0) << "Non-default queries must have a tag";
  if (query_params->orderBy()) {
    query.reset(query->OrderByChild(query_params->orderBy()->c_str()));
  } else if (query_params->orderByKey()) {
    query.reset(query->OrderByKey());
  } else if (query_params->orderByPriority()) {
    query.reset(query->OrderByPriority());
  }
  if (query_params->startAt()) {
    const test_data::Bound* bound = query_params->startAt();
    const char* name = bound->name() ? bound->name()->c_str() : nullptr;
    Variant index =
        bound->index()
            ? util::FlexbufferToVariant(bound->index_flexbuffer_root())
            : Variant::Null();
    query.reset(query->StartAt(index, name));
  }
  if (query_params->endAt()) {
    const test_data::Bound* bound = query_params->endAt();
    const char* name = bound->name() ? bound->name()->c_str() : nullptr;
    Variant index =
        bound->index()
            ? util::FlexbufferToVariant(bound->index_flexbuffer_root())
            : Variant::Null();
    query.reset(query->EndAt(index, name));
  }
  if (query_params->equalTo()) {
    const test_data::Bound* bound = query_params->equalTo();
    const char* name = bound->name() ? bound->name()->c_str() : nullptr;
    Variant index =
        bound->index()
            ? util::FlexbufferToVariant(bound->index_flexbuffer_root())
            : Variant::Null();
    query.reset(query->EqualTo(index, name));
  }
  if (query_params->limitToFirst()) {
    query.reset(query->LimitToFirst(query_params->limitToFirst()));
  }
  if (query_params->limitToLast()) {
    query.reset(query->LimitToLast(query_params->limitToLast()));
  }
  return query;
}

Event ParseEvent(const test_data::Event* event_spec, Path path) {
  EventType type = static_cast<EventType>(event_spec->type());

  EXPECT_NE(event_spec->path(), nullptr);
  path = path.GetChild(event_spec->path()->c_str());
  if (event_spec->name() != nullptr) {
    path = path.GetChild(event_spec->name()->c_str());
  }
  Variant data = event_spec->data()
                     ? FlexbufferToVariant(event_spec->data_flexbuffer_root())
                     : Variant::Null();
  DataSnapshotInternal snapshot(nullptr, data, QuerySpec(path));

  const char* prev_name =
      event_spec->prevName() != nullptr ? event_spec->prevName()->c_str() : "";
  return Event(type, nullptr, snapshot, prev_name);
}

void SyncTreeTest::RunTest(const test_data::TestCase* test_spec,
                           Path base_path) {
  logger_.LogInfo("Running \"%s\"", test_spec->name()->c_str());

  int current_write_id = 0;

  std::map<int, EventRegistration*> registrations;
  for (const test_data::Step* spec : *test_spec->steps()) {
    if (spec->comment()) {
      logger_.LogInfo(" > %s", spec->comment()->c_str());
    }
    const char* path_str = spec->path() ? spec->path()->c_str() : "";
    Path path = base_path.GetChild(path_str);
    DatabaseReferenceInternal reference(nullptr, path);
    std::vector<Event> expected;
    for (const auto* event_spec : *spec->events()) {
      expected.push_back(ParseEvent(event_spec, base_path));
    }
    switch (spec->type()) {
      case test_data::StepType_listen: {
        UniquePtr<QueryInternal> query(new QueryInternal(reference));
        if (spec->params()) {
          query = ParseQuery(std::move(query), spec->params());
        }
        EventRegistration* event_registration = nullptr;
        int callback_id = spec->callbackId();
        if (callback_id != 0 && registrations.count(callback_id) != 0) {
          event_registration = new TestEventRegistration(
              registrations[callback_id]->query_spec());
        } else {
          event_registration = new TestEventRegistration(query->query_spec());
          if (callback_id != 0) {
            registrations[callback_id] = event_registration;
          }
        }
        std::vector<Event> actual = sync_tree_->AddEventRegistration(
            UniquePtr<EventRegistration>(event_registration));
        EXPECT_THAT(actual, Pointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_unlisten: {
        EventRegistration* event_registration = nullptr;
        int callback_id = spec->callbackId();
        EXPECT_TRUE(callback_id != 0 && registrations.count(callback_id) != 0);
        event_registration = registrations[callback_id];
        ASSERT_NE(event_registration, nullptr);
        std::vector<Event> actual = sync_tree_->RemoveEventRegistration(
            event_registration->query_spec(),
            static_cast<void*>(event_registration), kErrorNone);
        EXPECT_THAT(actual, Pointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_serverUpdate: {
        Variant update = spec->data()
                             ? FlexbufferToVariant(spec->data_flexbuffer_root())
                             : Variant::Null();
        std::vector<Event> actual;
        if (spec->tag()) {
          actual = sync_tree_->ApplyTaggedQueryOverwrite(path, update,
                                                         Tag(spec->tag()));
        } else {
          actual = sync_tree_->ApplyServerOverwrite(path, update);
        }
        EXPECT_THAT(actual, UnorderedPointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_serverMerge: {
        std::map<Path, Variant> merges = VariantToPathMap(
            spec->data() ? FlexbufferToVariant(spec->data_flexbuffer_root())
                         : Variant::Null());
        std::vector<Event> actual;
        if (spec->tag()) {
          actual =
              sync_tree_->ApplyTaggedQueryMerge(path, merges, Tag(spec->tag()));
        } else {
          actual = sync_tree_->ApplyServerMerge(path, merges);
        }
        EXPECT_THAT(actual, UnorderedPointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_set: {
        Variant to_set = spec->data()
                             ? FlexbufferToVariant(spec->data_flexbuffer_root())
                             : Variant::Null();
        OverwriteVisibility visible =
            spec->visible() ? kOverwriteVisible : kOverwriteInvisible;
        // For now, assume anything visible should be persisted.
        Persist persist = spec->visible() ? kPersist : kDoNotPersist;
        std::vector<Event> actual = sync_tree_->ApplyUserOverwrite(
            path, to_set, to_set, current_write_id++, visible, persist);
        EXPECT_THAT(actual, UnorderedPointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_update: {
        CompoundWrite merges = CompoundWrite::FromVariantMerge(
            spec->data() ? FlexbufferToVariant(spec->data_flexbuffer_root())
                         : Variant::Null());
        std::vector<Event> actual = sync_tree_->ApplyUserMerge(
            path, merges, merges, current_write_id++, kPersist);
        EXPECT_THAT(actual, UnorderedPointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_ackUserWrite: {
        int to_clear = static_cast<int>(spec->writeId());
        AckStatus ack_status = spec->revert() ? kAckRevert : kAckConfirm;
        std::vector<Event> actual = sync_tree_->AckUserWrite(
            to_clear, ack_status, kPersist, /*server_time_offset=*/0);
        EXPECT_THAT(actual, UnorderedPointwise(EventEq(), expected));
        break;
      }
      case test_data::StepType_suppressWarning: {
        // Do nothing. This is a hack so JS's Jasmine tests don't throw warnings
        // for "expect no errors" tests.
        break;
      }
      default: {
        EXPECT_TRUE(false) << "Unknown spec: " << spec->type();
      }
    }
  }
}

void SyncTreeTest::RunOne(const char* name) {
  for (const auto* test_spec : *test_suite_->test_cases()) {
    if (strcmp(name, test_spec->name()->c_str()) == 0) {
      RunTest(test_spec, Path());

      TearDown();
      SetUp();

      // Run again at a deeper path.
      RunTest(test_spec, Path("foo/bar/baz"));
      return;
    }
  }
  EXPECT_TRUE(false) << "Didn't find test spec with name " << name;
}

TEST(EventEqTest, Matcher) {
  TestEventRegistration event_registration((QuerySpec()));
  TestEventRegistration another_event_registration((QuerySpec()));

  QueryParams query_params;
  QueryParams different_query_params;
  different_query_params.start_at_value = 9999;

  DataSnapshotInternal snapshot(nullptr, 1234, QuerySpec(query_params));
  DataSnapshotInternal snapshot_with_different_query_params(
      nullptr, 1234, QuerySpec(different_query_params));
  DataSnapshotInternal snapshot_with_different_value(nullptr, 4321,
                                                     QuerySpec(query_params));

  // These events should all be considered equal, even if they differ in a few
  // specific ways.
  Event event(kEventTypeValue, &event_registration, snapshot, "previous");
  Event same_event(kEventTypeValue, &event_registration, snapshot, "previous");
  Event different_registration_event(
      kEventTypeValue, &another_event_registration, snapshot, "previous");
  Event different_query_params_event(kEventTypeValue, &event_registration,
                                     snapshot, "previous");

  // These events should not be considered equal, as they each differ in
  // important, critical ways.
  Event null_snapshot_event(kEventTypeValue, &event_registration, snapshot,
                            "previous");
  null_snapshot_event.snapshot = Optional<DataSnapshotInternal>();
  Event different_snapshot_value_event(kEventTypeValue, &event_registration,
                                       snapshot_with_different_value,
                                       "previous");
  Event different_prevname_event(kEventTypeValue, &event_registration, snapshot,
                                 "next");

  EXPECT_THAT(std::make_tuple(event, same_event), EventEq());
  EXPECT_THAT(std::make_tuple(event, different_registration_event), EventEq());
  EXPECT_THAT(std::make_tuple(event, different_query_params_event), EventEq());

  EXPECT_THAT(std::make_tuple(event, null_snapshot_event), Not(EventEq()));
  EXPECT_THAT(std::make_tuple(event, different_snapshot_value_event),
              Not(EventEq()));
  EXPECT_THAT(std::make_tuple(event, different_prevname_event), Not(EventEq()));
}

TEST_F(SyncTreeTest, DefaultListenHandlesParentSet) {
  RunOne("Default listen handles a parent set");
}

TEST_F(SyncTreeTest, DefaultListenHandlesASetAtTheSameLevel) {
  RunOne("Default listen handles a set at the same level");
}

TEST_F(SyncTreeTest, AQueryCanGetACompleteCacheThenAMerge) {
  RunOne("A query can get a complete cache then a merge");
}

TEST_F(SyncTreeTest, ServerMergeOnListenerWithCompleteChildren) {
  RunOne("Server merge on listener with complete children");
}

TEST_F(SyncTreeTest, DeepMergeOnListenerWithCompleteChildren) {
  RunOne("Deep merge on listener with complete children");
}

TEST_F(SyncTreeTest, UpdateChildListenerTwice) {
  RunOne("Update child listener twice");
}

TEST_F(SyncTreeTest, ChildOfDefaultListenThatAlreadyHasACompleteCache) {
  RunOne("Update child of default listen that already has a complete cache");
}

TEST_F(SyncTreeTest, UpdateChildOfDefaultListenThatHasNoCache) {
  RunOne("Update child of default listen that has no cache");
}

TEST_F(SyncTreeTest, UpdateTheChildOfACoLocatedDefaultListenerAndQuery) {
  RunOne(
      "Update (via set) the child of a co-located default listener and "
      "query");
}

TEST_F(SyncTreeTest, UpdateTheChildOfAQueryWithAFullCache) {
  RunOne("Update (via set) the child of a query with a full cache");
}

TEST_F(SyncTreeTest, UpdateAChildBelowAnEmptyQuery) {
  RunOne("Update (via set) a child below an empty query");
}

TEST_F(SyncTreeTest, UpdateDescendantOfDefaultListenerWithFullCache) {
  RunOne("Update descendant of default listener with full cache");
}

TEST_F(SyncTreeTest, DescendantSetBelowAnEmptyDefaultLIstenerIsIgnored) {
  RunOne("Descendant set below an empty default listener is ignored");
}

TEST_F(SyncTreeTest, UpdateOfAChild) {
  RunOne(
      "Update of a child. This can happen if a child listener is added and "
      "removed");
}

TEST_F(SyncTreeTest, RevertSetWithOnlyChildCaches) {
  RunOne("Revert set with only child caches");
}

TEST_F(SyncTreeTest, CanRevertADuplicateChildSet) {
  RunOne("Can revert a duplicate child set");
}

TEST_F(SyncTreeTest, CanRevertAChildSetAndSeeTheUnderlyingData) {
  RunOne("Can revert a child set and see the underlying data");
}

TEST_F(SyncTreeTest, RevertChildSetWithNoServerData) {
  RunOne("Revert child set with no server data");
}

TEST_F(SyncTreeTest, RevertDeepSetWithNoServerData) {
  RunOne("Revert deep set with no server data");
}

TEST_F(SyncTreeTest, RevertSetCoveredByNonvisibleTransaction) {
  RunOne("Revert set covered by non-visible transaction");
}

TEST_F(SyncTreeTest, ClearParentShadowingServerValuesSetWithServerChildren) {
  RunOne("Clear parent shadowing server values set with server children");
}

TEST_F(SyncTreeTest, ClearChildShadowingServerValuesSetWithServerChildren) {
  RunOne("Clear child shadowing server values set with server children");
}

TEST_F(SyncTreeTest, UnrelatedMergeDoesntShadowServerUpdates) {
  RunOne("Unrelated merge doesn't shadow server updates");
}

TEST_F(SyncTreeTest, CanSetAlongsideARemoteMerge) {
  RunOne("Can set alongside a remote merge");
}

TEST_F(SyncTreeTest, SetPriorityOnALocationWithNoCache) {
  RunOne("setPriority on a location with no cache");
}

TEST_F(SyncTreeTest, DeepUpdateDeletesChildFromLimitWindowAndPullsInNewChild) {
  RunOne("deep update deletes child from limit window and pulls in new child");
}

TEST_F(SyncTreeTest, DeepSetDeletesChildFromLimitWindowAndPullsInNewChild) {
  RunOne("deep set deletes child from limit window and pulls in new child");
}

TEST_F(SyncTreeTest, EdgeCaseInNewChildForChange) {
  RunOne("Edge case in newChildForChange_");
}

TEST_F(SyncTreeTest, RevertSetInQueryWindow) {
  RunOne("Revert set in query window");
}

TEST_F(SyncTreeTest, HandlesAServerValueMovingAChildOutOfAQueryWindow) {
  RunOne("Handles a server value moving a child out of a query window");
}

TEST_F(SyncTreeTest, UpdateOfIndexedChildWorks) {
  RunOne("Update of indexed child works");
}

TEST_F(SyncTreeTest, MergeAppliedToEmptyLimit) {
  RunOne("Merge applied to empty limit");
}

TEST_F(SyncTreeTest, LimitIsRefilledFromServerDataAfterMerge) {
  RunOne("Limit is refilled from server data after merge");
}

TEST_F(SyncTreeTest, HandleRepeatedListenWithMergeAsFirstUpdate) {
  RunOne("Handle repeated listen with merge as first update");
}

TEST_F(SyncTreeTest, LimitIsRefilledFromServerDataAfterSet) {
  RunOne("Limit is refilled from server data after set");
}

TEST_F(SyncTreeTest, QueryOnWeirdPath) { RunOne("query on weird path."); }

TEST_F(SyncTreeTest, RunsRound2) { RunOne("runs, round2"); }

TEST_F(SyncTreeTest, HandlesNestedListens) { RunOne("handles nested listens"); }

TEST_F(SyncTreeTest, HandlesASetBelowAListen) {
  RunOne("Handles a set below a listen");
}

TEST_F(SyncTreeTest, DoesNonDefaultQueries) {
  RunOne("does non-default queries");
}

TEST_F(SyncTreeTest, HandlesCoLocatedDefaultListenerAndQuery) {
  RunOne("handles a co-located default listener and query");
}

TEST_F(SyncTreeTest,
       DefaultAndNonDefaultListenerAtSameLocationWithServerUpdate) {
  RunOne(
      "Default and non-default listener at same location with server update");
}

TEST_F(SyncTreeTest,
       AddAParentListenerToACompleteChildListenerExpectChildEvent) {
  RunOne(
      "Add a parent listener to a complete child listener, expect child "
      "event");
}

TEST_F(SyncTreeTest, AddListensToASetExpectCorrectEventsIncludingAChildEvent) {
  RunOne(
      "Add listens to a set, expect correct events, including a child event");
}

TEST_F(SyncTreeTest, ServerUpdateToAChildListenerRaisesChildEventsAtParent) {
  RunOne("ServerUpdate to a child listener raises child events at parent");
}

TEST_F(SyncTreeTest,
       ServerUpdateToAChildListenerRaisesChildEventsAtParentQuery) {
  RunOne(
      "ServerUpdate to a child listener raises child events at parent query");
}

TEST_F(SyncTreeTest, MultipleCompleteChildrenAreHandleProperly) {
  RunOne("Multiple complete children are handled properly");
}

TEST_F(SyncTreeTest, WriteLeafNodeOverwriteAtParentNode) {
  RunOne("Write leaf node, overwrite at parent node");
}

TEST_F(SyncTreeTest, ConfirmCompleteChildrenFromTheServer) {
  RunOne("Confirm complete children from the server");
}

TEST_F(SyncTreeTest, WriteLeafOverwriteFromParent) {
  RunOne("Write leaf, overwrite from parent");
}

TEST_F(SyncTreeTest, BasicUpdateTest) { RunOne("Basic update test"); }

TEST_F(SyncTreeTest, NoDoubleValueEventsForUserAck) {
  RunOne("No double value events for user ack");
}

TEST_F(SyncTreeTest, BasicKeyIndexSanityCheck) {
  RunOne("Basic key index sanity check");
}

TEST_F(SyncTreeTest, CollectCorrectSubviewsToListenOn) {
  RunOne("Collect correct subviews to listen on");
}

TEST_F(SyncTreeTest, LimitToFirstOneOnOrderedQuery) {
  RunOne("Limit to first one on ordered query");
}

TEST_F(SyncTreeTest, LimitToLastOneOnOrderedQuery) {
  RunOne("Limit to last one on ordered query");
}

TEST_F(SyncTreeTest, UpdateIndexedValueOnExistingChildFromLimitedQuery) {
  RunOne("Update indexed value on existing child from limited query");
}

TEST_F(SyncTreeTest, CanCreateStartAtEndAtEqualToQueriesWithBool) {
  RunOne("Can create startAt, endAt, equalTo queries with bool");
}

TEST_F(SyncTreeTest, QueryForExistingServerSnap) {
  RunOne("Query with existing server snap");
}

TEST_F(SyncTreeTest, ServerDataIsNotPurgedForNonServerIndexedQueries) {
  RunOne("Server data is not purged for non-server-indexed queries");
}

TEST_F(SyncTreeTest, LimitWithCustomOrderByIsRefilledWithCorrectItem) {
  RunOne("Limit with custom orderBy is refilled with correct item");
}

TEST_F(SyncTreeTest, StartAtEndAtDominatesLimit) {
  RunOne("startAt/endAt dominates limit");
}

TEST_F(SyncTreeTest, UpdateToSingleChildThatMovesOutOfWindow) {
  RunOne("Update to single child that moves out of window");
}

TEST_F(SyncTreeTest, LimitedQueryDoesntPullInOutOfRangeChild) {
  RunOne("Limited query doesn't pull in out of range child");
}

TEST_F(SyncTreeTest, MergerForLocationWithDefaultAndLimitedListener) {
  RunOne("Merge for location with default and limited listener");
}

TEST_F(SyncTreeTest, UserMergePullsInCorrectValues) {
  RunOne("User merge pulls in correct values");
}

TEST_F(SyncTreeTest, UserDeepSetPullsInCorrectValues) {
  RunOne("User deep set pulls in correct values");
}

TEST_F(SyncTreeTest, QueriesWithEqualToNullWork) {
  RunOne("Queries with equalTo(null) work");
}

TEST_F(SyncTreeTest, RevertedWritesUpdateQuery) {
  RunOne("Reverted writes update query");
}

TEST_F(SyncTreeTest, DeepSetForNonLocalDataDoesntRaiseEvents) {
  RunOne("Deep set for non-local data doesn't raise events");
}

TEST_F(SyncTreeTest, UserUpdateWithNewChildrenTriggersEvents) {
  RunOne("User update with new children triggers events");
}

TEST_F(SyncTreeTest, UserWriteWithDeepOverwrite) {
  RunOne("User write with deep user overwrite");
}

TEST_F(SyncTreeTest, DeepServerMerge) { RunOne("Deep server merge"); }

TEST_F(SyncTreeTest, ServerUpdatesPriority) {
  RunOne("Server updates priority");
}

TEST_F(SyncTreeTest, RevertFullUnderlyingWrite) {
  RunOne("Revert underlying full overwrite");
}

TEST_F(SyncTreeTest, UserChildOverwriteForNonexistentServerNode) {
  RunOne("User child overwrite for non-existent server node");
}

TEST_F(SyncTreeTest, RevertUserOverwriteOfChildOnLeafNode) {
  RunOne("Revert user overwrite of child on leaf node");
}

TEST_F(SyncTreeTest, ServerOverwriteWithDeepUserDelete) {
  RunOne("Server overwrite with deep user delete");
}

TEST_F(SyncTreeTest, UserOverwritesLeafNodeWithPriority) {
  RunOne("User overwrites leaf node with priority");
}

TEST_F(SyncTreeTest, UserOverwritesInheritPriorityValuesFromLeafNodes) {
  RunOne("User overwrites inherit priority values from leaf nodes");
}

TEST_F(SyncTreeTest, UserUpdateOnUserSetLeafNodeWithPriorityAfterServerUpdate) {
  RunOne("User update on user set leaf node with priority after server update");
}

TEST_F(SyncTreeTest, ServerDeepDeleteOnLeafNode) {
  RunOne("Server deep delete on leaf node");
}

TEST_F(SyncTreeTest, UserSetsRootPriority) {
  RunOne("User sets root priority");
}

TEST_F(SyncTreeTest, UserUpdatesPriorityOnEmptyRoot) {
  RunOne("User updates priority on empty root");
}

TEST_F(SyncTreeTest, RevertSetAtRootWithPriority) {
  RunOne("Revert set at root with priority");
}

TEST_F(SyncTreeTest, ServerUpdatesPriorityAfterUserSetsPriority) {
  RunOne("Server updates priority after user sets priority");
}

TEST_F(SyncTreeTest, EmptySetDoesntPreventServerUpdates) {
  RunOne("Empty set doesn't prevent server updates");
}

TEST_F(SyncTreeTest, UserUpdatesPriorityTwiceFirstIsReverted) {
  RunOne("User updates priority twice, first is reverted");
}

TEST_F(SyncTreeTest, ServerAcksRootPrioritySetAfterUserDeletesRootNode) {
  RunOne("Server acks root priority set after user deletes root node");
}

TEST_F(SyncTreeTest, ADeleteInAMergeDoesntPushOutNodes) {
  RunOne("A delete in a merge doesn't push out nodes");
}

TEST_F(SyncTreeTest, ATaggedQueryFiresEventsEventually) {
  RunOne("A tagged query fires events eventually");
}

TEST_F(SyncTreeTest, AServerUpdateThatLeavesUserSetsUnchangedIsNotIgnored) {
  RunOne("A server update that leaves user sets unchanged is not ignored");
}

TEST_F(SyncTreeTest, UserWriteOutsideOfLimitIsIgnoredForTaggedQueries) {
  RunOne("User write outside of limit is ignored for tagged queries");
}

TEST_F(SyncTreeTest, AckForMergeDoesntRaiseValueEventForLaterListen) {
  RunOne("Ack for merge doesn't raise value event for later listen");
}

TEST_F(SyncTreeTest, ClearParentShadowingServerValuesMergeWithServerChildren) {
  RunOne("Clear parent shadowing server values merge with server children");
}

TEST_F(SyncTreeTest, PrioritiesDontMakeMeSick) {
  RunOne("Priorities don't make me sick");
}

TEST_F(SyncTreeTest,
       MergeThatMovesChildFromWindowToBoundaryDoesNotCauseChildToBeReadded) {
  RunOne(
      "Merge that moves child from window to boundary does not cause child "
      "to be readded");
}

TEST_F(SyncTreeTest, DeepMergeAckIsHandledCorrectly) {
  RunOne("Deep merge ack is handled correctly.");
}

TEST_F(SyncTreeTest, DeepMergeAckOnIncompleteDataAndWithServerValues) {
  RunOne("Deep merge ack (on incomplete data, and with server values)");
}

TEST_F(SyncTreeTest, LimitQueryHandlesDeepServerMergeForOutOfViewItem) {
  RunOne("Limit query handles deep server merge for out-of-view item.");
}

TEST_F(SyncTreeTest, LimitQueryHandlesDeepUserMergeForOutOfViewItem) {
  RunOne("Limit query handles deep user merge for out-of-view item.");
}

TEST_F(SyncTreeTest,
       LimitQueryHandlesDeepUserMergeForOutOfViewItemFollowedByServerUpdate) {
  RunOne(
      "Limit query handles deep user merge for out-of-view item followed by "
      "server update.");
}

TEST_F(SyncTreeTest, UnrelatedUntaggedUpdateIsNotCachedInTaggedListen) {
  RunOne("Unrelated, untagged update is not cached in tagged listen");
}

TEST_F(SyncTreeTest, UnrelatedAckedSetIsNotCachedInTaggedListen) {
  RunOne("Unrelated, acked set is not cached in tagged listen");
}

TEST_F(SyncTreeTest, UnrelatedAckedUpdateIsNotCachedInTaggedListen) {
  RunOne("Unrelated, acked update is not cached in tagged listen");
}

TEST_F(SyncTreeTest, DeepUpdateRaisesImmediateEventsOnlyIfHasCompleteData) {
  RunOne("Deep update raises immediate events only if has complete data");
}

TEST_F(SyncTreeTest, DeepUpdateReturnsMinimumDataRequired) {
  RunOne("Deep update returns minimum data required");
}

TEST_F(SyncTreeTest, DeepUpdateRaisesAllEvents) {
  RunOne("Deep update raises all events");
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
