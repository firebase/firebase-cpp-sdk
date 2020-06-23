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

#include "database/src/desktop/view/view.h"

#include "app/memory/unique_ptr.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/view/view_cache.h"
#include "database/src/include/firebase/database/common.h"
#include "database/tests/desktop/test/matchers.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;
using ::testing::Not;
using ::testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(View, Constructor) {
  QuerySpec query_spec;
  query_spec.path = Path("test/path");
  query_spec.params.order_by = QueryParams::kOrderByValue;
  query_spec.params.start_at_value = "Apple";
  CacheNode local_cache(IndexedVariant(Variant(), query_spec.params), true,
                        true);
  CacheNode server_cache(IndexedVariant(Variant(), query_spec.params), true,
                         false);
  ViewCache initial_view_cache(local_cache, server_cache);

  View view(query_spec, initial_view_cache);

  EXPECT_EQ(view.query_spec(), query_spec);
  EXPECT_EQ(view.view_cache(), initial_view_cache);
}

TEST(View, MoveConstructor) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  params.order_by_child = "order_by_child";
  QuerySpec query_spec(Path("test/path"), params);
  CacheNode cache(IndexedVariant(Variant("test"), query_spec.params), true,
                  false);
  ViewCache initial_view_cache(cache, cache);

  View old_view(query_spec, initial_view_cache);

  // Add an event registration to make sure that it gets moved to the new View.
  ValueEventRegistration* registration =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  // old_view now owns registration.
  old_view.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(registration));

  View new_view(std::move(old_view));

  // The old cache should have its event registrations cleared out. If the
  // registration was left behind in the old_view, this test will crash at the
  // end due to double-deleting the registration.

  // The new cache should be exactly what the old one was.
  EXPECT_EQ(new_view.query_spec(), query_spec);
  EXPECT_EQ(new_view.view_cache(), initial_view_cache);
  EXPECT_THAT(new_view.event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {registration}));
}

TEST(View, MoveAssignment) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  params.order_by_child = "order_by_child";
  QuerySpec query_spec(Path("test/path"), params);
  CacheNode cache(IndexedVariant(Variant("test"), params), true, false);
  ViewCache initial_view_cache(cache, cache);

  View old_view(query_spec, initial_view_cache);

  // Add an event registration to make sure that it gets moved to the new View.
  ValueEventRegistration* registration =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  // old_view now owns registration.
  old_view.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(registration));

  // When we move the old_view into the new_view, make sure any existing
  // registrations are properly cleaned up and not leaked.
  ValueEventRegistration* registration_to_be_deleted =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  View new_view((QuerySpec()), ViewCache(CacheNode(), CacheNode()));
  new_view.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(registration_to_be_deleted));

  new_view = std::move(old_view);

  // The old cache should have its event registrations cleared out. If the
  // registration was left behind in the old_view, this test will crash at the
  // end due to double-deleting the registration.

  // The new cache should be exactly what the old one was.
  EXPECT_EQ(new_view.query_spec(), query_spec);
  EXPECT_EQ(new_view.view_cache(), initial_view_cache);
  EXPECT_THAT(new_view.event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {registration}));
}

// For Views, copies are actually moves, so this test is identical to the
// MoveConstructor test.
TEST(View, CopyConstructor) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  params.order_by_child = "order_by_child";
  QuerySpec query_spec(Path("test/path"), params);
  CacheNode cache(IndexedVariant(Variant("test"), params), true, false);
  ViewCache initial_view_cache(cache, cache);

  View old_view(query_spec, initial_view_cache);

  // Add an event registration to make sure that it gets moved to the new View.
  ValueEventRegistration* registration =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  // old_view now owns registration.
  old_view.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(registration));

  View new_view(old_view);

  // The old cache should have its event registrations cleared out. If the
  // registration was left behind in the old_view, this test will crash at the
  // end due to double-deleting the registration.

  // The new cache should be exactly what the old one was.
  EXPECT_EQ(new_view.query_spec(), query_spec);
  EXPECT_EQ(new_view.view_cache(), initial_view_cache);
  EXPECT_THAT(new_view.event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {registration}));
}

// For Views, copies are actually moves, so this test is identical to the
// MoveAssignment test.
TEST(View, CopyAssignment) {
  QueryParams params;
  params.order_by = QueryParams::kOrderByChild;
  params.order_by_child = "order_by_child";
  QuerySpec query_spec(Path("test/path"), params);
  CacheNode cache(IndexedVariant(Variant("test"), params), true, false);
  ViewCache initial_view_cache(cache, cache);

  View old_view(query_spec, initial_view_cache);

  // Add an event registration to make sure that it gets moved to the new View.
  ValueEventRegistration* registration =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  // old_view now owns registration.
  old_view.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(registration));

  // When we move the old_view into the new_view, make sure any existing
  // registrations are properly cleaned up and not leaked.
  ValueEventRegistration* registration_to_be_deleted =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  View new_view((QuerySpec()), ViewCache(CacheNode(), CacheNode()));
  new_view.AddEventRegistration(
      UniquePtr<ValueEventRegistration>(registration_to_be_deleted));

  new_view = old_view;

  // The old cache should have its event registrations cleared out. If the
  // registration was left behind in the old_view, this test will crash at the
  // end due to double-deleting the registration.

  // The new cache should be exactly what the old one was.
  EXPECT_EQ(new_view.query_spec(), query_spec);
  EXPECT_EQ(new_view.view_cache(), initial_view_cache);
  EXPECT_THAT(new_view.event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), {registration}));
}

TEST(View, GetCompleteServerCache_Empty) {
  QuerySpec query_spec;
  query_spec.path = Path("test/path");
  query_spec.params.order_by = QueryParams::kOrderByValue;
  query_spec.params.start_at_value = "Apple";
  CacheNode cache(IndexedVariant(Variant(), query_spec.params), true, false);
  ViewCache initial_view_cache(cache, cache);
  View view(query_spec, initial_view_cache);

  EXPECT_EQ(view.GetCompleteServerCache(Path("test/path")), nullptr);
}

TEST(View, GetCompleteServerCache_NonEmpty) {
  QuerySpec query_spec;
  query_spec.path = Path("test/path");
  query_spec.params.order_by = QueryParams::kOrderByValue;
  query_spec.params.start_at_value = "Apple";
  CacheNode cache(IndexedVariant(Variant(std::map<Variant, Variant>{
                                     std::make_pair("foo", "bar"),
                                     std::make_pair("baz", "quux"),
                                 }),
                                 query_spec.params),
                  true, false);
  ViewCache initial_view_cache(cache, cache);
  View view(query_spec, initial_view_cache);

  EXPECT_EQ(*view.GetCompleteServerCache(Path("foo")), "bar");
}

TEST(View, IsNotEmpty) {
  QuerySpec query_spec;
  ViewCache initial_view_cache;
  View view(query_spec, initial_view_cache);

  ValueEventRegistration* registration =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration));

  EXPECT_FALSE(view.IsEmpty());
}

TEST(View, IsEmpty) {
  QuerySpec query_spec;
  ViewCache initial_view_cache;
  View view(query_spec, initial_view_cache);

  EXPECT_TRUE(view.IsEmpty());
}

TEST(View, AddEventRegistration) {
  QuerySpec query_spec;
  ViewCache initial_view_cache;
  View view(query_spec, initial_view_cache);

  ValueEventRegistration* registration1 =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  ValueEventRegistration* registration2 =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  ValueEventRegistration* registration3 =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  ValueEventRegistration* registration4 =
      new ValueEventRegistration(nullptr, nullptr, QuerySpec());
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration1));
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration2));
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration3));
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration4));

  std::vector<EventRegistration*> expected_registrations{
      registration1,
      registration2,
      registration3,
      registration4,
  };

  EXPECT_THAT(view.event_registrations(),
              Pointwise(SmartPtrRawPtrEq(), expected_registrations));
}

class DummyValueListener : public ValueListener {
 public:
  ~DummyValueListener() override {}
  void OnValueChanged(const DataSnapshot& snapshot) override {}
  void OnCancelled(const Error& error, const char* error_message) override {}
};

