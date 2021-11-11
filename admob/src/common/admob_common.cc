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

#include "admob/src/common/admob_common.h"

#include <assert.h>

#include <string>
#include <vector>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/banner_view.h"
#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "admob/src/include/firebase/admob/rewarded_ad.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"

FIREBASE_APP_REGISTER_CALLBACKS(
    admob,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::InitResult result;
        firebase::admob::Initialize(*app, &result);
        return result;
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::admob::Terminate();
      }
    });

namespace firebase {
namespace admob {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAdMob);

static CleanupNotifier* g_cleanup_notifier = nullptr;
const char kAdMobModuleName[] = "admob";

// Error messages used for completing futures. These match the error codes in
// the AdMobError enumeration in the C++ API.
const char* kAdAlreadyInitializedErrorMessage = "Ad is already initialized.";
const char* kAdCouldNotParseAdRequestErrorMessage =
    "Could Not Parse AdRequest.";
const char* kAdLoadInProgressErrorMessage = "Ad is currently loading.";
const char* kAdUninitializedErrorMessage = "Ad has not been fully initialized.";

// AdMobInternal
void AdMobInternal::CompleteLoadAdFuture(
    FutureCallbackData<AdResult>* callback_data, int error_code,
    const std::string& error_message,
    const AdResultInternal& ad_result_internal) {
  callback_data->future_data->future_impl.CompleteWithResult(
      callback_data->future_handle, static_cast<int>(error_code),
      error_message.c_str(), AdResult(ad_result_internal));
  // This method is responsible for disposing of the callback data struct.
  delete callback_data;
}

AdResult AdMobInternal::CreateAdResult(
    const AdResultInternal& ad_result_internal) {
  return AdResult(ad_result_internal);
}

// AdSize
// Hardcoded values are from publicly available documentation:
// https://developers.google.com/android/reference/com/google/android/gms/ads/AdSize
// A dynamic resolution of thes values creates a lot of Android code,
// and these are standards that are not likely to change.
const AdSize AdSize::kBanner(/*width=*/320, /*height=*/50);
const AdSize AdSize::kFullBanner(468, 60);
const AdSize AdSize::kLargeBanner(320, 100);
const AdSize AdSize::kLeaderBoard(728, 90);
const AdSize AdSize::kMediumRectangle(300, 250);

AdSize::AdSize(uint32_t width, uint32_t height)
    : width_(width),
      height_(height),
      type_(AdSize::kTypeStandard),
      orientation_(AdSize::kOrientationCurrent) {}

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
void AdRequest::add_extra(const char* ad_network, const char* extra_key,
                          const char* extra_value) {
  if (ad_network != nullptr && extra_key != nullptr && extra_value != nullptr) {
    extras_[std::string(ad_network)][std::string(extra_key)] =
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

// AdView
// Method implementations of AdView which are platform independent.
void AdView::SetAdListener(AdListener* listener) { ad_listener_ = listener; }

void AdView::SetBoundingBoxListener(AdViewBoundingBoxListener* listener) {
  ad_view_bounding_box_listener_ = listener;
}

void AdView::SetPaidEventListener(PaidEventListener* listener) {
  paid_event_listener_ = listener;
}

// Non-inline implementation of the virtual destructors, to prevent
// their vtables from being emitted in each translation unit.
AdListener::~AdListener() {}
AdView::~AdView() {}
AdViewBoundingBoxListener::~AdViewBoundingBoxListener() {}
FullScreenContentListener::~FullScreenContentListener() {}
PaidEventListener::~PaidEventListener() {}
UserEarnedRewardListener::~UserEarnedRewardListener() {}

void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAdMobModuleName)) {
    // It's possible to initialize AdMob without firebase::App so only register
    // for cleanup notifications if the default app exists.
    App* app = App::GetInstance();
    if (app) {
      CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(app);
      assert(cleanup_notifier);
      cleanup_notifier->RegisterObject(const_cast<char*>(kAdMobModuleName),
                                       [](void*) {
                                         if (firebase::admob::IsInitialized()) {
                                           firebase::admob::Terminate();
                                         }
                                       });
    }
  }
}

void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAdMobModuleName)) {
    App* app = App::GetInstance();
    if (app) {
      CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(app);
      assert(cleanup_notifier);
      cleanup_notifier->UnregisterObject(const_cast<char*>(kAdMobModuleName));
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
  future_data->future_impl.Complete(handle, error, error_msg);
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

}  // namespace admob
}  // namespace firebase
