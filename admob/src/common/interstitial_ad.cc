/*
 * Copyright 2016 Google LLC
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

#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/common/interstitial_ad_internal.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"

namespace firebase {
namespace admob {

const char kUninitializedError[] =
    "Initialize() must be called before this method.";

InterstitialAd::InterstitialAd() {
  FIREBASE_ASSERT(admob::IsInitialized());
  internal_ = internal::InterstitialAdInternal::CreateInstance(this);
  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    FIREBASE_ASSERT_MESSAGE(
        false, "InterstitialAd must be deleted before admob::Terminate.");
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
  bool initialized =
      internal != nullptr &&
      internal->GetLastResult(internal::kInterstitialAdFnInitialize).status() ==
          kFutureStatusComplete;
  FIREBASE_ASSERT_MESSAGE_RETURN(false, initialized, kUninitializedError);
  return true;
}

Future<void> InterstitialAd::Initialize(AdParent parent,
                                        const char* ad_unit_id) {
  return internal_->Initialize(parent, ad_unit_id);
}

Future<void> InterstitialAd::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kInterstitialAdFnInitialize);
}

Future<void> InterstitialAd::LoadAd(const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->LoadAd(request);
}

Future<void> InterstitialAd::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kInterstitialAdFnLoadAd);
}

Future<void> InterstitialAd::Show() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Show();
}

Future<void> InterstitialAd::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kInterstitialAdFnShow);
}

InterstitialAd::PresentationState InterstitialAd::presentation_state() const {
  if (!CheckIsInitialized(internal_)) return kPresentationStateHidden;
  return internal_->GetPresentationState();
}

void InterstitialAd::SetListener(Listener* listener) {
  if (!CheckIsInitialized(internal_)) return;
  internal_->SetListener(listener);
}

}  // namespace admob
}  // namespace firebase
