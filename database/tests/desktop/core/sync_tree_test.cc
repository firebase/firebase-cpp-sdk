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

#include "database/src/desktop/core/sync_tree.h"

#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/child_event_registration.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/persistence/persistence_manager_interface.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/include/firebase/database/common.h"
#include "database/tests/desktop/test/mock_cache_policy.h"
#include "database/tests/desktop/test/mock_listen_provider.h"
#include "database/tests/desktop/test/mock_listener.h"
#include "database/tests/desktop/test/mock_persistence_manager.h"
#include "database/tests/desktop/test/mock_persistence_storage_engine.h"
#include "database/tests/desktop/test/mock_tracked_query_manager.h"
#include "database/tests/desktop/test/mock_write_tree.h"

using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::Test;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(SyncTree, Constructor) {
  UniquePtr<WriteTree> write_tree;
  UniquePtr<PersistenceManager> persistence_manager;
  UniquePtr<ListenProvider> listen_provider;
  SyncTree sync_tree(std::move(write_tree), std::move(persistence_manager),
                     std::move(listen_provider));

  // Just making sure this constructor doesn't crash or leak memory. No further
  // tests.
}

class SyncTreeTest : public Test {
 public:
  void SetUp() override {
    // These mocks are very noisy, so we make them NiceMocks and explicitly call
    // EXPECT_CALL when there are specific things we expect to have happen.
    UniquePtr<WriteTree> pending_write_tree_ptr(MakeUnique<WriteTree>());

    persistence_storage_engine_ = new NiceMock<MockPersistenceStorageEngine>();
    UniquePtr<MockPersistenceStorageEngine> storage_engine_ptr(
        persistence_storage_engine_);

    tracked_query_manager_ = new NiceMock<MockTrackedQueryManager>();
    UniquePtr<MockTrackedQueryManager> tracked_query_manager_ptr(
        tracked_query_manager_);

    cache_policy_ = new NiceMock<MockCachePolicy>();
    UniquePtr<MockCachePolicy> cache_policy_ptr(cache_policy_);

    persistence_manager_ = new NiceMock<MockPersistenceManager>(
        std::move(storage_engine_ptr), std::move(tracked_query_manager_ptr),
        std::move(cache_policy_ptr), &logger_);
    UniquePtr<MockPersistenceManager> persistence_manager_ptr(
        persistence_manager_);

    listen_provider_ = new NiceMock<MockListenProvider>();
    UniquePtr<MockListenProvider> listen_provider_ptr(listen_provider_);

    sync_tree_ = new SyncTree(std::move(pending_write_tree_ptr),
                              std::move(persistence_manager_ptr),
                              std::move(listen_provider_ptr));
  }

  void TearDown() override { delete sync_tree_; }

 protected:
  // We keep a local copy of these pointers so that we can do expectation
  // testing on them. The SyncTree (or the classes SyncTree owns) own these
  // pointers though so we let them handle the cleanup.
  MockWriteTree* pending_write_tree_;
  MockPersistenceStorageEngine* persistence_storage_engine_;
  MockTrackedQueryManager* tracked_query_manager_;
  MockCachePolicy* cache_policy_;
  SystemLogger logger_;
  MockPersistenceManager* persistence_manager_;
  MockListenProvider* listen_provider_;

  SyncTree* sync_tree_;
};

using SyncTreeDeathTest = SyncTreeTest;

TEST_F(SyncTreeTest, AddEventRegistration) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  EXPECT_TRUE(sync_tree_->IsEmpty());
  EXPECT_CALL(*persistence_manager_, SetQueryActive(query_spec));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));
  EXPECT_FALSE(sync_tree_->IsEmpty());
}

TEST_F(SyncTreeTest, ApplyListenComplete) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  CacheNode initial_cache(IndexedVariant(Variant(), query_spec.params), true,
                          false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Applying a ListenComplete should tell the PersistenceManager that listening
  // on the given query is complete.
  EXPECT_CALL(*persistence_manager_, SetQueryComplete(query_spec));
  std::vector<Event> results = sync_tree_->ApplyListenComplete(path);
  EXPECT_EQ(results, std::vector<Event>{});
}

