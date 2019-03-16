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

#include "app/src/invites/cached_receiver.h"

#include <assert.h>

namespace firebase {
namespace invites {
namespace internal {

CachedReceiver::CachedReceiver()
    : match_strength_(kLinkMatchStrengthNoMatch),
      result_code_(0),
      has_pending_invite_(false),
      receiver_(nullptr) {}

CachedReceiver::~CachedReceiver() { SetReceiver(nullptr); }

ReceiverInterface* CachedReceiver::SetReceiver(ReceiverInterface* receiver) {
  MutexLock lock(lock_);
  ReceiverInterface* prev_receiver = receiver_;
  receiver_ = receiver;
  SendCachedInvite();
  return prev_receiver;
}

void CachedReceiver::SendCachedInvite() {
  MutexLock lock(lock_);
  if (receiver_) {
    NotifyReceiver(receiver_);
    has_pending_invite_ = false;
  }
}

void CachedReceiver::NotifyReceiver(ReceiverInterface* receiver) {
  MutexLock lock(lock_);
  if (has_pending_invite_ && receiver) {
    receiver->ReceivedInviteCallback(invitation_id_, deep_link_url_,
                                     match_strength_, result_code_,
                                     error_message_);
  }
}

void CachedReceiver::ReceivedInviteCallback(
    const std::string& invitation_id, const std::string& deep_link_url,
    InternalLinkMatchStrength match_strength, int result_code,
    const std::string& error_message) {
  MutexLock lock(lock_);
  // If there is already a pending invite, don't override it with an empty
  // invite.
  if (has_pending_invite_ && invitation_id.empty() && deep_link_url.empty() &&
      result_code == 0) {
    return;
  }

  has_pending_invite_ = true;
  invitation_id_ = invitation_id;
  deep_link_url_ = deep_link_url;
  match_strength_ = match_strength;

  result_code_ = result_code;
  error_message_ = error_message;
  SendCachedInvite();
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase
