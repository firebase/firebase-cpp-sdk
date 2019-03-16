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

#include "invites/src/include/firebase/invites.h"

#include <assert.h>
#include <cstdio>
#include <sstream>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/invites/cached_receiver.h"
#include "app/src/invites/invites_receiver_internal.h"
#include "app/src/invites/receiver_interface.h"
#include "app/src/util.h"
#include "invites/src/common/invites_sender_internal.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

#if TARGET_OS_IPHONE
#include "invites/src/ios/invites.h"
#endif  // TARGET_OS_IPHONE

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(invites,
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    return ::firebase::invites::Initialize(
                                        *app);
                                  }
                                  return kInitResultSuccess;
                                },
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    firebase::invites::Terminate();
                                  }
                                });

namespace firebase {
namespace invites {

DEFINE_FIREBASE_VERSION_STRING(FirebaseInvites);

namespace internal {
const char kInvitesModuleName[] = "invites";
}  // namespace internal

// Notifies a listener of a cached invite.
class CachedListenerNotifier : public internal::ReceiverInterface {
 public:
  CachedListenerNotifier() : listener_(nullptr), listener_sent_invite_(false) {}
  virtual ~CachedListenerNotifier() { SetListener(nullptr); }

  // Set the listener which should be notified of any cached or receiver
  // invites.
  Listener* SetListener(Listener* listener) {
    MutexLock lock(lock_);
    Listener* previous_listener = listener_;
    listener_sent_invite_ = false;
    listener_ = listener;
    receiver_.SetReceiver(listener ? this : nullptr);
    return previous_listener;
  }

 private:
  // Callback called when an invite is received. If an error occurred,
  // result_code should be non-zero. Otherwise, either invitation_id should be
  // set, or deep_link_url should be set, or both.
  void ReceivedInviteCallback(
      const std::string& invitation_id, const std::string& deep_link_url,
      internal::InternalLinkMatchStrength match_strength, int result_code,
      const std::string& error_message) override {
    MutexLock lock(lock_);
    if (listener_) {
      if (result_code) {
        listener_->OnErrorReceived(result_code, error_message.c_str());
      } else if (!invitation_id.empty() || !deep_link_url.empty()) {
        const char* id =
            invitation_id.empty() ? nullptr : invitation_id.c_str();
        const char* dl =
            deep_link_url.empty() ? nullptr : deep_link_url.c_str();

        listener_->OnInviteReceived(
            id, dl, static_cast<invites::LinkMatchStrength>(match_strength));

      } else if (!listener_sent_invite_) {
        listener_->OnInviteNotReceived();
      }
      listener_sent_invite_ = true;
    } else {
      receiver_.ReceivedInviteCallback(invitation_id, deep_link_url,
                                       match_strength, result_code,
                                       error_message);
    }
  }