TEST_F(SyncTreeTest, ApplyServerMerge) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  std::map<Path, Variant> changed_children{
      std::make_pair(Path("fruit/apple"), "green"),
      std::make_pair(Path("fruit/banana"), "yellow"),
  };

  // Apply the merge and get the results.
  std::vector<Event> results =
      sync_tree_->ApplyServerMerge(path, changed_children);
  std::vector<Event> expected_results{
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                       std::make_pair("currant", "black"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);
}

TEST_F(SyncTreeTest, ApplyServerOverwrite) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  std::map<Variant, Variant> changed_children{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "green"),
                         std::make_pair("banana", "yellow"),
                     }),
  };

  // Apply the override and get the results.
  std::vector<Event> results =
      sync_tree_->ApplyServerOverwrite(path, changed_children);
  std::vector<Event> expected_results{
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);
}

TEST_F(SyncTreeTest, ApplyUserMerge) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  CompoundWrite unresolved_children =
      CompoundWrite::FromPathMerge(std::map<Path, Variant>{
          std::make_pair(Path("fruit/apple"), "green"),
          std::make_pair(Path("fruit/banana"), "yellow"),
      });
  // Resolved/unresolved children refer to special server values, timestamp
  // specifically, which we don't support right now.
  CompoundWrite children = unresolved_children;
  WriteId write_id = 100;

  // Verify the values get persisted locally.
  EXPECT_CALL(*persistence_manager_,
              SaveUserMerge(path, unresolved_children, write_id));

  // Apply the user merge and get the results.
  std::vector<Event> results = sync_tree_->ApplyUserMerge(
      path, unresolved_children, children, write_id, kPersist);
  std::vector<Event> expected_results{
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                       std::make_pair("currant", "black"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);
}

TEST_F(SyncTreeTest, ApplyUserOverwrite) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  Variant unresolved_new_data(std::map<Variant, Variant>{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "green"),
                         std::make_pair("banana", "yellow"),
                     }),
  });
  // Resolved/unresolved children refer to special server values, timestamp
  // specifically, which we don't support right now.
  Variant new_data = unresolved_new_data;
  WriteId write_id = 200;

  // Verify the values get persisted locally.
  EXPECT_CALL(*persistence_manager_,
              SaveUserOverwrite(path, unresolved_new_data, write_id));

  // Apply the user merge and get the results.
  std::vector<Event> results =
      sync_tree_->ApplyUserOverwrite(path, unresolved_new_data, new_data,
                                     write_id, kOverwriteVisible, kPersist);
  std::vector<Event> expected_results{
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);
}

TEST_F(SyncTreeTest, AckUserWrite) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  Variant unresolved_new_data(std::map<Variant, Variant>{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "green"),
                         std::make_pair("banana", "yellow"),
                     }),
  });
  // Resolved/unresolved children refer to special server values, timestamp
  // specifically, which we don't support right now.
  Variant new_data = unresolved_new_data;
  WriteId write_id = 200;

  // Verify the values get persisted locally.
  EXPECT_CALL(*persistence_manager_,
              SaveUserOverwrite(path, unresolved_new_data, write_id));

  std::vector<Event> results;
  std::vector<Event> expected_results;
  // Apply the user merge and get the results.
  results =
      sync_tree_->ApplyUserOverwrite(path, unresolved_new_data, new_data,
                                     write_id, kOverwriteVisible, kPersist);
  expected_results = {
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);

  expected_results = {
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "red"),
                                       std::make_pair("currant", "black"),
                                   }),
                }),
                QuerySpec(path))),
  };
  results = sync_tree_->AckUserWrite(write_id, kAckConfirm, kPersist, 0);
  EXPECT_EQ(results, expected_results);
}

TEST_F(SyncTreeTest, AckUserWriteRevert) {
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  Variant unresolved_new_data(std::map<Variant, Variant>{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "green"),
                         std::make_pair("banana", "yellow"),
                     }),
  });
  // Resolved/unresolved children refer to special server values, timestamp
  // specifically, which we don't support right now.
  Variant new_data = unresolved_new_data;
  WriteId write_id = 200;

  // Verify the values get persisted locally.
  EXPECT_CALL(*persistence_manager_,
              SaveUserOverwrite(path, unresolved_new_data, write_id));

  std::vector<Event> results;
  std::vector<Event> expected_results;
  // Apply the user merge and get the results.
  results =
      sync_tree_->ApplyUserOverwrite(path, unresolved_new_data, new_data,
                                     write_id, kOverwriteVisible, kPersist);
  expected_results = {
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);

  expected_results = {
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "red"),
                                       std::make_pair("currant", "black"),
                                   }),
                }),
                QuerySpec(path))),
  };
  results = sync_tree_->AckUserWrite(write_id, kAckRevert, kPersist, 0);
  EXPECT_EQ(results, expected_results);
}

