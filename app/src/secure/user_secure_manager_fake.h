
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_MANAGER_FAKE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_MANAGER_FAKE_H_

#include "app/src/secure/user_secure_manager.h"

namespace firebase {
namespace app {
namespace secure {

// Fake version of UserSecureManager usable for testing. Stores to TEST_TMPDIR.
class UserSecureManagerFake : public UserSecureManager {
 public:
  explicit UserSecureManagerFake(const char* domain, const char* app_id);
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_MANAGER_FAKE_H_
