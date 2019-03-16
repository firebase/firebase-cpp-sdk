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

#include "admob/src/include/firebase/admob/native_express_ad_view.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/common/native_express_ad_view_internal.h"
#include "app/src/include/firebase/future.h"

namespace firebase {
namespace admob {

const char kUninitializedError[] =
    "Initialize() must be called before this method.";

NativeExpressAdView::NativeExpressAdView() {
  FIREBASE_ASSERT(admob::IsInitialized());
  internal_ = internal::NativeExpressAdViewInternal::CreateInstance(this);
  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    FIREBASE_ASSERT_MESSAGE(
        false,
        "NativeExpressAdView must be deleted before "
        "admob::Terminate is called.  0x%08x is deleted.",
        static_cast<int>(reinterpret_cast<intptr_t>(object)));
    NativeExpressAdView* native_express_ad_view =
        reinterpret_cast<NativeExpressAdView*>(object);
    delete native_express_ad_view->internal_;
    native_express_ad_view->internal_ = nullptr;
  });
}

NativeExpressAdView::~NativeExpressAdView() {
  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(
    internal::NativeExpressAdViewInternal* internal) {
  bool initialized =
      internal != nullptr &&
      internal->GetLastResult(internal::kNativeExpressAdViewFnInitialize)
              .status() == kFutureStatusComplete;
  FIREBASE_ASSERT_MESSAGE_RETURN(false, initialized, kUninitializedError);
  return true;
}

Future<void> NativeExpressAdView::Initialize(AdParent parent,
                                             const char* ad_unit_id,
                                             AdSize size) {
  return internal_->Initialize(parent, ad_unit_id, size);
}

Future<void> NativeExpressAdView::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnInitialize);
}

Future<void> NativeExpressAdView::LoadAd(const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->LoadAd(request);
}

Future<void> NativeExpressAdView::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnLoadAd);
}

Future<void> NativeExpressAdView::Hide() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Hide();
}

Future<void> NativeExpressAdView::HideLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnHide);
}

Future<void> NativeExpressAdView::Show() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Show();
}

Future<void> NativeExpressAdView::ShowLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnShow);
}

Future<void> NativeExpressAdView::Pause() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Pause();
}

Future<void> NativeExpressAdView::PauseLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnPause);
}

Future<void> NativeExpressAdView::Resume() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Resume();
}

Future<void> NativeExpressAdView::ResumeLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnResume);
}

Future<void> NativeExpressAdView::Destroy() {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->Destroy();
}

Future<void> NativeExpressAdView::DestroyLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnDestroy);
}

Future<void> NativeExpressAdView::MoveTo(int x, int y) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->MoveTo(x, y);
}

Future<void> NativeExpressAdView::MoveTo(Position position) {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->MoveTo(position);
}

Future<void> NativeExpressAdView::MoveToLastResult() const {
  if (!CheckIsInitialized(internal_)) return Future<void>();
  return internal_->GetLastResult(internal::kNativeExpressAdViewFnMoveTo);
}

NativeExpressAdView::PresentationState
NativeExpressAdView::GetPresentationState() const {
  if (!CheckIsInitialized(internal_)) return kPresentationStateHidden;
  return internal_->GetPresentationState();
}

BoundingBox NativeExpressAdView::GetBoundingBox() const {
  if (!CheckIsInitialized(internal_)) return BoundingBox();
  return internal_->GetBoundingBox();
}

void NativeExpressAdView::SetListener(Listener* listener) {
  if (!CheckIsInitialized(internal_)) return;
  internal_->SetListener(listener);
}

}  // namespace admob
}  // namespace firebase
