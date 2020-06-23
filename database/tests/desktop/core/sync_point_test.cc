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

#include "database/src/desktop/core/sync_point.h"

#include "app/src/include/firebase/variant.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/cache_policy.h"
#include "database/src/desktop/core/child_event_registration.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/persistence/persistence_manager_interface.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/listener.h"
#include "database/tests/desktop/test/matchers.h"
#include "database/tests/desktop/test/mock_cache_policy.h"
#include "database/tests/desktop/test/mock_listener.h"
#include "database/tests/desktop/test/mock_persistence_manager.h"
#include "database/tests/desktop/test/mock_persistence_storage_engine.h"
#include "database/tests/desktop/test/mock_tracked_query_manager.h"

using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(SyncPoint, IsEmpty) {
  SyncPoint sync_point;
  EXPECT_TRUE(sync_point.IsEmpty());
}

class SyncPointTest : public ::testing::Test {
 public:
  SyncPointTest()
      : logger_(),
        sync_point_(),
        persistence_manager_(MakeUnique<MockPersistenceStorageEngine>(),
                             MakeUnique<MockTrackedQueryManager>(),
                             MakeUnique<MockCachePolicy>(), &logger_) {}

 protected:
  SystemLogger logger_;
  SyncPoint sync_point_;
  NiceMock<MockPersistenceManager> persistence_manager_;
};

TEST_F(SyncPointTest, IsNotEmpty) {
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref(Path(), &writes_cache);
  CacheNode server_cache;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());

  sync_point_.AddEventRegistration(
      UniquePtr<EventRegistration>(event_registration), writes_cache_ref,
      server_cache, &persistence_manager_);

  EXPECT_FALSE(sync_point_.IsEmpty());
}

TEST_F(SyncPointTest, ApplyOperation) {
  Operation operation;
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref(Path(), &writes_cache);
  Variant complete_server_cache;

  std::vector<Event> results =
      sync_point_.ApplyOperation(operation, writes_cache_ref,
                                 &complete_server_cache, &persistence_manager_);

  std::vector<Event> expected_results;

  EXPECT_THAT(results, Pointwise(Eq(), expected_results));
}

TEST_F(SyncPointTest, AddEventRegistration) {
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref(Path(), &writes_cache);
  CacheNode server_cache;
  // Give the EventRegistrations different QueryParams so that they get placed
  // in different Views.
  Path path("a/b/c");
  QueryParams value_params;
  value_params.end_at_value = 222;
  QuerySpec value_spec(path, value_params);
  QueryParams child_params;
  child_params.start_at_value = 111;
  QuerySpec child_spec(path, child_params);
  ValueEventRegistration* value_event_registration =
      new ValueEventRegistration(nullptr, nullptr, value_spec);
  ChildEventRegistration* child_event_registration =
      new ChildEventRegistration(nullptr, nullptr, child_spec);

  std::vector<Event> value_events = sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(value_event_registration),
      writes_cache_ref, server_cache, &persistence_manager_);
  std::vector<Event> child_events = sync_point_.AddEventRegistration(
      UniquePtr<ChildEventRegistration>(child_event_registration),
      writes_cache_ref, server_cache, &persistence_manager_);

  std::vector<const View*> view_results = sync_point_.GetIncompleteQueryViews();

  std::vector<Event> expected_value_events;
  std::vector<Event> expected_child_events;

  EXPECT_THAT(value_events, Pointwise(Eq(), expected_value_events));
  EXPECT_THAT(child_events, Pointwise(Eq(), expected_child_events));

  // Local cache gets updated to the values it expects the server to reflect
  // eventually.

  CacheNode expected_value_local_cache(
      IndexedVariant(Variant::Null(), value_spec.params), false, true);
  CacheNode expected_child_local_cache(
      IndexedVariant(Variant::Null(), child_spec.params), false, true);
  CacheNode expected_server_cache = server_cache;

  EXPECT_EQ(view_results.size(), 2);
  EXPECT_EQ(view_results[0]->query_spec(), value_spec);

  EXPECT_EQ(view_results[0]->view_cache(),
            ViewCache(expected_value_local_cache, expected_server_cache));

  EXPECT_THAT(view_results[0]->event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {value_event_registration}));

  EXPECT_EQ(view_results[1]->query_spec(), child_spec);
  EXPECT_EQ(view_results[1]->view_cache(),
            ViewCache(expected_child_local_cache, expected_server_cache));
  EXPECT_THAT(view_results[1]->event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {child_event_registration}));
}

