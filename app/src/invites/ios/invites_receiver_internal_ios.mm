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

#include "app/src/invites/ios/invites_receiver_internal_ios.h"

#include <Foundation/Foundation.h>
#include <UIKit/UIKit.h>
#include <string.h>
#include <string>

#include "app/src/assert.h"
#include "app/src/build_type_generated.h"
#include "app/src/include/firebase/app.h"
#include "app/src/invites/invites_receiver_internal.h"
#include "app/src/invites/ios/invites_ios_startup.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"

#include "FIRDynamicLink.h"
#include "FIRDynamicLinks.h"

// We need access to the inviteId method of FIRDynamicLink. This is not in the
// public API yet, but will be added in a future release.
@interface FIRDynamicLink ()
- (nullable NSString *)inviteId;
@end

namespace firebase {
namespace invites {
namespace internal {

// When fetching invites, we will time out after kFetchCheckUrlTimeoutSeconds.
const float kFetchCheckUrlTimeoutSeconds = 300.0f;
// When fetching invites, how often to check if the URL has been set.
const float kFetchCheckUrlInitialIntervalSeconds = 0.2f;
// Amount to increase polling interval per iteration.
const float kFetchCheckUrlIntervalIncreaseSeconds = 0.2f;

enum LinkType { kLinkTypeNone, kLinkTypeUrl, kLinkTypeUniversal };

// Fetch dynamic link / invite using the currently instanced receiver object.
// Do nothing if a receiver isn't available.
static void FetchDynamicLink();

class InvitesIosStartupImpl : public InvitesIosStartup {
 public:
  explicit InvitesIosStartupImpl(int priority) : InvitesIosStartup(priority) {}
  virtual ~InvitesIosStartupImpl() {}

 protected:
  void HandleDidBecomeActive(UIApplication *application) override {
    LogDebug("HandleDidBecomeActive 0x%08x",
             static_cast<int>(reinterpret_cast<intptr_t>(application)));
    FetchDynamicLink();
  }

  BOOL HandleOpenUrl(UIApplication *application, NSURL *url,
                     NSString *sourceApplication, id annotation) override {
    LogDebug("HandleOpenUrl %s %s", url ? url.absoluteString.UTF8String : "(null)",
             sourceApplication ? sourceApplication.UTF8String : "(null)");
    return InvitesReceiverInternalIos::OpenUrl(
        url, sourceApplication, annotation) != NO;
  }

  BOOL HandleOpenUrl(UIApplication *application, NSURL *url,
                     NSDictionary *options) override {
    NSString *sourceApplication = options[UIApplicationOpenURLOptionsSourceApplicationKey];
    id annotation = options[UIApplicationOpenURLOptionsAnnotationKey];
    LogDebug("HandleOpenUrl(new) %s %s", url ? url.absoluteString.UTF8String : "(null)",
             sourceApplication ? sourceApplication.UTF8String : "(null)");
    return InvitesReceiverInternalIos::OpenUrl(
        url, sourceApplication, annotation) != NO;
  }