TEST(View, RemoveEventRegistration_RemoveOne) {
  QuerySpec query_spec;
  query_spec.path = Path("test/path");
  query_spec.params.order_by = QueryParams::kOrderByValue;
  query_spec.params.start_at_value = "Apple";
  CacheNode cache(IndexedVariant(Variant(), query_spec.params), true, false);
  ViewCache initial_view_cache(cache, cache);
  View view(query_spec, initial_view_cache);

  DummyValueListener listener1;
  DummyValueListener listener2;
  DummyValueListener listener3;
  DummyValueListener listener4;

  ValueEventRegistration* registration1 =
      new ValueEventRegistration(nullptr, &listener1, QuerySpec());
  ValueEventRegistration* registration2 =
      new ValueEventRegistration(nullptr, &listener2, QuerySpec());
  ValueEventRegistration* registration3 =
      new ValueEventRegistration(nullptr, &listener3, QuerySpec());
  ValueEventRegistration* registration4 =
      new ValueEventRegistration(nullptr, &listener4, QuerySpec());
  view.AddEventRegistration(UniquePtr<EventRegistration>(registration1));
  view.AddEventRegistration(UniquePtr<EventRegistration>(registration2));
  view.AddEventRegistration(UniquePtr<EventRegistration>(registration3));
  view.AddEventRegistration(UniquePtr<EventRegistration>(registration4));

  std::vector<Event> expected_events{};
  std::vector<EventRegistration*> expected_registrations{
      registration1,
      registration2,
      registration4,
  };

  EXPECT_THAT(
      view.RemoveEventRegistration(static_cast<void*>(&listener3), kErrorNone),
      Pointwise(Eq(), expected_events));
}

TEST(View, RemoveEventRegistration_RemoveAll) {
  QuerySpec query_spec;
  query_spec.path = Path("test/path");
  query_spec.params.order_by = QueryParams::kOrderByValue;
  query_spec.params.start_at_value = "Apple";
  CacheNode cache(IndexedVariant(Variant(), query_spec.params), true, false);
  ViewCache initial_view_cache(cache, cache);
  View view(query_spec, initial_view_cache);

  DummyValueListener listener1;
  DummyValueListener listener2;
  DummyValueListener listener3;
  DummyValueListener listener4;

  ValueEventRegistration* registration1 =
      new ValueEventRegistration(nullptr, &listener1, QuerySpec());
  ValueEventRegistration* registration2 =
      new ValueEventRegistration(nullptr, &listener2, QuerySpec());
  ValueEventRegistration* registration3 =
      new ValueEventRegistration(nullptr, &listener3, QuerySpec());
  ValueEventRegistration* registration4 =
      new ValueEventRegistration(nullptr, &listener4, QuerySpec());
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration1));
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration2));
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration3));
  view.AddEventRegistration(UniquePtr<ValueEventRegistration>(registration4));

  std::vector<Event> results =
      view.RemoveEventRegistration(nullptr, kErrorDisconnected);

  EXPECT_EQ(results.size(), 4);

  EXPECT_EQ(results[0].type, kEventTypeError);
  EXPECT_EQ(results[0].event_registration, registration1);
  EXPECT_EQ(results[0].error, kErrorDisconnected);
  EXPECT_EQ(results[0].path, Path("test/path"));
  EXPECT_EQ(results[0].event_registration_ownership_ptr.get(), registration1);

  EXPECT_EQ(results[1].type, kEventTypeError);
  EXPECT_EQ(results[1].event_registration, registration2);
  EXPECT_EQ(results[1].error, kErrorDisconnected);
  EXPECT_EQ(results[1].path, Path("test/path"));
  EXPECT_EQ(results[1].event_registration_ownership_ptr.get(), registration2);

  EXPECT_EQ(results[2].type, kEventTypeError);
  EXPECT_EQ(results[2].event_registration, registration3);
  EXPECT_EQ(results[2].error, kErrorDisconnected);
  EXPECT_EQ(results[2].path, Path("test/path"));
  EXPECT_EQ(results[2].event_registration_ownership_ptr.get(), registration3);

  EXPECT_EQ(results[3].type, kEventTypeError);
  EXPECT_EQ(results[3].event_registration, registration4);
  EXPECT_EQ(results[3].error, kErrorDisconnected);
  EXPECT_EQ(results[3].path, Path("test/path"));
  EXPECT_EQ(results[3].event_registration_ownership_ptr.get(), registration4);
}

// View::ApplyOperation tests omitted. It just calls through to the functions
// ViewProcessor::ApplyOperation and GenerateEventsForChanges, and it is
// difficult to mock the interaction. Those functions are themselves tested in
// view_processor_test.cc and event_generator_test.cc respectively.

TEST(ViewDeathTest, ApplyOperation_MustHaveLocalCache) {
  QuerySpec query_spec;
  CacheNode local_cache(IndexedVariant(Variant()), true, false);
  CacheNode server_cache(IndexedVariant(Variant()), false, false);
  ViewCache initial_view_cache(local_cache, server_cache);
  View view(query_spec, initial_view_cache);

  Operation operation(Operation::kTypeMerge,
                      OperationSource(Optional<QueryParams>()), Path(),
                      Variant(), CompoundWrite(), Tree<bool>(), kAckConfirm);
  WriteTree write_tree;
  WriteTreeRef writes_cache(Path(), &write_tree);
  Variant complete_server_cache;
  std::vector<Change> changes;

  EXPECT_DEATH(view.ApplyOperation(operation, writes_cache,
                                   &complete_server_cache, &changes),
               DEATHTEST_SIGABRT);
}

TEST(ViewDeathTest, ApplyOperation_MustHaveServerCache) {
  QuerySpec query_spec;
  CacheNode local_cache(IndexedVariant(Variant()), false, false);
  CacheNode server_cache(IndexedVariant(Variant()), true, false);
  ViewCache initial_view_cache(local_cache, server_cache);
  View view(query_spec, initial_view_cache);

  Operation operation(Operation::kTypeMerge,
                      OperationSource(Optional<QueryParams>()), Path(),
                      Variant(), CompoundWrite(), Tree<bool>(), kAckConfirm);
  WriteTree write_tree;
  WriteTreeRef writes_cache(Path(), &write_tree);
  Variant complete_server_cache;
  std::vector<Change> changes;

  EXPECT_DEATH(view.ApplyOperation(operation, writes_cache,
                                   &complete_server_cache, &changes),
               DEATHTEST_SIGABRT);
}

TEST(View, GetInitialEvents) {
  QuerySpec query_spec;
  query_spec.path = Path("test/path");
  query_spec.params.order_by = QueryParams::kOrderByValue;
  CacheNode cache(IndexedVariant(Variant(std::map<Variant, Variant>{
                                     std::make_pair("foo", "bar"),
                                     std::make_pair("baz", "quux"),
                                 }),
                                 query_spec.params),
                  true, false);
  ViewCache initial_view_cache(cache, cache);
  View view(query_spec, initial_view_cache);

  ValueEventRegistration registration(nullptr, nullptr, QuerySpec());

  std::vector<Event> results = view.GetInitialEvents(&registration);
  std::vector<Event> expected_results{
      Event(kEventTypeValue, &registration,
            DataSnapshotInternal(nullptr,
                                 Variant(std::map<Variant, Variant>{
                                     std::make_pair("foo", "bar"),
                                     std::make_pair("baz", "quux"),
                                 }),
                                 query_spec),
            ""),
  };

  EXPECT_THAT(results, Pointwise(Eq(), expected_results));
}

TEST(View, GetEventCache) {
  CacheNode local_cache(IndexedVariant(Variant("Apples")), false, false);
  CacheNode server_cache(IndexedVariant(Variant("Bananas")), true, false);
  ViewCache initial_view_cache(local_cache, server_cache);
  View view(QuerySpec(), initial_view_cache);

  EXPECT_EQ(view.GetLocalCache(), "Apples");
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
