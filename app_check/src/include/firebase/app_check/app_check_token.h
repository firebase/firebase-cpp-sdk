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

namespace firebase {
namespace app_check {

/**
 * Class to hold tokens emitted by the Firebase App Check service which are minted upon a successful
 * application verification. These tokens are the federated output of a verification flow, the
 * structure of which is independent of the mechanism by which the application was verified.
 */
class AppCheckToken {

  public:
  virtual ~AppCheckToken();

  /** Returns the raw JWT attesting to this application’s identity. */
  virtual std::string getToken() = 0;

  /** Returns the time at which the token will expire in milliseconds since epoch. */
  virtual long getExpireTimeMillis() = 0;
}

}  // namespace app_check
}  // namespace firebase
