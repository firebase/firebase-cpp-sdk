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

#include "gma/src/include/firebase/gma/ad_view.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "gma/src/common/ad_view_internal.h"
#include "gma/src/common/gma_common.h"

namespace firebase {
namespace gma {

AdView::AdView() {
  FIREBASE_ASSERT(gma::IsInitialized());
  internal_ = internal::AdViewInternal::CreateInstance(this);
  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    LogWarning("AdView must be deleted before gma::Terminate.");
    AdView* ad_view = reinterpret_cast<AdView*>(object);
    delete ad_view->internal_;
    ad_view->internal_ = nullptr;
  });
}

AdView::~AdView() {
  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(internal::AdViewInternal* internal) {
  FIREBASE_ASSERT(internal);
  return internal->is_initialized();
}

Future<void> AdView::Initialize(AdParent parent, const char* ad_unit_id,
                                const AdSize& size) {
  return internal_->Initialize(parent, ad_unit_id, size);
}

Future<void> AdView::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kAdViewFnInitialize);
}

void AdView::SetAdListener(AdListener* listener) {
  internal_->SetAdListener(listener);
}

void AdView::SetBoundingBoxListener(AdViewBoundingBoxListener* listener) {
  internal_->SetBoundingBoxListener(listener);
}

void AdView::SetPaidEventListener(PaidEventListener* listener) {
  internal_->SetPaidEventListener(listener);
}

Future<void> AdView::SetPosition(int x, int y) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnSetPosition,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->SetPosition(x, y);
}

Future<void> AdView::SetPosition(Position position) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnSetPosition,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->SetPosition(position);
}

Future<void> AdView::SetPositionLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnSetPosition,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kAdViewFnSetPosition);
}

Future<AdResult> AdView::LoadAd(const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::gma::internal::kAdViewFnLoadAd, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_, AdResult());
  }
  return internal_->LoadAd(request);
}

Future<AdResult> AdView::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::gma::internal::kAdViewFnLoadAd, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &(internal_->future_data_), AdResult());
  }
  return internal_->GetLoadAdLastResult();
}

Future<void> AdView::Hide() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnHide, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Hide();
}

Future<void> AdView::HideLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnHide, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kAdViewFnHide);
}

Future<void> AdView::Show() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnShow, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Show();
}

Future<void> AdView::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnShow, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kAdViewFnShow);
}

Future<void> AdView::Pause() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnPause, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Pause();
}

Future<void> AdView::PauseLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnPause, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kAdViewFnPause);
}

Future<void> AdView::Resume() {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnResume, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->Resume();
}

Future<void> AdView::ResumeLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kAdViewFnResume, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_);
  }
  return internal_->GetLastResult(internal::kAdViewFnResume);
}

Future<void> AdView::Destroy() { return internal_->Destroy(); }

Future<void> AdView::DestroyLastResult() const {
  return internal_->GetLastResult(internal::kAdViewFnDestroy);
}

BoundingBox AdView::bounding_box() const {
  if (!CheckIsInitialized(internal_)) return BoundingBox();
  return internal_->bounding_box();
}

AdSize AdView::ad_size() const { return internal_->ad_size(); }

}  // namespace gma
}  // namespace firebase
