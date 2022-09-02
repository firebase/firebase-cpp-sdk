// Copyright 2022 Google LLC
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

#include <memory>

#include "app/rest/transport_builder.h"
#include "app/src/app_common.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/desktop/rpcs/create_auth_uri_request.h"
#include "auth/src/desktop/rpcs/secure_token_request.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

using ::firebase::heartbeat::HeartbeatStorageDesktop;
using ::firebase::heartbeat::LoggedHeartbeats;

#if FIREBASE_PLATFORM_DESKTOP
TEST(AuthRequestHeartbeatTest, TestAuthRequestHasHeartbeatPayload) {
  std::unique_ptr<App> app(testing::CreateApp());
  Logger logger(nullptr);
  HeartbeatStorageDesktop storage(app->name(), logger);
  // For the sake of testing, clear any pre-existing stored heartbeats.
  LoggedHeartbeats empty_heartbeats_struct;
  storage.Write(empty_heartbeats_struct);
  // Then log a single heartbeat for today's date.
  app->LogDesktopHeartbeat();
  CreateAuthUriRequest request(*app, "APIKEY", "email");

  // The payload will be encoded and the date will be today's date
  // But it should at least be non-empty.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST(AuthRequestHeartbeatTest,
     TestSecureTokenRequestDoesNotHaveHeartbeatPayload) {
  std::unique_ptr<App> app(testing::CreateApp());
  Logger logger(nullptr);
  HeartbeatStorageDesktop storage(app->name(), logger);
  // For the sake of testing, clear any pre-existing stored heartbeats.
  LoggedHeartbeats empty_heartbeats_struct;
  storage.Write(empty_heartbeats_struct);
  // Then log a single heartbeat for today's date.
  app->LogDesktopHeartbeat();
  SecureTokenRequest request(*app, "APIKEY", "email");

  // SecureTokenRequest should not have heartbeat payload since it is sent
  // to a backend that does not support the payload.
  EXPECT_EQ("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("", request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

}  // namespace auth
}  // namespace firebase