TEST_F(SyncPointTest, RemoveEventRegistration_FromCompleteView) {
  Path path("a/b/c");
  // Give the EventRegistrations different QueryParams, but neither one filters,
  // so they'll get placed in the same View.
  QueryParams query_params;
  query_params.order_by = QueryParams::kOrderByChild;
  query_params.order_by_child = "Phillip";
  QuerySpec query_spec(path, query_params);

  QueryParams another_query_params;
  another_query_params.order_by = QueryParams::kOrderByChild;
  another_query_params.order_by_child = "Lillian";
  QuerySpec another_query_spec(path, another_query_params);

  CacheNode server_cache(IndexedVariant(Variant(), query_spec.params), false,
                         false);

  MockValueListener listener;
  MockValueListener another_listener;
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref(Path(), &writes_cache);

  ValueEventRegistration* value_event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);
  ValueEventRegistration* another_value_event_registration =
      new ValueEventRegistration(nullptr, &another_listener,
                                 another_query_spec);

  // Add some EventRegistrations...
  sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(value_event_registration),
      writes_cache_ref, server_cache, &persistence_manager_);
  sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(another_value_event_registration),
      writes_cache_ref, server_cache, &persistence_manager_);

  // ...And then remove one of them.
  std::vector<QuerySpec> removed_specs;
  sync_point_.RemoveEventRegistration(another_query_spec, &another_listener,
                                      kErrorNone, &removed_specs);

  // There should be no incomplete views.
  std::vector<const View*> view_results = sync_point_.GetIncompleteQueryViews();
  EXPECT_EQ(view_results.size(), 0);

  // We expect that the local cache will get updated to the values that the
  // server will eventually have.
  CacheNode expected_local_cache(
      IndexedVariant(Variant::Null(), query_spec.params), false, false);
  CacheNode expected_server_cache = server_cache;
  ViewCache expected_view_cache(expected_local_cache, expected_server_cache);

  // No QuerySpecs were removed, because there is only one Complete QuerySpec.
  EXPECT_THAT(removed_specs, Pointwise(Eq(), std::vector<QuerySpec>{}));

  // Verify that the correct view remains.
  const View* view = sync_point_.GetCompleteView();
  EXPECT_EQ(view->query_spec(), query_spec);
  EXPECT_EQ(view->view_cache(), expected_view_cache);
  EXPECT_THAT(view->event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {value_event_registration}));
}

TEST_F(SyncPointTest, RemoveEventRegistration_FromIncompleteView) {
  Path path("a/b/c");
  // Give the EventRegistrations different QueryParams so that they get placed
  // in different Views.
  QueryParams query_params;
  query_params.end_at_value = 222;
  QuerySpec query_spec(path, query_params);

  QueryParams another_query_params;
  another_query_params.start_at_value = 111;
  QuerySpec another_query_spec(path, another_query_params);

  CacheNode server_cache(IndexedVariant(Variant(), query_params), false, false);

  MockValueListener listener;
  MockValueListener another_listener;
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref(Path(), &writes_cache);

  ValueEventRegistration* value_event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);
  ValueEventRegistration* another_value_event_registration =
      new ValueEventRegistration(nullptr, &another_listener,
                                 another_query_spec);

  // Add some EventRegistrations...
  sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(value_event_registration),
      writes_cache_ref, server_cache, &persistence_manager_);
  sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(another_value_event_registration),
      writes_cache_ref, server_cache, &persistence_manager_);

  // ...And then remove one of them.
  std::vector<QuerySpec> removed_specs;
  sync_point_.RemoveEventRegistration(another_query_spec, &another_listener,
                                      kErrorNone, &removed_specs);

  // There should be one incomplete view remaining.
  std::vector<const View*> view_results = sync_point_.GetIncompleteQueryViews();
  EXPECT_EQ(view_results.size(), 1);

  // We expect that the local cache will get updated to the values that the
  // server will eventually have.
  CacheNode expected_local_cache(IndexedVariant(Variant::Null(), query_params),
                                 false, true);
  CacheNode expected_server_cache = server_cache;
  ViewCache expected_view_cache(expected_local_cache, expected_server_cache);

  // Check that the correct QuerySpecs were removed.
  EXPECT_THAT(removed_specs, Pointwise(Eq(), {another_query_spec}));

  // Verify that the correct view remain.
  const View* view = view_results[0];
  EXPECT_EQ(view->query_spec(), query_spec);
  EXPECT_EQ(view->view_cache(), expected_view_cache);
  EXPECT_THAT(view->event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {value_event_registration}));
}

