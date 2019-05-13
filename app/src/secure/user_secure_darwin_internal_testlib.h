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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_DARWIN_INTERNAL_TESTLIB_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_DARWIN_INTERNAL_TESTLIB_H_

#include <string>

namespace firebase {
namespace app {
namespace secure {

class UserSecureDarwinTestHelper {
 public:
  UserSecureDarwinTestHelper();
  ~UserSecureDarwinTestHelper();

 private:
  std::string test_keychain_file_;
  bool previous_interaction_mode_;  // To restore previous mode to
                                    // SecKeychainSetUserInteractionAllowed
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_DARWIN_INTERNAL_TESTLIB_H_
