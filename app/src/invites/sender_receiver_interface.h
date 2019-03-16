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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INVITES_SENDER_RECEIVER_INTERFACE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INVITES_SENDER_RECEIVER_INTERFACE_H_

#include <string>
#include <vector>

#include "app/src/invites/receiver_interface.h"

namespace firebase {
namespace invites {
namespace internal {

// Called by AndroidHelper when operations complete.
// This is due to AndroidHelper implementing both invite send and  dynamic link
// receive operations on Android.
// This interface is not implemented on iOS as the dynamic link receive logic
// is completely separate from the invite sending logic.
class SenderReceiverInterface : public ReceiverInterface {
 public:
  SenderReceiverInterface() {}
  virtual ~SenderReceiverInterface() {}

  // Called when an invite has been sent.
  virtual void SentInviteCallback(
      const std::vector<std::string>& invitation_ids, int result_code,
      const std::string& error_message) = 0;

  // Callback called when an invite conversion occurs. If an error occurred,
  // result_code will be non-zero. Otherwise, the conversion was successful.
  virtual void ConvertedInviteCallback(const std::string& invitation_id,
                                       int result_code,
                                       std::string error_message) = 0;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INVITES_SENDER_RECEIVER_INTERFACE_H_
