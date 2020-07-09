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

namespace firebase {
namespace auth {
namespace test {

namespace detail {
ListenerChangeCounter::ListenerChangeCounter()
    : actual_changes_(0), expected_changes_(-1) {}

ListenerChangeCounter::~ListenerChangeCounter() { Verify(); }

void ListenerChangeCounter::ExpectChanges(const int num) {
  expected_changes_ = num;
}
void ListenerChangeCounter::VerifyAndReset() {
  int timeout_value = -1;
#ifdef __APPLE__
    timeout_value = 10000;
#endif
  Verify(timeout_value);
  expected_changes_ = -1;
  actual_changes_ = 0;
}

void ListenerChangeCounter::Verify(int timeout) {
  if (expected_changes_ != -1) {
    if (timeout >= 0 && expected_changes_ != actual_changes_) {
      int elapsed_time = 0;
      while( elapsed_time <= timeout ) {
        printf(".");
        firebase::internal::Sleep(100);
        elapsed_time += 100; // only estimating elapsed time to simplfy portability.
        if(expected_changes_ == actual_changes_) {
          break;
        }
      }
      printf("\n");
    }
    EXPECT_EQ(expected_changes_, actual_changes_);
  }
}

}  // namespace detail

void IdTokenChangesCounter::OnIdTokenChanged(Auth* const /*unused*/) {
  ++actual_changes_;
}

void AuthStateChangesCounter::OnAuthStateChanged(Auth* const /*unused*/) {
  ++actual_changes_;
}

using ::testing::NotNull;
using ::testing::StrNe;

void WaitForFuture(const firebase::Future<void>& future,
                   const firebase::auth::AuthError expected_error) {
  while (future.status() == firebase::kFutureStatusPending) {
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
