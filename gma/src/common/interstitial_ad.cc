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

#include "gma/src/include/firebase/gma/interstitial_ad.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/common/interstitial_ad_internal.h"

namespace firebase {
namespace gma {

InterstitialAd::InterstitialAd() {
  FIREBASE_ASSERT(gma::IsInitialized());
  internal_ = internal::InterstitialAdInternal::CreateInstance(this);

  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    LogWarning("InterstitialAd must be deleted before gma::Terminate.");
    InterstitialAd* interstitial_ad = reinterpret_cast<InterstitialAd*>(object);
    delete interstitial_ad->internal_;
    interstitial_ad->internal_ = nullptr;
  });
}

InterstitialAd::~InterstitialAd() {
  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(internal::InterstitialAdInternal* internal) {
  FIREBASE_ASSERT(internal);
  return internal->is_initialized();
}

Future<void> InterstitialAd::Initialize(AdParent parent) {
  return internal_->Initialize(parent);
}

Future<void> InterstitialAd::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kInterstitialAdFnInitialize);
}

Future<AdResult> InterstitialAd::LoadAd(const char* ad_unit_id,
                                        const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::gma::internal::kInterstitialAdFnLoadAd,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, AdResult());
  }

  return internal_->LoadAd(ad_unit_id, request);
}

Future<AdResult> InterstitialAd::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::gma::internal::kInterstitialAdFnLoadAd,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, AdResult());
  }
  return internal_->GetLoadAdLastResult();
}

Future<void> InterstitialAd::Show() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kInterstitialAdFnShow,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->Show();
}

Future<void> InterstitialAd::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kInterstitialAdFnShow,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kInterstitialAdFnShow);
}

void InterstitialAd::SetFullScreenContentListener(
    FullScreenContentListener* listener) {
  internal_->SetFullScreenContentListener(listener);
}

void InterstitialAd::SetPaidEventListener(PaidEventListener* listener) {
  internal_->SetPaidEventListener(listener);
}

}  // namespace gma
}  // namespace firebase
