// Copyright 2016 Google LLC
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

#include "invites/src/ios/invites_sender_internal_ios.h"

#include <UIKit/UIKit.h>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/invites/ios/invites_receiver_internal_ios.h"
#include "app/src/log.h"
#include "invites/src/common/invites_sender_internal.h"
#include "invites/src/include/firebase/invites.h"

#import "FIRInvites.h"
#import "FIRInvitesTargetApplication.h"
#import "GIDSignIn.h"

// Suppress "warning: performSelector may cause a leak because its selector is unknown" in this
// source file. It's safe to ignore this, because the only performSelector calls we make return
// void, so automatic reference counting wouldn't care about the return values.
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"

// The Firebase Invites library needs an Objective-C class callback to
// give us back results.
@interface FBIInviteSenderDelegate
    : NSObject<FIRInviteDelegate, GIDSignInDelegate, GIDSignInUIDelegate>
@property(nonatomic) void* senderObject;
@property(nonatomic) BOOL signInSilently;
@property(nonatomic, weak) UIViewController* viewController;
- (id)initWithSenderObject:(void*)senderObject;
- (void)inviteFinishedWithInvitations:(NSArray*)invitationIds error:(NSError*)error;
- (void)signIn:(GIDSignIn*)signIn didSignInForUser:(GIDGoogleUser*)user withError:(NSError*)error;
- (void)signIn:(GIDSignIn*)signIn
    didDisconnectWithUser:(GIDGoogleUser*)user
                withError:(NSError*)error;
- (void)signIn:(GIDSignIn*)signIn presentViewController:(UIViewController*)viewController;
- (void)signIn:(GIDSignIn*)signIn dismissViewController:(UIViewController*)viewController;
@end