TEST_F(SyncTreeTest, RemoveAllWrites) {
  // This starts off the same as the ApplyUserOverwrite test, but then
  // afterward.
  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the
  // PersistenceManager, but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager_, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  // Change one element in the database, and add one new one.
  Variant unresolved_new_data(std::map<Variant, Variant>{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "green"),
                         std::make_pair("banana", "yellow"),
                     }),
  });
  // Resolved/unresolved children refer to special server values, timestamp
  // specifically, which we don't support right now.
  Variant new_data = unresolved_new_data;
  WriteId write_id = 200;

  // Verify the values get persisted locally.
  EXPECT_CALL(*persistence_manager_,
              SaveUserOverwrite(path, unresolved_new_data, write_id));

  // Apply the user merge and get the results.
  std::vector<Event> results =
      sync_tree_->ApplyUserOverwrite(path, unresolved_new_data, new_data,
                                     write_id, kOverwriteVisible, kPersist);
  std::vector<Event> expected_results{
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "green"),
                                       std::make_pair("banana", "yellow"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(results, expected_results);

  // We now have a pending write to undo. Verify we get the right events.
  EXPECT_CALL(*persistence_manager_, RemoveAllUserWrites());
  std::vector<Event> remove_results = sync_tree_->RemoveAllWrites();
  std::vector<Event> expected_remove_results{
      Event(kEventTypeValue, event_registration,
            DataSnapshotInternal(
                nullptr,
                Variant(std::map<Variant, Variant>{
                    std::make_pair("fruit",
                                   std::map<Variant, Variant>{
                                       std::make_pair("apple", "red"),
                                       std::make_pair("currant", "black"),
                                   }),
                }),
                QuerySpec(path))),
  };
  EXPECT_EQ(remove_results, expected_remove_results);
}

TEST_F(SyncTreeTest, RemoveAllEventRegistrations) {
  QueryParams loads_all_data;
  QueryParams does_not_load_all_data;
  does_not_load_all_data.limit_first = 10;
  QuerySpec query_spec1(Path("aaa/bbb/ccc"), loads_all_data);
  // Two QuerySpecs at same location but different parameters.
  QuerySpec query_spec2(Path("aaa/bbb/ccc"), does_not_load_all_data);
  // Shadowing QuerySpec at higher location .
  QuerySpec query_spec3(Path("aaa"), loads_all_data);
  // QuerySpec in a totally different area of the tree.
  QuerySpec query_spec4(Path("ddd/eee/fff"), does_not_load_all_data);
  MockValueListener listener1;
  MockChildListener listener2;
  MockValueListener listener3;
  MockChildListener listener4;
  ValueEventRegistration* event_registration1 =
      new ValueEventRegistration(nullptr, &listener1, query_spec1);
  ChildEventRegistration* event_registration2 =
      new ChildEventRegistration(nullptr, &listener2, query_spec2);
  ValueEventRegistration* event_registration3 =
      new ValueEventRegistration(nullptr, &listener3, query_spec3);
  ChildEventRegistration* event_registration4 =
      new ChildEventRegistration(nullptr, &listener4, query_spec4);
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration1));
  sync_tree_->AddEventRegistration(
      UniquePtr<ChildEventRegistration>(event_registration2));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration3));
  sync_tree_->AddEventRegistration(
      UniquePtr<ChildEventRegistration>(event_registration4));

  std::vector<Event> results;
  // This will not cause any calls to StopListening because the listener
  // is listening on aaa and redirecting changes to this location internally.
  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec1)).Times(2);
  results = sync_tree_->RemoveAllEventRegistrations(query_spec1, kErrorNone);
  EXPECT_EQ(results, std::vector<Event>{});

  // This will cause the ListenProvider to stop listening on aaa because it is
  // the rootmost listener on this location.
  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec3));
  EXPECT_CALL(*listen_provider_, StopListening(query_spec3, Tag()));
  results = sync_tree_->RemoveAllEventRegistrations(query_spec3, kErrorNone);
  EXPECT_EQ(results, std::vector<Event>{});

  // In the case of an error, no explicit call to StopListening is made. This
  // is expected. However, we will stop tracking the query.
  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec4));
  results =
      sync_tree_->RemoveAllEventRegistrations(query_spec4, kErrorExpiredToken);

  // I have to manually construct this because normally building an 'error'
  // event requres that I pass in a UniquePtr.
  Event expected_event;
  expected_event.type = kEventTypeError;
  expected_event.event_registration = event_registration4;
  expected_event.snapshot = Optional<DataSnapshotInternal>();
  expected_event.error = kErrorExpiredToken;
  expected_event.path = Path("ddd/eee/fff");
  EXPECT_EQ(results, std::vector<Event>{expected_event});
}

