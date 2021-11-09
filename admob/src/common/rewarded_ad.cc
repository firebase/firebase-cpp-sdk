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

#include "admob/src/include/firebase/admob/rewarded_ad.h"

#include "admob/src/common/admob_common.h"
#include "admob/src/common/rewarded_ad_internal.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"

namespace firebase {
namespace admob {

const char kUninitializedError[] =
    "Initialize() must be called before this method.";

RewardedAd::RewardedAd() {
  FIREBASE_ASSERT(admob::IsInitialized());
  internal_ = internal::RewardedAdInternal::CreateInstance(this);

  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    FIREBASE_ASSERT_MESSAGE(
        false, "RewardedAd must be deleted before admob::Terminate.");
    RewardedAd* rewarded_ad = reinterpret_cast<RewardedAd*>(object);
    delete rewarded_ad->internal_;
    rewarded_ad->internal_ = nullptr;
  });
}

RewardedAd::~RewardedAd() {
  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(internal::RewardedAdInternal* internal) {
  FIREBASE_ASSERT(internal);
  return internal->is_initialized();
}

Future<void> RewardedAd::Initialize(AdParent parent) {
  return internal_->Initialize(parent);
}

Future<void> RewardedAd::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kRewardedAdFnInitialize);
}

Future<AdResult> RewardedAd::LoadAd(const char* ad_unit_id,
                                    const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::admob::internal::kRewardedAdFnLoadAd,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, AdResult());
  }
  return internal_->LoadAd(ad_unit_id, request);
}

Future<AdResult> RewardedAd::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::admob::internal::kRewardedAdFnLoadAd,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, AdResult());
  }
  return internal_->GetLoadAdLastResult();
}

Future<void> RewardedAd::Show(UserEarnedRewardListener* listener) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kRewardedAdFnShow, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Show(listener);
}

Future<void> RewardedAd::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kRewardedAdFnShow, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kRewardedAdFnShow);
}

void RewardedAd::SetFullScreenContentListener(
    FullScreenContentListener* listener) {
  internal_->SetFullScreenContentListener(listener);
}

void RewardedAd::SetPaidEventListener(PaidEventListener* listener) {
  internal_->SetPaidEventListener(listener);
}

}  // namespace admob
}  // namespace firebase
