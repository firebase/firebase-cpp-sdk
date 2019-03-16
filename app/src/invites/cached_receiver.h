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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INVITES_CACHED_RECEIVER_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INVITES_CACHED_RECEIVER_H_

#include <string>

#include "app/src/invites/receiver_interface.h"
#include "app/src/mutex.h"

namespace firebase {
namespace invites {
namespace internal {

// Receives and potentially caches invites / dynamic links until a receiver
// is registered with this class at which point all notifications are
// forwarded to the registered receiver.
class CachedReceiver : public ReceiverInterface {
 public:
  CachedReceiver();
  virtual ~CachedReceiver();

  // Callback called when an invite is received. If an error occurred,
  // result_code should be non-zero. Otherwise, either invitation_id should be
  // set, or deep_link_url should be set, or both.
  void ReceivedInviteCallback(const std::string& invitation_id,
                              const std::string& deep_link_url,
                              InternalLinkMatchStrength match_strength,
                              int result_code,
                              const std::string& error_message) override;

  // Set the receiver to forward invites / dynamic links to.
  // If an invite / link is cached call the receiver immediately with the data.
  ReceiverInterface* SetReceiver(ReceiverInterface* receiver);

  // Notify a receiver of any invites cached in this class.
  void NotifyReceiver(ReceiverInterface* receiver);

 private:
  // Send a cache data to the registered receiver.
  void SendCachedInvite();

 protected:
  // Mutex to manage the state of this class.
  Mutex lock_;

  // Last invite / dynamic link that was received.
  // The Invitation ID, if any.
  std::string invitation_id_;
  // Deep Link URL, if any.
  std::string deep_link_url_;

  // How strong or week the deep link match is, as defined by the Invites
  // library.
  InternalLinkMatchStrength match_strength_;

  // The error / result code, if an error occurred.
  int result_code_;
  // A description of the error, if one occurred.
  std::string error_message_;

  // Is there a pending invite to send when a receiver is set.
  bool has_pending_invite_;
  // Receiver to forward invites to.
  ReceiverInterface* receiver_;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INVITES_CACHED_RECEIVER_H_