TEST_F(SyncPointTest, GetCompleteServerCache) {
  Path path;

  EXPECT_EQ(sync_point_.GetCompleteServerCache(path), nullptr);
  EXPECT_FALSE(sync_point_.HasCompleteView());

  // No filtering.
  QueryParams apples_query_params;
  QuerySpec apples_query_spec(path, apples_query_params);

  // Filtering
  QueryParams bananas_query_params;
  bananas_query_params.start_at_value = 111;
  QuerySpec bananas_query_spec(path, bananas_query_params);

  CacheNode apples_server_cache(
      IndexedVariant(Variant("Apples"), apples_query_params), true, false);
  CacheNode bananas_server_cache(
      IndexedVariant(Variant("Bananas"), bananas_query_params), true, false);

  MockValueListener apples_listener;
  MockValueListener bananas_listener;
  WriteTree writes_cache;
  WriteTreeRef writes_cache_ref(Path(), &writes_cache);

  ValueEventRegistration* apples_event_registration =
      new ValueEventRegistration(nullptr, &apples_listener, apples_query_spec);
  ValueEventRegistration* bananas_event_registration =
      new ValueEventRegistration(nullptr, &bananas_listener,
                                 bananas_query_spec);

  sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(apples_event_registration),
      writes_cache_ref, apples_server_cache, &persistence_manager_);
  sync_point_.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(bananas_event_registration),
      writes_cache_ref, bananas_server_cache, &persistence_manager_);

  QueryParams carrots_query_params;
  carrots_query_params.equal_to_value = "Carrots";
  QuerySpec carrots_query_spec(path, carrots_query_params);
  EXPECT_TRUE(sync_point_.ViewExistsForQuery(apples_query_spec));
  EXPECT_TRUE(sync_point_.ViewExistsForQuery(bananas_query_spec));
  EXPECT_FALSE(sync_point_.ViewExistsForQuery(carrots_query_spec));

  const View* apples_view = sync_point_.ViewForQuery(apples_query_spec);
  const View* bananas_view = sync_point_.ViewForQuery(bananas_query_spec);
  const View* carrots_view = sync_point_.ViewForQuery(carrots_query_spec);
  EXPECT_EQ(apples_view->view_cache().server_snap(), apples_server_cache);
  EXPECT_EQ(bananas_view->view_cache().server_snap(), bananas_server_cache);
  EXPECT_EQ(carrots_view, nullptr);

  EXPECT_EQ(*sync_point_.GetCompleteServerCache(path), Variant("Apples"));
  EXPECT_TRUE(sync_point_.HasCompleteView());
}

TEST_F(SyncPointTest, GetCompleteView_FromQuerySpecThatLoadsAllData) {
  WriteTree write_tree;
  WriteTreeRef write_tree_ref(Path(), &write_tree);
  Path path;

  // Values to feed to AddEventRegistration that will result in a "complete"
  // View, i.e. a view with no filtering (ordering is okay)
  QueryParams good_params;
  good_params.order_by = QueryParams::kOrderByChild;
  good_params.order_by_child = "Bob";
  QuerySpec good_spec(path, good_params);
  CacheNode good_server_cache(IndexedVariant(Variant("good"), good_params),
                              true, true);
  sync_point_.AddEventRegistration(
      MakeUnique<ValueEventRegistration>(nullptr, nullptr, good_spec),
      write_tree_ref, good_server_cache, &persistence_manager_);

  // Values to feed to AddEventRegistration that will not result in an
  // incomplete View, i.e. a view with some filters on it. This should not be
  // returned when we ask for the complete view.
  QueryParams bad_params;
  bad_params.limit_first = 10;
  QuerySpec bad_spec(path, bad_params);
  CacheNode incorrect_server_cache(IndexedVariant(Variant("bad"), bad_params),
                                   true, true);
  sync_point_.AddEventRegistration(
      MakeUnique<ValueEventRegistration>(nullptr, nullptr, bad_spec),
      write_tree_ref, incorrect_server_cache, &persistence_manager_);

  const View* result = sync_point_.GetCompleteView();
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result->query_spec(), good_spec);
  EXPECT_EQ(result->GetLocalCache(), "good");
}

TEST_F(SyncPointTest, GetCompleteView_FromQuerySpecThatDoesNotLoadsAllData) {
  WriteTree write_tree;
  WriteTreeRef write_tree_ref(Path(), &write_tree);
  Path path;

  // Values to feed to AddEventRegistration that will not result in an
  // incomplete View, i.e. a view with some filters on it. This should not be
  // retuened when we ask for the complete view.
  QueryParams bad_params;
  bad_params.limit_first = 10;
  QuerySpec bad_spec(path, bad_params);
  CacheNode incorrect_server_cache(IndexedVariant(Variant("bad"), bad_params),
                                   true, true);
  sync_point_.AddEventRegistration(
      MakeUnique<ValueEventRegistration>(nullptr, nullptr, bad_spec),
      write_tree_ref, incorrect_server_cache, &persistence_manager_);

  EXPECT_EQ(sync_point_.GetCompleteView(), nullptr);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
