// Copyright 2017 Google LLC
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

#include "auth/tests/desktop/test_utils.h"

#include <vector>

#include "app/src/include/firebase/internal/mutex.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {
namespace test {

namespace {
// Constant describing how many times we sleep 1ms to wait for loading
// persistence cache.
const int kWaitForLoadMaxTryout = 500;
}  // namespace

namespace detail {
ListenerChangeCounter::ListenerChangeCounter()
    : actual_changes_(0), expected_changes_(-1) {}

ListenerChangeCounter::~ListenerChangeCounter() { Verify(); }

void ListenerChangeCounter::ExpectChanges(const int num) {
  expected_changes_ = num;
}
void ListenerChangeCounter::VerifyAndReset() {
  Verify();
  expected_changes_ = -1;
  actual_changes_ = 0;
}

void ListenerChangeCounter::Verify() {
  const int expected = expected_changes_.load();
  if (expected != -1) {
    EXPECT_EQ(expected, actual_changes_.load());
  }
}

}  // namespace detail

void IdTokenChangesCounter::OnIdTokenChanged(Auth* const /*unused*/) {
  ++actual_changes_;
}

void AuthStateChangesCounter::OnAuthStateChanged(Auth* const /*unused*/) {
  ++actual_changes_;
}

void VerifyProviderData(const User& user) {
  const std::vector<UserInfoInterface>& provider_data = user.provider_data();
  EXPECT_EQ(1u, provider_data.size());
  if (provider_data.empty()) {
    return;  // Avoid crashing on vector out-of-bounds access below
  }
  EXPECT_EQ("fake_uid", provider_data[0].uid());
  EXPECT_EQ("fake_email@example.com", provider_data[0].email());
  EXPECT_EQ("fake_display_name", provider_data[0].display_name());
  EXPECT_EQ("fake_photo_url", provider_data[0].photo_url());
  EXPECT_EQ("fake_provider_id", provider_data[0].provider_id());
  EXPECT_EQ("123123", provider_data[0].phone_number());
}

void VerifyUser(const User& user) {
  EXPECT_EQ("localid123", user.uid());
  EXPECT_EQ("testsignin@example.com", user.email());
  EXPECT_EQ("", user.display_name());
  EXPECT_EQ("", user.photo_url());
  EXPECT_EQ("Firebase", user.provider_id());
  EXPECT_EQ("", user.phone_number());
  EXPECT_FALSE(user.is_email_verified());
  VerifyProviderData(user);
}

bool WaitOnLoadPersistence(AuthData* auth_data) {
  if (auth_data == nullptr) return false;
  bool load_finished = false;
  int load_wait_counter = 0;
  while (!load_finished) {
    if (load_wait_counter >= kWaitForLoadMaxTryout) {
      break;
    }
    load_wait_counter++;
    firebase::internal::Sleep(1);
    {
      MutexLock lock(auth_data->listeners_mutex);
      load_finished = !auth_data->persistent_cache_load_pending;
    }
  }
  return load_finished;
}

void WaitForPersistedUserDeleted(AuthData* auth_data) {
  if (auth_data == nullptr || auth_data->auth_impl == nullptr) return;
  auto* auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (!auth_impl->user_data_persist) return;
  // Persistence operations run in order on one scheduler, so once this delete
  // completes any save/delete queued earlier (e.g. by SignOut) has finished.
  const Future<void> deleted =
      auth_impl->user_data_persist->DeleteUserData(auth_data);
  while (deleted.status() == kFutureStatusPending) {
    firebase::internal::Sleep(1);
  }
}

using ::testing::NotNull;
using ::testing::StrNe;

void WaitForFuture(const firebase::Future<void>& future,
                   const firebase::auth::AuthError expected_error) {
  while (future.status() == firebase::kFutureStatusPending) {
    firebase::internal::Sleep(1);
  }
  [&] {
    ASSERT_EQ(firebase::kFutureStatusComplete, future.status());
    EXPECT_EQ(expected_error, future.error());
    if (expected_error != kAuthErrorNone) {
      EXPECT_THAT(future.error_message(), NotNull());
      EXPECT_THAT(future.error_message(), StrNe(""));
    }
  }();
}

}  // namespace test
}  // namespace auth
}  // namespace firebase
