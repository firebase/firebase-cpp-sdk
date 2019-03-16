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

// Stores all constants of Firebase Auth desktop implementation for developers.
#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_CONSTANTS_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_CONSTANTS_H_

namespace firebase {
namespace auth {

// Provider ID strings.
extern const char kEmailPasswordAuthProviderId[];
extern const char kGoogleAuthProviderId[];
extern const char kFacebookAuthProviderId[];
extern const char kTwitterAuthProviderId[];
extern const char kGitHubAuthProviderId[];
extern const char kPhoneAuthProdiverId[];
extern const char kPlayGamesAuthProviderId[];
extern const char kGameCenterAuthProviderId[];

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_CONSTANTS_H_
