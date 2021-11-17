/*
 * Copyright 2021 Google LLC
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

#include "admob/src/ios/rewarded_ad_internal_ios.h"

#import "admob/src/ios/FADRequest.h"

#import "admob/src/ios/admob_ios.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace admob {
namespace internal {

RewardedAdInternalIOS::RewardedAdInternalIOS(RewardedAd* base)
    : RewardedAdInternal(base), initialized_(false),
    ad_load_callback_data_(nil), rewarded_ad_(nil),
    parent_view_(nil), rewarded_ad_delegate_(nil) {}

RewardedAdInternalIOS::~RewardedAdInternalIOS() { }

Future<void> RewardedAdInternalIOS::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);
  return CreateAndCompleteFuture(
        kRewardedAdFnInitialize, kAdMobErrorNone, nullptr, &future_data_);
}

Future<AdResult> RewardedAdInternalIOS::LoadAd(
    const char* ad_unit_id, const AdRequest& request) {
  return CreateAndCompleteFutureWithResult(
    kRewardedAdFnLoadAd, kAdMobErrorNone, nullptr, &future_data_, AdResult());
}

Future<void> RewardedAdInternalIOS::Show(UserEarnedRewardListener* listener) {
   return CreateAndCompleteFuture(
        kRewardedAdFnShow, kAdMobErrorNone, nullptr, &future_data_);
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
