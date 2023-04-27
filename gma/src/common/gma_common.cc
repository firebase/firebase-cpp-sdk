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

#include "gma/src/common/gma_common.h"

#include <assert.h>

#include <string>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"
#include "gma/src/common/ad_view_internal.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/ad_view.h"
#include "gma/src/include/firebase/gma/interstitial_ad.h"
#include "gma/src/include/firebase/gma/rewarded_ad.h"
#include "gma/src/include/firebase/gma/types.h"

FIREBASE_APP_REGISTER_CALLBACKS(
    gma,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::InitResult result;
        firebase::gma::Initialize(*app, &result);
        return result;
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::gma::Terminate();
      }
    },
    false);

namespace firebase {
namespace gma {

DEFINE_FIREBASE_VERSION_STRING(FirebaseGma);

static CleanupNotifier* g_cleanup_notifier = nullptr;
const char kGmaModuleName[] = "gma";

// Error messages used for completing futures. These match the error codes in
// the AdError enumeration in the C++ API.
const char* kAdAlreadyInitializedErrorMessage = "Ad is already initialized.";
const char* kAdCouldNotParseAdRequestErrorMessage =
    "Could Not Parse AdRequest.";
const char* kAdLoadInProgressErrorMessage = "Ad is currently loading.";
const char* kAdUninitializedErrorMessage = "Ad has not been fully initialized.";

// GmaInternal
void GmaInternal::CompleteLoadAdFutureSuccess(
    FutureCallbackData<AdResult>* callback_data,
    const ResponseInfoInternal& response_info_internal) {
  callback_data->future_data->future_impl.CompleteWithResult(
      callback_data->future_handle, static_cast<int>(kAdErrorCodeNone), "",
      AdResult(ResponseInfo(response_info_internal)));
  delete callback_data;
}

void GmaInternal::CompleteLoadAdFutureFailure(
    FutureCallbackData<AdResult>* callback_data, int error_code,
    const std::string& error_message,
    const AdErrorInternal& ad_error_internal) {
  callback_data->future_data->future_impl.CompleteWithResult(
      callback_data->future_handle, static_cast<int>(error_code),
      error_message.c_str(), AdResult(CreateAdError(ad_error_internal)));
  // This method is responsible for disposing of the callback data struct.
  delete callback_data;
}

AdError GmaInternal::CreateAdError(const AdErrorInternal& ad_error_internal) {
  return AdError(ad_error_internal);
}

void GmaInternal::UpdateAdViewInternalAdSizeDimensions(
    internal::AdViewInternal* ad_view_internal, int width, int height) {
  assert(ad_view_internal);
  ad_view_internal->update_ad_size_dimensions(width, height);
}

// AdInspectorClosedListener
AdInspectorClosedListener::~AdInspectorClosedListener() {}

// AdResult
AdResult::AdResult() : is_successful_(true) {}
AdResult::AdResult(const AdError& ad_error)
    : is_successful_(false), ad_error_(ad_error) {}
AdResult::AdResult(const ResponseInfo& response_info)
    : is_successful_(true), response_info_(response_info) {}

AdResult::~AdResult() {}
bool AdResult::is_successful() const { return is_successful_; }
const AdError& AdResult::ad_error() const { return ad_error_; }
const ResponseInfo& AdResult::response_info() const { return response_info_; }

// AdSize
// Hardcoded values are from publicly available documentation:
// https://developers.google.com/android/reference/com/google/android/gms/ads/AdSize
// A dynamic resolution of thes values creates a lot of Android code,
// and these are standards that are not likely to change.
const AdSize AdSize::kBanner(/*width=*/320, /*height=*/50);
const AdSize AdSize::kFullBanner(468, 60);
const AdSize AdSize::kLargeBanner(320, 100);
const AdSize AdSize::kLeaderboard(728, 90);
const AdSize AdSize::kMediumRectangle(300, 250);

AdSize::AdSize(uint32_t width, uint32_t height)
    : orientation_(AdSize::kOrientationCurrent),
      width_(width),
      height_(height),
      type_(AdSize::kTypeStandard) {}

AdSize AdSize::GetAnchoredAdaptiveBannerAdSize(uint32_t width,
                                               Orientation orientation) {
  AdSize ad_size(width, 0);
  ad_size.type_ = AdSize::kTypeAnchoredAdaptive;
  ad_size.orientation_ = orientation;
  return ad_size;
}

AdSize AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(uint32_t width) {
  return GetAnchoredAdaptiveBannerAdSize(width, AdSize::kOrientationLandscape);
}

AdSize AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(uint32_t width) {
  return GetAnchoredAdaptiveBannerAdSize(width, AdSize::kOrientationPortrait);
}

AdSize AdSize::GetCurrentOrientationAnchoredAdaptiveBannerAdSize(
    uint32_t width) {
  return GetAnchoredAdaptiveBannerAdSize(width, AdSize::kOrientationCurrent);
}

AdSize AdSize::GetCurrentOrientationInlineAdaptiveBannerAdSize(int width) {
  AdSize ad_size(width, 0);
  ad_size.type_ = AdSize::kTypeInlineAdaptive;
  ad_size.orientation_ = AdSize::kOrientationCurrent;
  return ad_size;
}

AdSize AdSize::GetInlineAdaptiveBannerAdSize(int width, int max_height) {
  AdSize ad_size(width, max_height);
  ad_size.type_ = AdSize::kTypeInlineAdaptive;
  return ad_size;
}

AdSize AdSize::GetLandscapeInlineAdaptiveBannerAdSize(int width) {
  AdSize ad_size(width, 0);
  ad_size.type_ = AdSize::kTypeInlineAdaptive;
  ad_size.orientation_ = AdSize::kOrientationLandscape;
  return ad_size;
}

AdSize AdSize::GetPortraitInlineAdaptiveBannerAdSize(int width) {
  AdSize ad_size(width, 0);
  ad_size.type_ = AdSize::kTypeInlineAdaptive;
  ad_size.orientation_ = AdSize::kOrientationPortrait;
  return ad_size;
}

bool AdSize::is_equal(const AdSize& ad_size) const {
  return (type_ == ad_size.type_) && (width_ == ad_size.width_) &&
         (height_ == ad_size.height_) && (orientation_ == ad_size.orientation_);
}

bool AdSize::operator==(const AdSize& rhs) const { return is_equal(rhs); }

bool AdSize::operator!=(const AdSize& rhs) const { return !is_equal(rhs); }

// AdRequest
// Method implementations of AdRequest which are platform independent.
AdRequest::AdRequest() {}
AdRequest::~AdRequest() {}

AdRequest::AdRequest(const char* content_url) { set_content_url(content_url); }

void AdRequest::add_extra(const char* adapter_class_name, const char* extra_key,
                          const char* extra_value) {
  if (adapter_class_name != nullptr && extra_key != nullptr &&
      extra_value != nullptr) {
    extras_[std::string(adapter_class_name)][std::string(extra_key)] =
        std::string(extra_value);
  }
}

void AdRequest::add_keyword(const char* keyword) {
  if (keyword != nullptr) {
    keywords_.insert(std::string(keyword));
  }
}

void AdRequest::set_content_url(const char* content_url) {
  if (content_url == nullptr) {
    return;
  }
  std::string url(content_url);
  if (url.size() <= 512) {
    content_url_ = url;
  }
}

void AdRequest::add_neighboring_content_urls(
    const std::vector<std::string>& neighboring_content_urls) {
  for (auto urls = neighboring_content_urls.begin();
       urls != neighboring_content_urls.end(); ++urls) {
    neighboring_content_urls_.insert(*urls);
  }
}

// Non-inline implementation of the virtual destructors, to prevent
// their vtables from being emitted in each translation unit.
AdapterResponseInfo::~AdapterResponseInfo() {}
AdListener::~AdListener() {}
AdViewBoundingBoxListener::~AdViewBoundingBoxListener() {}
FullScreenContentListener::~FullScreenContentListener() {}
PaidEventListener::~PaidEventListener() {}
UserEarnedRewardListener::~UserEarnedRewardListener() {}

void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kGmaModuleName)) {
    // It's possible to initialize GMA without firebase::App so only register
    // for cleanup notifications if the default app exists.
    App* app = App::GetInstance();
    if (app) {
      CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(app);
      assert(cleanup_notifier);
      cleanup_notifier->RegisterObject(const_cast<char*>(kGmaModuleName),
                                       [](void*) {
                                         if (firebase::gma::IsInitialized()) {
                                           firebase::gma::Terminate();
                                         }
                                       });
    }
  }
}

void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kGmaModuleName)) {
    App* app = App::GetInstance();
    if (app) {
      CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(app);
      assert(cleanup_notifier);
      cleanup_notifier->UnregisterObject(const_cast<char*>(kGmaModuleName));
    }
  }
}

CleanupNotifier* GetOrCreateCleanupNotifier() {
  if (!g_cleanup_notifier) {
    g_cleanup_notifier = new CleanupNotifier();
  }
  return g_cleanup_notifier;
}

void DestroyCleanupNotifier() {
  delete g_cleanup_notifier;
  g_cleanup_notifier = nullptr;
}

const char* GetRequestAgentString() {
  // This is a string that can be used to uniquely identify requests coming
  // from this version of the library.
  return "firebase-cpp-api." FIREBASE_VERSION_NUMBER_STRING;
}

// Mark a future as complete.
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<void> handle, FutureData* future_data) {
  if (future_data->future_impl.ValidFuture(handle)) {
    future_data->future_impl.Complete(handle, error, error_msg);
  }
}

// For calls that aren't asynchronous, we can create and complete at the
// same time.
Future<void> CreateAndCompleteFuture(int fn_idx, int error,
                                     const char* error_msg,
                                     FutureData* future_data) {
  SafeFutureHandle<void> handle = CreateFuture<void>(fn_idx, future_data);
  CompleteFuture(error, error_msg, handle, future_data);
  return MakeFuture(&future_data->future_impl, handle);
}

Future<AdResult> CreateAndCompleteFutureWithResult(int fn_idx, int error,
                                                   const char* error_msg,
                                                   FutureData* future_data,
                                                   const AdResult& result) {
  SafeFutureHandle<AdResult> handle =
      CreateFuture<AdResult>(fn_idx, future_data);
  CompleteFuture(error, error_msg, handle, future_data, result);
  return MakeFuture(&future_data->future_impl, handle);
}

FutureCallbackData<void>* CreateVoidFutureCallbackData(
    int fn_idx, FutureData* future_data) {
  return new FutureCallbackData<void>{
      future_data, future_data->future_impl.SafeAlloc<void>(fn_idx)};
}

FutureCallbackData<AdResult>* CreateAdResultFutureCallbackData(
    int fn_idx, FutureData* future_data) {
  return new FutureCallbackData<AdResult>{
      future_data,
      future_data->future_impl.SafeAlloc<AdResult>(fn_idx, AdResult())};
}

}  // namespace gma
}  // namespace firebase