TEST_F(SyncTreeTest, RemoveEventRegistration) {
  QueryParams loads_all_data;
  QueryParams does_not_load_all_data;
  does_not_load_all_data.limit_first = 10;
  QuerySpec query_spec1(Path("aaa/bbb/ccc"), loads_all_data);
  // Two QuerySpecs at same location but different parameters.
  QuerySpec query_spec2(Path("aaa/bbb/ccc"), does_not_load_all_data);
  // Shadowing QuerySpec at higher location .
  QuerySpec query_spec3(Path("aaa"), loads_all_data);
  // QuerySpec in a totally different area of the tree.
  QuerySpec query_spec4(Path("ddd/eee/fff"), does_not_load_all_data);
  MockValueListener listener1;
  MockChildListener listener2;
  MockValueListener listener3;
  MockChildListener listener4;
  MockValueListener unassigned_listener;
  ValueEventRegistration* event_registration1 =
      new ValueEventRegistration(nullptr, &listener1, query_spec1);
  ChildEventRegistration* event_registration2 =
      new ChildEventRegistration(nullptr, &listener2, query_spec2);
  ValueEventRegistration* event_registration3 =
      new ValueEventRegistration(nullptr, &listener3, query_spec3);
  ChildEventRegistration* event_registration4 =
      new ChildEventRegistration(nullptr, &listener4, query_spec4);
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration1));
  sync_tree_->AddEventRegistration(
      UniquePtr<ChildEventRegistration>(event_registration2));
  sync_tree_->AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration3));
  sync_tree_->AddEventRegistration(
      UniquePtr<ChildEventRegistration>(event_registration4));

  std::vector<Event> results;
  // This will not cause any calls to StopListening because the listener
  // is listening on aaa and redirecting changes to this location internally.
  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec1)).Times(2);
  results =
      sync_tree_->RemoveEventRegistration(query_spec1, &listener1, kErrorNone);
  EXPECT_EQ(results, std::vector<Event>{});
  results =
      sync_tree_->RemoveEventRegistration(query_spec1, &listener2, kErrorNone);
  EXPECT_EQ(results, std::vector<Event>{});

  // Expect nothing to happen.
  results = sync_tree_->RemoveEventRegistration(
      query_spec1, &unassigned_listener, kErrorNone);
  EXPECT_EQ(results, std::vector<Event>{});

  // This will cause the ListenProvider to stop listening on aaa because it is
  // the rootmost listener on this location.
  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec3));
  EXPECT_CALL(*listen_provider_, StopListening(query_spec3, Tag()));
  results =
      sync_tree_->RemoveEventRegistration(query_spec3, &listener3, kErrorNone);
  EXPECT_EQ(results, std::vector<Event>{});

  // In the case of an error, no explicit call to StopListening is made. This
  // is expected. However, we will stop tracking the query.
  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec4));
  results = sync_tree_->RemoveEventRegistration(query_spec4, nullptr,
                                                kErrorExpiredToken);

  // I have to manually construct this because normally constructing an 'error'
  // event requres that I pass in a UniquePtr.
  Event expected_event;
  expected_event.type = kEventTypeError;
  expected_event.event_registration = event_registration4;
  expected_event.snapshot = Optional<DataSnapshotInternal>();
  expected_event.error = kErrorExpiredToken;
  expected_event.path = Path("ddd/eee/fff");
  EXPECT_EQ(results, std::vector<Event>{expected_event});
}

