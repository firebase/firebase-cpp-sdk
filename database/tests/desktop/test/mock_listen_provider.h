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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_LISTEN_PROVIDER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_LISTEN_PROVIDER_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/listen_provider.h"

namespace firebase {
namespace database {
namespace internal {

class MockListenProvider : public ListenProvider {
 public:
  MOCK_METHOD(void, StartListening,
              (const QuerySpec& query_spec, const Tag& tag, const View* view),
              (override));
  MOCK_METHOD(void, StopListening,
              (const QuerySpec& query_spec, const Tag& tag), (override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_LISTEN_PROVIDER_H_
