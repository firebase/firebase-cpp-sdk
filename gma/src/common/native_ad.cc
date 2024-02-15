/*
 * Copyright 2023 Google LLC
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

#include "gma/src/include/firebase/gma/internal/native_ad.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/common/native_ad_internal.h"

namespace firebase {
namespace gma {

NativeAd::NativeAd() {
  FIREBASE_ASSERT(gma::IsInitialized());
  internal_ = internal::NativeAdInternal::CreateInstance(this);

  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    LogWarning("NativeAd must be deleted before gma::Terminate.");
    NativeAd* native_ad = reinterpret_cast<NativeAd*>(object);
    delete native_ad->internal_;
    native_ad->internal_ = nullptr;
  });
}

NativeAd::~NativeAd() {
  FIREBASE_ASSERT(internal_);

  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(internal::NativeAdInternal* internal) {
  FIREBASE_ASSERT(internal);
  return internal->is_initialized();
}

Future<void> NativeAd::Initialize(AdParent parent) {
  return internal_->Initialize(parent);
}

Future<void> NativeAd::InitializeLastResult() const {
  return internal_->GetLastResult(internal::kNativeAdFnInitialize);
}

Future<AdResult> NativeAd::LoadAd(const char* ad_unit_id,
                                  const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::gma::internal::kNativeAdFnLoadAd, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_, AdResult());
  }

  return internal_->LoadAd(ad_unit_id, request);
}

Future<AdResult> NativeAd::LoadAdLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithResult(
        firebase::gma::internal::kNativeAdFnLoadAd, kAdErrorCodeUninitialized,
        kAdUninitializedErrorMessage, &internal_->future_data_, AdResult());
  }
  return internal_->GetLoadAdLastResult();
}

void NativeAd::SetAdListener(AdListener* listener) {
  internal_->SetAdListener(listener);
}

const NativeAdImage& NativeAd::icon() const { return internal_->icon(); }

const std::vector<NativeAdImage>& NativeAd::images() const {
  return internal_->images();
}

const NativeAdImage& NativeAd::adchoices_icon() const {
  return internal_->adchoices_icon();
}

Future<void> NativeAd::RecordImpression(const Variant& impression_data) {
  if (!impression_data.is_map()) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kNativeAdFnRecordImpression,
        kAdErrorCodeInvalidArgument, kUnsupportedVariantTypeErrorMessage,
        &internal_->future_data_);
  }
  return internal_->RecordImpression(impression_data);
}

Future<void> NativeAd::RecordImpressionLastResult() const {
  return internal_->GetLastResult(
      firebase::gma::internal::kNativeAdFnRecordImpression);
}

Future<void> NativeAd::PerformClick(const Variant& click_data) {
  if (!click_data.is_map()) {
    return CreateAndCompleteFuture(
        firebase::gma::internal::kNativeAdFnPerformClick,
        kAdErrorCodeInvalidArgument, kUnsupportedVariantTypeErrorMessage,
        &internal_->future_data_);
  }
  return internal_->PerformClick(click_data);
}

Future<void> NativeAd::PerformClickLastResult() const {
  return internal_->GetLastResult(
      firebase::gma::internal::kNativeAdFnPerformClick);
}

}  // namespace gma
}  // namespace firebase