namespace firebase {
namespace invites {
namespace internal {

struct InvitesSenderInternalIos::ObjcData {
  id<FIRInviteBuilder> builder;
  FBIInviteSenderDelegate* delegate;
  ~ObjcData() {}  // Needed so ARC can dispose of the above.
};

InvitesSenderInternalIos::InvitesSenderInternalIos(const ::firebase::App& app)
    : InvitesSenderInternal(app) {
  objc_ = new ObjcData();
  objc_->delegate = [[FBIInviteSenderDelegate alloc] initWithSenderObject:this];
}

InvitesSenderInternalIos::~InvitesSenderInternalIos() {
  [FIRInvites closeActiveInviteDialog];
  objc_->delegate = nil;
  objc_->builder = nil;
  delete objc_;
}

// Ensure that the selectors here match the selectors on FIRInviteBuilder. Also, to prevent a
// warning, we add these selectors to the @interface above, even though the delegate doesn't support
// them.
static const struct {
  InvitesSenderInternal::InvitationSetting native_key;
  NSString* selector_name;
} kObjcMappings[] = {{InvitesSenderInternal::kTitleText, @"setTitle:"},
                     {InvitesSenderInternal::kMessageText, @"setMessage:"},
                     {InvitesSenderInternal::kDeepLinkURL, @"setDeepLink:"},
                     {InvitesSenderInternal::kDescriptionText, @"setAppDescription:"},
                     {InvitesSenderInternal::kDescriptionText, @"setDescription:"},  // Fallback.
                     {InvitesSenderInternal::kCustomImageURL, @"setCustomImage:"},
                     {InvitesSenderInternal::kCallToActionText, @"setCallToActionText:"},
                     {InvitesSenderInternal::kInvitationSettingCount, nil}};

bool InvitesSenderInternalIos::PerformSendInvite() {
  // Run on UI thread.
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([GIDSignIn sharedInstance].currentUser != nil) {
      // Already signed in, so just send the invite.
      FinishSendingInvite();
    } else {
      // Not signed in, try to sign in silently first.
      NSString* path =
          [[NSBundle mainBundle] pathForResource:@"GoogleService-Info" ofType:@"plist"];
      NSDictionary* plistDict = [NSDictionary dictionaryWithContentsOfFile:path];
      NSString* clientId = plistDict[@"CLIENT_ID"];
      [GIDSignIn sharedInstance].clientID = clientId;
      [GIDSignIn sharedInstance].delegate = objc_->delegate;
      [GIDSignIn sharedInstance].uiDelegate = objc_->delegate;
      objc_->delegate.signInSilently = TRUE;
      [[GIDSignIn sharedInstance] signInSilently];
    }
  });
  return true;
}

bool InvitesSenderInternalIos::FinishSendingInvite() {
  @try {
    objc_->builder = [FIRInvites inviteDialog];
    [objc_->builder setInviteDelegate:objc_->delegate];
    for (int i = 0; kObjcMappings[i].selector_name != nil; i++) {
      InvitationSetting key = kObjcMappings[i].native_key;

      if (HasInvitationSetting(key)) {
        NSString* nsstring = @(GetInvitationSetting(key));
        SEL selector = NSSelectorFromString(kObjcMappings[i].selector_name);
        if ([objc_->builder respondsToSelector:selector]) {
          [objc_->builder performSelector:selector withObject:nsstring];
        }
      }
    }
    // Special cases:
    // Handle AndroidMinimumVersionCode, if set.
    if (HasInvitationSetting(kAndroidMinimumVersionCode)) {
      // Convert the Android minimum version code to a number.
      int androidMinimumVersionCode = atoi(GetInvitationSetting(kAndroidMinimumVersionCode));
      [objc_->builder setAndroidMinimumVersionCode:androidMinimumVersionCode];
    }
    // Handle OtherPlatformsTargetApplication, if set.
    if (HasInvitationSetting(kAndroidClientID)) {
      FIRInvitesTargetApplication* otherApp = [[FIRInvitesTargetApplication alloc] init];
      otherApp.androidClientID = @(GetInvitationSetting(kAndroidClientID));
      [objc_->builder setOtherPlatformsTargetApplication:otherApp];
    }

    // TODO(jsimantov): Remove this workaround when b/27612427 is fixed.
    if (!HasInvitationSetting(kDeepLinkURL)) {
      [objc_->builder setDeepLink:@(InvitesReceiverInternalIos::kNullDeepLinkUrl)];
    }

    // Handle GoogleAnalyticsTrackingID, if set.
    if (HasInvitationSetting(kGoogleAnalyticsTrackingID)) {
      [FIRInvites setGoogleAnalyticsTrackingId:@(GetInvitationSetting(kGoogleAnalyticsTrackingID))];
    }
    [objc_->builder open];
  } @catch (NSException* e) {
    LogError("InvitesSender: Failed to build iOS invitation: %s", [e.reason UTF8String]);
    SentInviteCallback({}, -1, [e.reason UTF8String]);
  }
  return true;
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase

@implementation FBIInviteSenderDelegate
- (id)initWithSenderObject:(void*)senderObject {
  self = [super init];
  self.senderObject = senderObject;
  self.signInSilently = FALSE;
  return self;
}

- (void)inviteFinishedWithInvitations:(NSArray*)invitationIds error:(NSError*)error {
  // Store these back in the InvitesSenderInternal object.
  std::vector<std::string> id_vector;
  id_vector.clear();
  id_vector.reserve(invitationIds.count);
  for (NSUInteger i = 0; i < invitationIds.count; i++) {
    id_vector.push_back([[invitationIds objectAtIndex:i] UTF8String]);
  }

  firebase::invites::internal::InvitesSenderInternalIos* obj =
      reinterpret_cast<firebase::invites::internal::InvitesSenderInternalIos*>(self.senderObject);
  if (obj != nullptr) {
    obj->SentInviteCallback(id_vector, error == nullptr ? 0 : static_cast<int>([error code]),
                            error == nullptr ? "" : [[error localizedDescription] UTF8String]);
  }
}

- (void)signIn:(GIDSignIn*)signIn didSignInForUser:(GIDGoogleUser*)user withError:(NSError*)error {
  firebase::invites::internal::InvitesSenderInternalIos* obj =
      reinterpret_cast<firebase::invites::internal::InvitesSenderInternalIos*>(self.senderObject);

  if (error == nil) {
    if (!obj->FinishSendingInvite()) {
      obj->SentInviteCallback({}, static_cast<int>([error code]),
                              [[error localizedDescription] UTF8String]);
    }
  } else {
    // An error signing in.
    if (self.signInSilently) {
      ::firebase::LogDebug("SignIn trying non-silent: %s",
                           [[error localizedDescription] UTF8String]);
      // Try to sign in not-so-silently.
      self.signInSilently = FALSE;
      [[GIDSignIn sharedInstance] signIn];
    } else {
      // Signing in non-silently, and failed to do so. Report error.
      ::firebase::LogDebug("SignIn failed: %s", [[error localizedDescription] UTF8String]);
      obj->SentInviteCallback({}, static_cast<int>([error code]),
                              [[error localizedDescription] UTF8String]);
    }
  }
}

- (void)signIn:(GIDSignIn*)signIn
    didDisconnectWithUser:(GIDGoogleUser*)user
                withError:(NSError*)error {
}

- (void)signIn:(GIDSignIn*)signIn presentViewController:(UIViewController*)viewController {
  // Find the most top view controller to display the login window.
  UIViewController* topViewController =
      [UIApplication sharedApplication].delegate.window.rootViewController;
  while ([topViewController presentedViewController]) {
    topViewController = [topViewController presentedViewController];
  }
  [topViewController presentViewController:viewController animated:YES completion:nil];
}
- (void)signIn:(GIDSignIn*)signIn dismissViewController:(UIViewController*)viewController {
  [viewController dismissViewControllerAnimated:YES completion:nil];
}

@end
