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

#include "invites/src/ios/invites_receiver_internal_ios_callbacks.h"

#include <Foundation/Foundation.h>

#include "GIDSignIn.h"
#include "FIRInvites.h"

#include "app/src/invites/ios/invites_receiver_internal_ios.h"

namespace firebase {
namespace invites {
namespace internal {

// Used by Invites to complete Google Sign-in when sending an invite.
bool InvitesReceiverInternalIosCallbacks::OpenUrl(NSURL* url, NSString* source_application,
                                                  id annotation) {
  if ([[GIDSignIn sharedInstance] handleURL:(NSURL*)url
                          sourceApplication:(NSString*)source_application
                                 annotation:annotation]) {
    LogDebug("OpenURL handled by GIDSignIn.");
    // Return if signin handled the URL, in which case it's not for us.
    return true;
  }
  return false;
}

bool InvitesReceiverInternalIosCallbacks::FinishFetch(
    NSURL* url, NSString* source_application, id annotation,
    InvitesReceiverInternalIos::LinkInfo* link_info)  {
  bool handled_link = false;
  // Check FIRInvites for an incoming invitation.
  FIRReceivedInvite *invite =
      [FIRInvites handleURL:(NSURL*)url
          sourceApplication:(NSString*)source_application
                 annotation:annotation];
  if (invite) {
    if (invite.inviteId) {
      link_info->invite_id = invite.inviteId.UTF8String;
      handled_link = true;
    }
    if (invite.deepLink) {
      link_info->deep_link = invite.deepLink.UTF8String;
      handled_link = true;
    }
    // TODO(jsimantov): Remove this workaround when b/27612427 is fixed.
    if (link_info->deep_link == InvitesReceiverInternalIos::kNullDeepLinkUrl) {
      link_info->deep_link = "";
    }
    link_info->match_strength = (invite.matchType == FIRReceivedInviteMatchTypeStrong)
                                    ? internal::kLinkMatchStrengthPerfectMatch
                                    : internal::kLinkMatchStrengthWeakMatch;
  }
  return handled_link;
}

void InvitesReceiverInternalIosCallbacks::PerformConvertInvitation(const char* invitation_id) {
  [FIRInvites convertInvitation:@(invitation_id)];
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase
