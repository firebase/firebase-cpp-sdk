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

// Internal header file for InvitesSender functionality.

#ifndef FIREBASE_INVITES_CLIENT_CPP_SRC_COMMON_INVITES_SENDER_INTERNAL_H_
#define FIREBASE_INVITES_CLIENT_CPP_SRC_COMMON_INVITES_SENDER_INTERNAL_H_

#include <map>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/invites/sender_receiver_interface.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "invites/src/include/firebase/invites.h"

namespace firebase {
namespace invites {
namespace internal {

// This class performs the general functionality of the InvitesSender class.
// It handles the invitation settings, setting up the Future results
// returned by InvitesSender, and processing the "invitation sent" callback
// from Java. A subclass of this class that overrides PerformSendInvite()
// handles the platform-specific parts of displaying the UI and sending the
// actual invite.
class InvitesSenderInternal : public SenderReceiverInterface {
 public:
  // Create an instance of whichever subclass of InvitesSenderInternal is
  // appropriate for our platform.
  static InvitesSenderInternal* CreateInstance(const ::firebase::App& app);

  // The next time an instance would be created via a call to CreateInstance(),
  // return this instance instead. Use this for testing (to stub the
  // platform-specific subclass of InvitesSenderInternal). It will be deleted
  // normally when the InvitesSender class is deleted.
  static void SetNextCreatedInstance(InvitesSenderInternal* new_instance);

  // Full list of all the invitation settings supported by all platforms.
  // In some cases, some platforms may ignore some of these. See the
  // platform-specific App Invites documentation for more information.
  enum InvitationSetting {
    kTitleText = 0,
    kMessageText,
    kAndroidClientID,
    kCallToActionText,
    kDescriptionText,
    kEmailContentHTML,
    kEmailSubjectText,
    kDeepLinkURL,
    kGoogleAnalyticsTrackingID,
    kIOSClientID,
    kCustomImageURL,
    kAndroidMinimumVersionCode,

    kInvitationSettingCount
  };

  // Virtual destructor required.
  virtual ~InvitesSenderInternal() { ClearInvitationSettings(); }

  // Platform-specific action: begin showing the UI.
  // Returns true if successful or false if not.
  virtual bool PerformSendInvite() = 0;

  // Set an invitation setting to the given value, or delete it by passing
  // in nullptr.
  void SetInvitationSetting(InvitationSetting key, const char* value);

  // Clear all previously-set invitation settings.
  void ClearInvitationSettings();

  // Setting the additional referral parameter with the given key to the given
  // value. See the platform-specific App Invites documentation for explanation.
  void AddReferralParam(const char* key, const char* value);

  // Clear all additional referral parameters entirely.
  void ClearReferralParams();

  // Start displaying the Send Invite UI. This will call PerformSendInvite
  // to do the platform-specific part. If PerformSendInvite returns true, the
  // Future will complete once SentInviteCallback runs. If PerformSendInvite
  // returns false, the Future will complete immediately (reporting an error).
  //
  // If the Send Invite UI is already being displayed when you call this, you
  // will hook into the existing UI and get the same result.
  Future<SendInviteResult> SendInvite();

  // Get the most recent (possibly still pending) result from SendInvite.
  Future<SendInviteResult> SendInviteLastResult();

  // Called when an invite has been sent.
  void SentInviteCallback(const std::vector<std::string>& invitation_ids,
                          int result_code,
                          const std::string& error_message) override;

  // Not used by the sender.
  void ReceivedInviteCallback(const std::string& invitation_id,
                              const std::string& deep_link_url,
                              InternalLinkMatchStrength match_strength,
                              int result_code,
                              const std::string& error_message) override {}

  // Not used by the sender.
  void ConvertedInviteCallback(const std::string& invitation_id,
                               int result_code,
                               std::string error_message) override {}

 protected:
  enum InvitesSenderFn { kInvitesSenderFnSend, kInvitesSenderFnCount };

  // Only instantiated by the static Create() method.
  explicit InvitesSenderInternal(const ::firebase::App& app)
      : app_(&app),
        future_impl_(kInvitesSenderFnCount),
        future_handle_send_(ReferenceCountedFutureImpl::kInvalidHandle) {
    invitation_settings_.resize(static_cast<size_t>(kInvitationSettingCount));
  }

  const std::map<std::string, std::string>& referral_parameters() {
    return referral_parameters_;
  }

  // The result will only be valid until any invitation settings are changed,
  // so use it quick!
  const char* GetInvitationSetting(InvitationSetting key);

  bool HasInvitationSetting(InvitationSetting key) {
    return (GetInvitationSetting(key) != nullptr);
  }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

 protected:
  // Keep a pointer to the App in case we need to call Initialize().
  const App* app_;

 private:
  // Futures implementation, and the corresponding mutex.
  ReferenceCountedFutureImpl future_impl_;

  // When sending invites, ref_future_send_result_ will be non-null until the
  // fetch finishes. After the send finishes, the result will then set in
  // future_send_result_, and and can be received via future_send_result_.
  FutureHandle future_handle_send_;

  // Mutex for locking invitation_settings_ and referral_parameters_.
  Mutex invitation_settings_mutex_;

  // A nullptr string implies the setting is not set. Otherwise, the string
  // value is used (even if blank).
  std::vector<std::string*> invitation_settings_;

  // Additional referral parameters, to be passed to the platform-specific
  // library.
  std::map<std::string, std::string> referral_parameters_;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_SRC_COMMON_INVITES_SENDER_INTERNAL_H_
