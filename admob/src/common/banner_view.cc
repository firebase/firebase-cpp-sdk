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
  bool initialized =
      internal != nullptr &&
      internal->GetLastResult(internal::kBannerViewFnInitialize).status() ==
          kFutureStatusComplete;
  FIREBASE_ASSERT_MESSAGE_RETURN(false, initialized, kUninitializedError);
  return true;
}

Future<void> BannerView::Initialize(AdParent parent, const char* ad_unit_id,
                                    AdSize size) {
  return internal_->Initialize(parent, ad_unit_id, size);
}

Future<void> BannerView::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kBannerViewFnInitialize);
}

Future<void> BannerView::LoadAd(const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->LoadAd(request);
}

Future<void> BannerView::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnLoadAd);
}

Future<void> BannerView::Hide() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Hide();
}

Future<void> BannerView::HideLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnHide);
}

Future<void> BannerView::Show() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Show();
}

Future<void> BannerView::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnShow);
}

Future<void> BannerView::Pause() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Pause();
}

Future<void> BannerView::PauseLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnPause);
}

Future<void> BannerView::Resume() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Resume();
}

Future<void> BannerView::ResumeLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnResume);
}

Future<void> BannerView::Destroy() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Destroy();
}

Future<void> BannerView::DestroyLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnDestroy);
}

Future<void> BannerView::MoveTo(int x, int y) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->MoveTo(x, y);
}

Future<void> BannerView::MoveTo(Position position) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->MoveTo(position);
}

Future<void> BannerView::MoveToLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kBannerViewFnMoveTo);
}

BannerView::PresentationState BannerView::presentation_state() const {
  if (!CheckIsInitialized(internal_)) return kPresentationStateHidden;
  return internal_->GetPresentationState();
}

BoundingBox BannerView::bounding_box() const {
  if (!CheckIsInitialized(internal_)) return BoundingBox();
  return internal_->GetBoundingBox();
}

void BannerView::SetListener(Listener* listener) {
  if (!CheckIsInitialized(internal_)) return;
  internal_->SetListener(listener);
}

}  // namespace admob
}  // namespace firebase