TEST_F(SyncTreeDeathTest, RemoveEventRegistration) {
  QuerySpec query_spec(Path("i/am/become/death"));
  MockChildListener listener;
  ChildEventRegistration* event_registration =
      new ChildEventRegistration(nullptr, &listener, query_spec);
  sync_tree_->AddEventRegistration(
      UniquePtr<ChildEventRegistration>(event_registration));
  EXPECT_DEATH(sync_tree_->RemoveEventRegistration(query_spec, &listener,
                                                   kErrorExpiredToken),
               DEATHTEST_SIGABRT);
}

TEST(SyncTree, CalcCompleteEventCache) {
  // For this test we set up our own sync tree instead of using the premade test
  // harness because we need a mock write tree instead of a functional one to
  // run this test.
  //
  // TODO(amablue): retrofit the other tests to function with a MockWriteTree by
  // filling in the expected values to calls to the write tree.
  SystemLogger logger;
  MockWriteTree* pending_write_tree = new NiceMock<MockWriteTree>();
  UniquePtr<MockWriteTree> pending_write_tree_ptr(pending_write_tree);
  MockPersistenceManager* persistence_manager =
      new NiceMock<MockPersistenceManager>(
          MakeUnique<NiceMock<MockPersistenceStorageEngine>>(),
          MakeUnique<NiceMock<MockTrackedQueryManager>>(),
          MakeUnique<NiceMock<MockCachePolicy>>(), &logger);
  UniquePtr<MockPersistenceManager> persistence_manager_ptr(
      persistence_manager);
  SyncTree sync_tree(std::move(pending_write_tree_ptr),
                     std::move(persistence_manager_ptr),
                     MakeUnique<NiceMock<MockListenProvider>>());

  Path path("aaa/bbb/ccc");
  QuerySpec query_spec(path);
  MockValueListener listener;
  ValueEventRegistration* event_registration =
      new ValueEventRegistration(nullptr, &listener, query_spec);

  // The initial cache node would normally be set up by the PersistenceManager,
  // but we're mocking it so we set it up manually.
  std::map<Variant, Variant> initial_variant{
      std::make_pair("fruit",
                     std::map<Variant, Variant>{
                         std::make_pair("apple", "red"),
                         std::make_pair("currant", "black"),
                     }),
  };
  CacheNode initial_cache(IndexedVariant(initial_variant, query_spec.params),
                          true, false);
  EXPECT_CALL(*persistence_manager, ServerCache(query_spec))
      .WillOnce(Return(initial_cache));

  sync_tree.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(event_registration));

  std::vector<WriteId> write_ids_to_exclude{1, 2, 3, 4};
  Variant expected_server_cache(std::map<Variant, Variant>{
      std::make_pair("apple", "red"),
      std::make_pair("currant", "black"),
  });
  EXPECT_CALL(*pending_write_tree,
              CalcCompleteEventCache(
                  Path("aaa/bbb/ccc/fruit"), Pointee(expected_server_cache),
                  write_ids_to_exclude, kIncludeHiddenWrites));
  sync_tree.CalcCompleteEventCache(Path("aaa/bbb/ccc/fruit"),
                                   write_ids_to_exclude);
}

TEST_F(SyncTreeTest, SetKeepSynchronized) {
  QuerySpec query_spec1(Path("aaa/bbb/ccc"));
  QuerySpec query_spec2(Path("aaa/bbb/ccc/ddd"));

  EXPECT_CALL(*persistence_manager_, SetQueryActive(query_spec1));
  sync_tree_->SetKeepSynchronized(query_spec1, true);

  EXPECT_CALL(*persistence_manager_, SetQueryActive(query_spec2));
  sync_tree_->SetKeepSynchronized(query_spec2, true);

  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec1));
  sync_tree_->SetKeepSynchronized(query_spec1, false);

  EXPECT_CALL(*persistence_manager_, SetQueryInactive(query_spec2));
  sync_tree_->SetKeepSynchronized(query_spec2, false);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
