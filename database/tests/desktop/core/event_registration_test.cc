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

#include "database/src/desktop/core/event_registration.h"

#include "database/src/desktop/core/child_event_registration.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/event_type.h"
#include "database/src/include/firebase/database/common.h"
#include "database/tests/desktop/test/mock_listener.h"
#include "firebase/database/common.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::_;
using ::testing::StrEq;

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(ValueEventRegistrationTest, RespondsTo) {
  ValueEventRegistration registration(nullptr, nullptr, QuerySpec());
  EXPECT_FALSE(registration.RespondsTo(kEventTypeChildRemoved));
  EXPECT_FALSE(registration.RespondsTo(kEventTypeChildAdded));
  EXPECT_FALSE(registration.RespondsTo(kEventTypeChildMoved));
  EXPECT_FALSE(registration.RespondsTo(kEventTypeChildChanged));
  EXPECT_TRUE(registration.RespondsTo(kEventTypeValue));
  EXPECT_FALSE(registration.RespondsTo(kEventTypeError));
}

TEST(ValueEventRegistrationTest, CreateEvent) {
  ValueEventRegistration registration(nullptr, nullptr, QuerySpec());
  Variant variant = std::map<Variant, Variant>{
      std::make_pair(".value", 100),
      std::make_pair(".priority", 200),
  };
  IndexedVariant change_variant(variant, QueryParams());
  Change change(kEventTypeValue, change_variant, "new");
  QuerySpec query_spec;
  query_spec.path = Path("change/path");
  Event event = registration.GenerateEvent(change, query_spec);
  EXPECT_EQ(event.type, kEventTypeValue);
  EXPECT_EQ(event.event_registration, &registration);
  EXPECT_EQ(event.snapshot->GetValue().int64_value(), 100);
  EXPECT_EQ(event.snapshot->GetPriority().int64_value(), 200);
  EXPECT_EQ(event.snapshot->path(), Path("change/path/new"));
  EXPECT_STREQ(event.prev_name.c_str(), "");
  EXPECT_EQ(event.error, kErrorNone);
  EXPECT_EQ(event.path, Path());
}

TEST(ValueEventRegistrationTest, FireEvent) {
  MockValueListener listener;
  ValueEventRegistration registration(nullptr, &listener, QuerySpec());
  DataSnapshotInternal snapshot(nullptr, Variant(), QuerySpec());
  Event event(kEventTypeValue, &registration, snapshot);
  EXPECT_CALL(listener, OnValueChanged(_));
  registration.FireEvent(event);
}

TEST(ValueEventRegistrationTest, FireEventCancel) {
  MockValueListener listener;
  ValueEventRegistration registration(nullptr, &listener, QuerySpec());
  EXPECT_CALL(listener, OnCancelled(kErrorDisconnected, _));
  registration.FireCancelEvent(kErrorDisconnected);
}

TEST(ValueEventRegistrationTest, MatchesListener) {
  MockValueListener right_listener;
  MockValueListener wrong_listener;
  MockChildListener wrong_type_listener;
  ValueEventRegistration registration(nullptr, &right_listener, QuerySpec());
  EXPECT_TRUE(registration.MatchesListener(&right_listener));
  EXPECT_FALSE(registration.MatchesListener(&wrong_listener));
  EXPECT_FALSE(registration.MatchesListener(&wrong_type_listener));
}

TEST(ChildEventRegistrationTest, RespondsTo) {
  ChildEventRegistration registration(nullptr, nullptr, QuerySpec());
  EXPECT_TRUE(registration.RespondsTo(kEventTypeChildRemoved));
  EXPECT_TRUE(registration.RespondsTo(kEventTypeChildAdded));
  EXPECT_TRUE(registration.RespondsTo(kEventTypeChildMoved));
  EXPECT_TRUE(registration.RespondsTo(kEventTypeChildChanged));
  EXPECT_FALSE(registration.RespondsTo(kEventTypeValue));
  EXPECT_FALSE(registration.RespondsTo(kEventTypeError));
}

TEST(ChildEventRegistrationTest, CreateEvent) {
  ChildEventRegistration registration(nullptr, nullptr, QuerySpec());
  Variant variant = std::map<Variant, Variant>{
      std::make_pair(".value", 100),
      std::make_pair(".priority", 200),
  };
  IndexedVariant change_variant(variant, QueryParams());
  Change change(kEventTypeChildAdded, change_variant, "new");
  QuerySpec query_spec;
  query_spec.path = Path("change/path");
  Event event = registration.GenerateEvent(change, query_spec);
  EXPECT_EQ(event.type, kEventTypeChildAdded);
  EXPECT_EQ(event.event_registration, &registration);
  EXPECT_EQ(event.snapshot->GetValue().int64_value(), 100);
  EXPECT_EQ(event.snapshot->GetPriority().int64_value(), 200);
  EXPECT_EQ(event.snapshot->path(), Path("change/path/new"));
  EXPECT_STREQ(event.prev_name.c_str(), "");
  EXPECT_EQ(event.error, kErrorNone);
  EXPECT_EQ(event.path, Path());
}

TEST(ChildEventRegistrationTest, FireChildAddedEvent) {
  MockChildListener listener;
  ChildEventRegistration registration(nullptr, &listener, QuerySpec());
  DataSnapshotInternal snapshot(nullptr, Variant(), QuerySpec());
  Event event(kEventTypeChildAdded, &registration, snapshot,
              "Apples and bananas");
  EXPECT_CALL(listener, OnChildAdded(_, StrEq("Apples and bananas")));
  registration.FireEvent(event);
}

TEST(ChildEventRegistrationTest, FireChildChangedEvent) {
  MockChildListener listener;
  ChildEventRegistration registration(nullptr, &listener, QuerySpec());
  DataSnapshotInternal snapshot(nullptr, Variant(), QuerySpec());
  Event event(kEventTypeChildChanged, &registration, snapshot,
              "Upples and banunus");
  EXPECT_CALL(listener, OnChildChanged(_, StrEq("Upples and banunus")));
  registration.FireEvent(event);
}

TEST(ChildEventRegistrationTest, FireChildMovedEvent) {
  MockChildListener listener;
  ChildEventRegistration registration(nullptr, &listener, QuerySpec());
  DataSnapshotInternal snapshot(nullptr, Variant(), QuerySpec());
  Event event(kEventTypeChildMoved, &registration, snapshot,
              "Epples and banenes");
  EXPECT_CALL(listener, OnChildMoved(_, StrEq("Epples and banenes")));
  registration.FireEvent(event);
}

TEST(ChildEventRegistrationTest, FireChildRemovedEvent) {
  MockChildListener listener;
  ChildEventRegistration registration(nullptr, &listener, QuerySpec());
  DataSnapshotInternal snapshot(nullptr, Variant(), QuerySpec());
  Event event(kEventTypeChildRemoved, &registration, snapshot);
  EXPECT_CALL(listener, OnChildRemoved(_));
  registration.FireEvent(event);
}

TEST(ChildEventRegistrationTest, FireEventCancel) {
  MockChildListener listener;
  ChildEventRegistration registration(nullptr, &listener, QuerySpec());
  EXPECT_CALL(listener, OnCancelled(kErrorDisconnected, _));
  registration.FireCancelEvent(kErrorDisconnected);
}

TEST(ChildEventRegistrationTest, MatchesListener) {
  MockChildListener right_listener;
  MockChildListener wrong_listener;
  MockValueListener wrong_type_listener;
  ChildEventRegistration registration(nullptr, &right_listener, QuerySpec());
  EXPECT_TRUE(registration.MatchesListener(&right_listener));
  EXPECT_FALSE(registration.MatchesListener(&wrong_listener));
  EXPECT_FALSE(registration.MatchesListener(&wrong_type_listener));
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
