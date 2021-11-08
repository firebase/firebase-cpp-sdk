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

#include "admob/src/include/firebase/admob/banner_view.h"

#include "admob/src/common/admob_common.h"
#include "admob/src/common/banner_view_internal.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"

namespace firebase {
namespace admob {

const char kUninitializedError[] =
    "Initialize() must be called before this method.";

BannerView::BannerView() {
  FIREBASE_ASSERT(admob::IsInitialized());
  internal_ = internal::BannerViewInternal::CreateInstance(this);
  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    FIREBASE_ASSERT_MESSAGE(
        false, "BannerView must be deleted before admob::Terminate.");
    BannerView* banner_view = reinterpret_cast<BannerView*>(object);
    delete banner_view->internal_;
    banner_view->internal_ = nullptr;
  });
}

BannerView::~BannerView() {
  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(internal::BannerViewInternal* internal) {
  FIREBASE_ASSERT(internal);
  return internal->is_initialized();
}

Future<void> BannerView::Initialize(AdParent parent, const char* ad_unit_id,
                                    const AdSize& size) {
  return internal_->Initialize(parent, ad_unit_id, size);
}

Future<void> BannerView::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kBannerViewFnInitialize);
}

void BannerView::SetAdListener(AdListener* listener) {
  internal_->SetAdListener(listener);
}

void BannerView::SetBoundingBoxListener(AdViewBoundingBoxListener* listener) {
  internal_->SetBoundingBoxListener(listener);
}

void BannerView::SetPaidEventListener(PaidEventListener* listener) {
  internal_->SetPaidEventListener(listener);
}

Future<void> BannerView::SetPosition(int x, int y) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnSetPosition,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->SetPosition(x, y);
}

Future<void> BannerView::SetPosition(Position position) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnSetPosition,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->SetPosition(position);
}

Future<void> BannerView::SetPositionLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnSetPosition,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kBannerViewFnSetPosition);
}

Future<AdResult> BannerView::LoadAd(const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::admob::internal::kBannerViewFnLoadAd,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, AdResult());
  }
  return internal_->LoadAd(request);
}

Future<AdResult> BannerView::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::admob::internal::kBannerViewFnLoadAd,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &(internal_->future_data_), AdResult());
  }
  return internal_->GetLoadAdLastResult();
}

Future<void> BannerView::Hide() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnHide, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Hide();
}

Future<void> BannerView::HideLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnHide, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kBannerViewFnHide);
}

Future<void> BannerView::Show() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnShow, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Show();
}

Future<void> BannerView::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnShow, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kBannerViewFnShow);
}

Future<void> BannerView::Pause() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnPause, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Pause();
}

Future<void> BannerView::PauseLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnPause, kAdMobErrorUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kBannerViewFnPause);
}

Future<void> BannerView::Resume() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnResume,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->Resume();
}

Future<void> BannerView::ResumeLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::admob::internal::kBannerViewFnResume,
        kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kBannerViewFnResume);
}

Future<void> BannerView::Destroy() { return internal_->Destroy(); }

Future<void> BannerView::DestroyLastResult() const {
  return internal_->GetLastResult(internal::kBannerViewFnDestroy);
}

BoundingBox BannerView::bounding_box() const {
  if (!CheckIsInitialized(internal_)) return BoundingBox();
  return internal_->bounding_box();
}

}  // namespace admob
}  // namespace firebase
