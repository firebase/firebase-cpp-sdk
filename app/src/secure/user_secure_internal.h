
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_INTERNAL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_INTERNAL_H_

#include <string>

#include "app/src/scheduler.h"

namespace firebase {
namespace app {
namespace secure {

class UserSecureInternal {
 public:
  virtual ~UserSecureInternal() = default;

  // Load persisted user data for given app name.
  virtual std::string LoadUserData(const std::string& app_name) = 0;

  // Save user data under the key of given app name.
  virtual void SaveUserData(const std::string& app_name,
                            const std::string& user_data) = 0;

  // Delete user data under the given app name.
  virtual void DeleteUserData(const std::string& app_name) = 0;

  // Delete all user data.
  virtual void DeleteAllData() = 0;

  // By default does nothing, but for subclasses this enables running in test
  // mode (needed on some platforms).
  static void EnableTestMode() {}
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_INTERNAL_H_
