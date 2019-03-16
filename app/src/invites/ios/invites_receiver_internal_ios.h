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

// Internal header file for iOS Firebase invites sending functionality.

#ifndef FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_RECEIVER_INTERNAL_IOS_H_
#define FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_RECEIVER_INTERNAL_IOS_H_

#include <string>

#include "app/src/invites/invites_receiver_internal.h"

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#endif  // __OBJC__

namespace firebase {
class App;
class Mutex;

namespace invites {
namespace internal {

// An internal class that performs the iOS-specific parts of receiving app
// invites and deep links. Because of the way iOS App Invites are implmented in
// the original library, the bulk of the work is done in the SetLaunchUrl()
// method.
//
// The general sequence of events when there's an incoming invite/deep link is:
//
// 1. SetLaunchUrl initializes and calls the dynamic links
// service to check for a deep link.
//
// 2. The dynamic links service, finding a link for the user, calls
// SetOpenUrl, passing in a special link that the app invites library will
// parse, which SetOpenUrl saves until later.
//
// 3. PerformFetch() checks the saved URL once it's present. If it matches the
// special format that the invites/DDL library uses, it parses out an invitation
// ID and/or deep link URL from it.
class InvitesReceiverInternalIos : public InvitesReceiverInternal {
 public:
  // Registers the startup class associated with this module.
  // This can be used from a static object to ensure registration occurs before
  // main().
  class StartupRegistration {
   public:
    // id must be valid for the lifetime of this class.
    StartupRegistration(const char* id) : identifier_(id) {
      InvitesReceiverInternalIos::RegisterStartup(this);
    }

    // Identifier used to debug this object.
    const char* identifier() const { return identifier_; }

   private:
    const char* identifier_;
  };

  // Used to receive link data from Callbacks::FinishFetch() so that the
  // InvitesReceiverInternalIos::Callbacks implementation can handle the
  // URL link before InvitesReceiverInternalIos falls back to it's default
  // behavior.
  struct LinkInfo {
    LinkInfo() : match_strength(kLinkMatchStrengthNoMatch) {}

    // ID of the invite derived from a URL link.
    std::string invite_id;
    // Deep link derived from a URL link.
    std::string deep_link;
    // How strong the match is.
    InternalLinkMatchStrength match_strength;
  };

#ifdef __OBJC__
  // Class used by the invites API to hook operations performed by the receiver.
  class Callbacks {
   public:
    virtual ~Callbacks() {}

    // Used by Invites to complete Google Sign-in when sending an invite.
    virtual bool OpenUrl(NSURL* url, NSString* source_application,
                         id annotation) = 0;

    // Called when a URL link (vs. universal link) is being processed by
    // InvitesReceiverInternalIos::FinishFetch().  Dynamic link processing
    // stops if this returns true.
    virtual bool FinishFetch(NSURL* url, NSString* source_application,
                             id annotation, LinkInfo* link_info) = 0;

    // Called from InvitesReceiverInternalIos::PerformConvertInvitation() to
    // convert an invite.
    virtual void PerformConvertInvitation(const char* invitation_id) = 0;
  };
#else
  // This header is also included by plain C++.
  class Callbacks;
#endif  // __OBJC__

  // Work around a bug where null deep links cause Android clients to fail.
  // TODO(jsimantov): Remove this constant when b/27612427 is fixed.
  static const char* const kNullDeepLinkUrl;

  InvitesReceiverInternalIos(const ::firebase::App& app);
  virtual ~InvitesReceiverInternalIos();

  // This function consumes the URL previously set by open_url_.
  virtual bool PerformFetch();

  virtual bool PerformConvertInvitation(const char* invitation_id);

#ifdef __OBJC__
  static void SetLaunchOptions(NSDictionary* launch_options);
  static BOOL OpenUrl(NSURL* url, NSString* source_application, id annotation);
  static BOOL OpenUniversalLink(NSURL* url);
#endif  // __OBJC__

  // Configure library specific callbacks, see Callbacks class for more
  // information.
  static void SetCallbacks(Callbacks* callbacks);

  // Register the component that handles app delegate callbacks.
  static void RegisterStartup(StartupRegistration* registration);

 private:
  void FinishFetch();

  Mutex callback_mutex_;    // Acquired when PerformFetch starts.
                            // Released after ReceivedInviteCallback is called.
  bool fetch_in_progress_;  // Whether a fetch is currently in progress.
  Mutex fetch_mutex_;       // Protects fetch_in_progress_.
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_RECEIVER_INTERNAL_IOS_H_
