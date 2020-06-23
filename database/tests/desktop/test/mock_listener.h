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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_LISTENER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_LISTENER_H_

#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/listener.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {

class MockValueListener : public ValueListener {
 public:
  MOCK_METHOD(void, OnValueChanged, (const DataSnapshot& snapshot), (override));
  MOCK_METHOD(void, OnCancelled,
              (const Error& error, const char* error_message), (override));
};

class MockChildListener : public ChildListener {
 public:
  MOCK_METHOD(void, OnChildAdded,
              (const DataSnapshot& snapshot, const char* previous_sibling_key),
              (override));
  MOCK_METHOD(void, OnChildChanged,
              (const DataSnapshot& snapshot, const char* previous_sibling_key),
              (override));
  MOCK_METHOD(void, OnChildMoved,
              (const DataSnapshot& snapshot, const char* previous_sibling_key),
              (override));
  MOCK_METHOD(void, OnChildRemoved, (const DataSnapshot& snapshot), (override));
  MOCK_METHOD(void, OnCancelled,
              (const Error& error, const char* error_message), (override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_LISTENER_H_