  BOOL HandleContinueUserActivity(
      UIApplication *application, NSUserActivity *userActivity,
      void (^restorationHandler)(NSArray *)) override {
    LogDebug("ContinueUserActivity %s", userActivity && userActivity.webpageURL ?
             userActivity.webpageURL.absoluteString.UTF8String : "(null)");
    return userActivity ?
        InvitesReceiverInternalIos::OpenUniversalLink(userActivity.webpageURL) != NO
        : false;
  }
};

static LinkType g_got_link_type = kLinkTypeNone;
static NSURL *g_open_url;
static NSString *g_source_application;
static id g_annotation = nil;
// Amount of time elapsed (in seconds) since the dynamic link URL poller started.
static float g_fetch_timer = 0.0f;
// Amount of time (in seconds) to wait between polling the dynamic link URL.
static float g_fetch_poll_interval = 0.0f;

// Instance of this class.  This is used to execute dynamic link fetches from swizzled methods of
// the app delegate.
static InvitesReceiverInternalIos* g_invites_receiver = nullptr;

// Miscellaneous hooks that are overriden by the invites library if it's initialized.
static InvitesReceiverInternalIos::Callbacks* g_callbacks = nullptr;

static InvitesIosStartupImpl g_ios_startup_impl(0);

// Mutex for the static stuff.
static ::firebase::Mutex g_static_mutex;

// Fetch dynamic link / invite using the currently instanced receiver object.
// Do nothing if a receiver isn't available.
static void FetchDynamicLink() {
  // Spin up a thread to perform a fetch call, because the underlying Fetch
  // call can potentially end up blocking if another callback is happening,
  // and we don't want to delay the startup.
  MutexLock lock(g_static_mutex);
  if (g_invites_receiver) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                   ^{
                     MutexLock lock(g_static_mutex);
                     if (g_invites_receiver) g_invites_receiver->Fetch();
                   });
  }
}

// Register the component that handles app delegate callbacks.
void InvitesReceiverInternalIos::RegisterStartup(StartupRegistration *registration) {
  firebase::LogDebug("Registered dynamic links handler %s", registration->identifier());
  g_ios_startup_impl.Register();
}


// TODO(jsimantov): Remove this constant when b/27612427 is fixed.
const char* const InvitesReceiverInternalIos::kNullDeepLinkUrl = "file:///dev/null";

BOOL InvitesReceiverInternalIos::OpenUrl(NSURL *url, NSString *source_application, id annotation) {
  LogDebug("OpenURL(%s, %s, %s)", url.description.UTF8String,
           source_application.description.UTF8String, [annotation description].UTF8String);
  MutexLock lock(g_static_mutex);
  if (g_callbacks && g_callbacks->OpenUrl(url, source_application, annotation)) {
    return YES;
  }
  g_open_url = url;
  g_source_application = source_application;
  g_annotation = annotation;
  g_got_link_type = kLinkTypeUrl;
  FetchDynamicLink();
  return YES;  // We are potentially handling this URL.
}

BOOL InvitesReceiverInternalIos::OpenUniversalLink(NSURL *universal_link) {
  LogDebug("OpenUniversalLink(%s)", universal_link.description.UTF8String);
  MutexLock lock(g_static_mutex);
  g_open_url = universal_link;
  g_source_application = nil;
  g_annotation = nil;
  g_got_link_type = kLinkTypeUniversal;
  FetchDynamicLink();
  return YES;  // We are potentially handling this URL.
}

InvitesReceiverInternalIos::InvitesReceiverInternalIos(
    const ::firebase::App &app)
    : InvitesReceiverInternal(app),
      callback_mutex_(Mutex::kModeNonRecursive),
      fetch_in_progress_(false) {
  assert(!g_invites_receiver);
  g_invites_receiver = this;
  g_ios_startup_impl.Register();

}

InvitesReceiverInternalIos::~InvitesReceiverInternalIos() {
  {
    MutexLock lock(fetch_mutex_);
    fetch_in_progress_ = false;
  }
  {
    MutexLock lock(g_static_mutex);
    g_invites_receiver = nullptr;
  }
  {
    // Wait for any pending callbacks to finish.
    MutexLock lock(callback_mutex_);
  }
  g_ios_startup_impl.Unregister();
}

bool InvitesReceiverInternalIos::PerformFetch() {
  {
    MutexLock lock(g_static_mutex);
    g_fetch_timer = 0.0f;
    g_fetch_poll_interval = kFetchCheckUrlInitialIntervalSeconds;
  }
  {
    MutexLock lock(fetch_mutex_);
    if (fetch_in_progress_) return true;
    fetch_in_progress_ = true;
  }
  callback_mutex_.Acquire();  // Released by FinishFetch() when the fetch is complete.
  std::string url;
  MutexLock lock(g_static_mutex);
  // Check if g_open_url is set. When the app starts, g_open_url will be null while
  // checkForPendingDynamicLink executes, and then it will receive a special URL parsed by
  // [FIRInvite handleURL], whether or not there is an app invite or deep link. This means there
  // will be a short delay the first time we check for invites/deep links when the app starts.
  if (g_open_url != nil) {
    // Finish fetching now.
    FinishFetch();  // Releases callback_mutex_.
  } else {
    // Spin up a thread to check on the URL.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      // Check if we have received a URL yet. We should get one approx a quarter to half second
      // after the app starts.  We'll check a few times a second, and after a few seconds we give up
      // and time out.
      while (true) {
        float current_interval;
        {
          MutexLock lock(g_static_mutex);
          if (g_fetch_timer >= kFetchCheckUrlTimeoutSeconds) break;
          g_fetch_timer += g_fetch_poll_interval;
          current_interval = g_fetch_poll_interval;
          g_fetch_poll_interval += kFetchCheckUrlIntervalIncreaseSeconds;
        }
        [NSThread sleepForTimeInterval:current_interval];
        {
          MutexLock fetch_lock(fetch_mutex_);
          if (!fetch_in_progress_) break;  // Time out immediately.
        }
        bool link_ready = false;
        {
          MutexLock static_lock(g_static_mutex);
          link_ready = g_open_url != nil;
        }
        if (link_ready) {
          FinishFetch();  // Releases callback_mutex_.
          return;
        }
      }
      LogDebug("Timed out waiting for a link (waited %.2f seconds).", kFetchCheckUrlTimeoutSeconds);
      // Timed out waiting to receive the invite. Unfortunately, this happens in the Invites library
      // in certain cases. We have to consider that as just meaning there is no invite or deep link.
      ReceivedInviteCallback("", "", kLinkMatchStrengthNoMatch, 0, "");
      callback_mutex_.Release();
    });
  }
  return true;
}

static internal::InternalLinkMatchStrength MatchTypeToLinkStrength(FIRDLMatchType match_type) {
  switch (match_type) {
    case FIRDLMatchTypeNone:
      return internal::kLinkMatchStrengthNoMatch;
    case FIRDLMatchTypeWeak:
      return internal::kLinkMatchStrengthWeakMatch;
    case FIRDLMatchTypeDefault:
      return internal::kLinkMatchStrengthStrongMatch;
    case FIRDLMatchTypeUnique:
      return internal::kLinkMatchStrengthPerfectMatch;
  }
}

void InvitesReceiverInternalIos::FinishFetch() {
  NSURL *url;
  NSString *source_application;
  id annotation;
  LinkType link_type;
  {
    MutexLock lock(g_static_mutex);
    FIREBASE_ASSERT(g_open_url != nil);
    // Consume the URL.
    url = g_open_url;
    g_open_url = nil;
    source_application = g_source_application;
    g_source_application = nil;
    annotation = g_annotation ? g_annotation : @{};  // handleURL fails on nil, use empty instead.
    g_annotation = nil;
    link_type = g_got_link_type;
    g_got_link_type = kLinkTypeNone;
  }

  if (link_type == kLinkTypeUrl) {
    LogDebug("URL link %s", url.absoluteString.UTF8String);
    // Regular URL, handle it synchronously.
    bool processed_link = false;
    LinkInfo link_info;
    {
      MutexLock lock(g_static_mutex);
      processed_link = g_callbacks && g_callbacks->FinishFetch(url, source_application, annotation,
                                                               &link_info);
    }

    int error_code = 0;
    const char* error_string = "";
    if (!processed_link) {
      // If FIRInvites couldn't get a link, check via FIRDynamicLinks instead (required for iOS 8).
      FIRDynamicLink *dynamicLink =
          [[FIRDynamicLinks dynamicLinks] dynamicLinkFromCustomSchemeURL:url];

      if (dynamicLink) {
        // Got a FIRDynamicLink. It may or may not have an inviteId, but it will have a link.
        // TODO(jsimantov): when FIRDynamicLink has metadata implemented, switch inviteId to use
        // that.
        NSString *inviteId = [dynamicLink inviteId];
        if (inviteId) link_info.invite_id = inviteId.UTF8String;
        if (dynamicLink.url) link_info.deep_link = [dynamicLink.url absoluteString].UTF8String;
        // TODO(jsimantov): Remove this workaround when b/27612427 is fixed.
        if (link_info.deep_link == InvitesReceiverInternalIos::kNullDeepLinkUrl) {
          link_info.deep_link = "";
        }
        link_info.match_strength = MatchTypeToLinkStrength(dynamicLink.matchType);
      } else {
        LogWarning("Failed to process %s", url.absoluteString.UTF8String);
        error_string = "Unable to retrieve link";
        error_code = 1;
      }
    }
    ReceivedInviteCallback(link_info.invite_id, link_info.deep_link, link_info.match_strength,
                           error_code, error_string);
  } else if (link_type == kLinkTypeUniversal) {
    LogDebug("Universal link %s", url.absoluteString.UTF8String);
    // Keep a strong reference to the FIRDynamicLinks singleton in the completion block until
    // the block is complete.
    __block FIRDynamicLinks* dynamic_links_reference = [FIRDynamicLinks dynamicLinks];
    // iOS 9+ Universal Link, handle asynchronously. (dynamic links only)
    BOOL handled = [dynamic_links_reference handleUniversalLink:url
                                                     completion:^(
                                                         FIRDynamicLink *_Nullable dynamic_link,
                                                         NSError *_Nullable error) {
                   MutexLock lock(g_static_mutex);
                   dynamic_links_reference = nil;
                   auto receiver = g_invites_receiver;
                   if (receiver) {
                     int error_code = 1; /* firebase::dynamic_links::kErrorCodeFailed */
                     std::string error_string = "Unknown error occurred.";
                     if (dynamic_link) {
                       std::string invite_id = util::NSStringToString(dynamic_link.inviteId);
                       std::string url_string = util::NSStringToString(
                           dynamic_link.url.absoluteString);
                       if (!url_string.empty() || !invite_id.empty()) {
                         receiver->ReceivedInviteCallback(
                             invite_id, url_string, MatchTypeToLinkStrength(dynamic_link.matchType),
                             0, "");
                         return;
                       }
                     }
                     if (error) {
                       std::string error_description =
                           util::NSStringToString(error.localizedDescription);
                       if (!error_description.empty()) error_string = error_description;
                       if (error.code) error_code = static_cast<int>(error.code);
                     } else {
                       error_string = "The short dynamic link references a scheme that does not "
                           "match this application's bundle ID.";
                     }
                     receiver->ReceivedInviteCallback("", "", kLinkMatchStrengthNoMatch, error_code,
                                                      error_string);
                   }
                 }];
    if (!handled) {
      // Link wasn't handled, complete with no received link.
      ReceivedInviteCallback("", "", kLinkMatchStrengthNoMatch, 0, "");
    }
  }
  {
    MutexLock lock(fetch_mutex_);
    fetch_in_progress_ = false;
  }
  callback_mutex_.Release();
}

bool InvitesReceiverInternalIos::PerformConvertInvitation(const char* invitation_id) {
  {
    MutexLock lock(g_static_mutex);
    if (g_callbacks) g_callbacks->PerformConvertInvitation(invitation_id);
  }
  ConvertedInviteCallback(invitation_id, 0, "");
  return true;
}

void InvitesReceiverInternalIos::SetCallbacks(
    InvitesReceiverInternalIos::Callbacks* callbacks) {
  MutexLock lock(g_static_mutex);
  g_callbacks = callbacks;
}


}  // namespace internal
}  // namespace invites
}  // namespace firebase