 private:
  // Protects access to members of this class.
  Mutex lock_;
  // End user's listener which is notified of invites.
  Listener* listener_;
  // Caches received invites.
  internal::CachedReceiver receiver_;
  // Whether the listener has been notified of no invite received.
  bool listener_sent_invite_;
};

bool g_initialized = false;

extern const int kInitErrorNum;
extern const char kInitErrorMsg[];

const int kInitErrorNum = -2;
const char kInitErrorMsg[] = "firebase::invites::Initialize() unsuccessful.";

static const ::firebase::App* g_app = nullptr;
static internal::InvitesSenderInternal* g_sender = nullptr;
static internal::InvitesReceiverInternal* g_receiver = nullptr;
static CachedListenerNotifier* g_cached_receiver = nullptr;

InitResult Initialize(const App& app) {
  FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(app);
#if TARGET_OS_IPHONE
  internal::InitializeIos();
#endif  // TARGET_OS_IPHONE
  g_initialized = true;
  g_app = &app;
  g_cached_receiver = new CachedListenerNotifier();
  g_receiver =
      internal::InvitesReceiverInternal::CreateInstance(app, g_cached_receiver);
  if (!g_receiver) {
    delete g_cached_receiver;
    g_cached_receiver = nullptr;
    g_initialized = false;
    g_app = nullptr;
    return kInitResultFailedMissingDependency;
  }
  if (!AppCallback::GetEnabledByName(internal::kInvitesModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(const_cast<App*>(g_receiver->app()));
    assert(cleanup_notifier);
    cleanup_notifier->RegisterObject(
        const_cast<char*>(internal::kInvitesModuleName), [](void*) {
          LogError(
              "invites::Terminate() should be called before the "
              "default app is destroyed.");
          if (g_initialized) firebase::invites::Terminate();
        });
  }
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_initialized; }

}  // namespace internal

void Terminate() {
  if (g_initialized &&
      !AppCallback::GetEnabledByName(internal::kInvitesModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(const_cast<App*>(g_receiver->app()));
    assert(cleanup_notifier);
    cleanup_notifier->UnregisterObject(
        const_cast<char*>(internal::kInvitesModuleName));
  }
  g_initialized = false;
  g_app = nullptr;
  SetListener(nullptr);
  if (g_sender) {
    delete g_sender;
    g_sender = nullptr;
  }
  if (g_receiver) {
    internal::InvitesReceiverInternal::DestroyInstance(g_receiver,
                                                       g_cached_receiver);
    g_receiver = nullptr;
    delete g_cached_receiver;
    g_cached_receiver = nullptr;
  }
#if TARGET_OS_IPHONE
  internal::TerminateIos();
#endif  // TARGET_OS_IPHONE
}

Future<SendInviteResult> SendInvite(const Invite& invite) {
  FIREBASE_ASSERT_RETURN(Future<SendInviteResult>(), internal::IsInitialized());
  if (!g_sender) {
    g_sender = internal::InvitesSenderInternal::CreateInstance(*g_app);
    if (!g_sender) {
      LogError("Failed to create invites sender, invites not sent");
      return Future<SendInviteResult>();  // Construct an error future.
    }
  }
  g_sender->ClearInvitationSettings();
  if (invite.android_minimum_version_code) {
#ifdef FIREBASE_USE_SNPRINTF
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", invite.android_minimum_version_code);
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kAndroidMinimumVersionCode, buf);
#else
    std::stringstream ss;
    ss << invite.android_minimum_version_code;
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kAndroidMinimumVersionCode,
        ss.str().c_str());
#endif  // FIREBASE_USE_SNPRINTF
  }
  if (!invite.call_to_action_text.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kCallToActionText,
        invite.call_to_action_text.c_str());
  }
  if (!invite.custom_image_url.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kCustomImageURL,
        invite.custom_image_url.c_str());
  }
  if (!invite.deep_link_url.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kDeepLinkURL,
        invite.deep_link_url.c_str());
  }
  if (!invite.description_text.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kDescriptionText,
        invite.description_text.c_str());
  }
  if (!invite.email_content_html.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kEmailContentHTML,
        invite.email_content_html.c_str());
  }
  if (!invite.email_subject_text.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kEmailSubjectText,
        invite.email_subject_text.c_str());
  }
  if (!invite.google_analytics_tracking_id.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kGoogleAnalyticsTrackingID,
        invite.google_analytics_tracking_id.c_str());
  }
  if (!invite.message_text.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kMessageText,
        invite.message_text.c_str());
  }
  if (!invite.title_text.empty()) {
    g_sender->SetInvitationSetting(internal::InvitesSenderInternal::kTitleText,
                                   invite.title_text.c_str());
  }
  if (!invite.android_platform_client_id.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kAndroidClientID,
        invite.android_platform_client_id.c_str());
  }
  if (!invite.ios_platform_client_id.empty()) {
    g_sender->SetInvitationSetting(
        internal::InvitesSenderInternal::kIOSClientID,
        invite.ios_platform_client_id.c_str());
  }
  for (auto it = invite.referral_parameters.begin();
       it != invite.referral_parameters.end(); ++it) {
    g_sender->AddReferralParam(it->first.c_str(), it->second.c_str());
  }
  return g_sender->SendInvite();
}

Future<SendInviteResult> SendInviteLastResult() {
  FIREBASE_ASSERT_RETURN(Future<SendInviteResult>(), internal::IsInitialized());
  return g_sender->SendInviteLastResult();
}

Listener* SetListener(Listener* listener) {
  if (!internal::IsInitialized()) return nullptr;

  if (listener) {
    Fetch();
  }

  return g_cached_receiver->SetListener(listener);
}

Future<void> ConvertInvitation(const char* invitation_id) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  return g_receiver->ConvertInvitation(invitation_id);
}

Future<void> ConvertInvitationLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  return g_receiver->ConvertInvitationLastResult();
}

void Fetch() {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (g_receiver) {
    g_receiver->Fetch();
  }
}

void Listener::OnInviteReceived(const char* invitation_id,
                                const char* dynamic_link,
                                bool is_strong_match) {
  // At least one version of Listener::OnInviteReceived needs to be
  // overridden.  (Ideally other one, since this one is deprecated.)
  FIREBASE_ASSERT_MESSAGE(false,
                          "At least one version of "
                          "Listener::OnInviteReceived() must be overridden in "
                          "order for the Listener to be used.");
}

void Listener::OnInviteReceived(const char* invitation_id,
                                const char* dynamic_link,
                                LinkMatchStrength match_strength) {
  // Ideally this method should be overridden by the developer.
  OnInviteReceived(invitation_id, dynamic_link,
                   match_strength == kLinkMatchStrengthPerfectMatch);
}

}  // namespace invites
}  // namespace firebase
