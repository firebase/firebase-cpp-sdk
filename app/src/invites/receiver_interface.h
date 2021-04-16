/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INVITES_RECEIVER_INTERFACE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INVITES_RECEIVER_INTERFACE_H_

#include <string>

namespace firebase {
namespace invites {

namespace internal {

/// @brief Enum describing the strength of a dynamic links match.
///
/// This version is only used internally, and is not exposed to the user.  The
/// dynamic links and invites libraries both mirror this for a different version
/// that the dev can use.
enum InternalLinkMatchStrength {
  /// No match has been achieved
  kLinkMatchStrengthNoMatch = 0,

  /// The match between the Dynamic Link and device is not perfect.  You should
  /// not reveal any personal information related to the Dynamic Link.
  kLinkMatchStrengthWeakMatch,

  /// The match between the Dynamic Link and this device has a high confidence,
  /// but there is a small possibility of error.
  kLinkMatchStrengthStrongMatch,

  /// The match between the Dynamic Link and the device is exact.  You may
  /// safely
  /// reveal any personal information related to this Dynamic Link.
  kLinkMatchStrengthPerfectMatch
};

class ReceiverInterface {
 public:
  virtual ~ReceiverInterface() {}

  // Callback called when an invite is received. If an error occurred,
  // result_code should be non-zero. Otherwise, either invitation_id should be
  // set, or deep_link_url should be set, or both.
  virtual void ReceivedInviteCallback(const std::string& invitation_id,
                                      const std::string& deep_link_url,
                                      InternalLinkMatchStrength match_strength,
                                      int result_code,
                                      const std::string& error_message) = 0;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INVITES_RECEIVER_INTERFACE_H_
