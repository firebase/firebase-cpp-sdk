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

#ifndef FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_RECEIVER_INTERNAL_IOS_CALLBACKS_H_
#define FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_RECEIVER_INTERNAL_IOS_CALLBACKS_H_

#include "app/src/invites/ios/invites_receiver_internal_ios.h"

#include <Foundation/Foundation.h>

namespace firebase {
namespace invites {
namespace internal {

// Setup invites specific callbacks for the InvitesReceiverInternalIos.
class InvitesReceiverInternalIosCallbacks
    : public InvitesReceiverInternalIos::Callbacks {
 public:
  InvitesReceiverInternalIosCallbacks() {
    InvitesReceiverInternalIos::SetCallbacks(this);
  }

  virtual ~InvitesReceiverInternalIosCallbacks() {
    InvitesReceiverInternalIos::SetCallbacks(nullptr);
  }

  // Used by Invites to complete Google Sign-in when sending an invite.
  bool OpenUrl(NSURL* url, NSString* source_application,
               id annotation) override;

  // Called when a URL link (vs. universal link) is being processed by
  // InvitesReceiverInternalIos::FinishFetch().  Link processing stops if
  // this returns true.
  bool FinishFetch(NSURL* url, NSString* source_application, id annotation,
                   InvitesReceiverInternalIos::LinkInfo* link_info) override;

  // Called from InvitesReceiverInternalIos::PerformConvertInvitation() to
  // convert an invite.
  void PerformConvertInvitation(const char* invitation_id) override;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_RECEIVER_INTERNAL_IOS_CALLBACKS_H_
