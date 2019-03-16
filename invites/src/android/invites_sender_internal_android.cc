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

#include "invites/src/android/invites_sender_internal_android.h"
#include "app/src/include/firebase/app.h"
#include "app/src/invites/android/invites_android_helper.h"
#include "invites/src/common/invites_sender_internal.h"
#include "invites/src/include/firebase/invites.h"

namespace firebase {
namespace invites {
namespace internal {

static const struct {
  InvitesSenderInternal::InvitationSetting native_key;
  const char *java_key;
} kJNIMapping[] = {
    // Important:
    // These string constants must be kept in sync with the strings used
    // in the Java AppInviteNativeWrapper class's showSenderUI function.
    // If you modify those strings, you must change them here as well.
    {InvitesSenderInternal::kTitleText, "title"},
    {InvitesSenderInternal::kMessageText, "message"},
    {InvitesSenderInternal::kCustomImageURL, "customImage"},
    {InvitesSenderInternal::kCallToActionText, "callToActionText"},
    {InvitesSenderInternal::kEmailContentHTML, "emailHtmlContent"},
    {InvitesSenderInternal::kEmailSubjectText, "emailSubject"},
    {InvitesSenderInternal::kDeepLinkURL, "deepLink"},
    {InvitesSenderInternal::kGoogleAnalyticsTrackingID,
     "googleAnalyticsTrackingId"},
    {InvitesSenderInternal::kAndroidMinimumVersionCode,
     "androidMinimumVersionCode"},
    {InvitesSenderInternal::kIOSClientID, "otherPlatformsTargetApplicationIOS"},
    {InvitesSenderInternal::kAndroidClientID,
     "otherPlatformsTargetApplicationsAndroid"},
    {InvitesSenderInternal::kInvitationSettingCount, nullptr}};

InvitesSenderInternalAndroid::InvitesSenderInternalAndroid(
    const ::firebase::App &app)
    : InvitesSenderInternal(app), android(app, this) {
  if (!android.initialized()) app_ = nullptr;
}

bool InvitesSenderInternalAndroid::PerformSendInvite() {
  android.CallMethod(invite::kResetSenderSettings);
  for (int i = 0; kJNIMapping[i].java_key != nullptr; i++) {
    android.CallMethodStringString(
        invite::kSetInvitationOption, kJNIMapping[i].java_key,
        GetInvitationSetting(kJNIMapping[i].native_key));
  }

  android.CallMethod(invite::kClearReferralParams);
  for (auto i = referral_parameters().begin(); i != referral_parameters().end();
       ++i) {
    android.CallMethodStringString(invite::kAddReferralParam, i->first.c_str(),
                                   i->second.c_str());
  }

  return android.CallBooleanMethod(invite::kShowSenderUI);
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase
