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

#include <vector>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/banner_view.h"
#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"

FIREBASE_APP_REGISTER_CALLBACKS(
    admob,
    {
      if (app == ::firebase::App::GetInstance()) {
        return firebase::admob::Initialize(*app);
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

// Error messages used for completing futures. These match the error codes in
// the AdMobError enumeration in the C++ API.
const char* kAdUninitializedErrorMessage = "Ad has not been fully initialized.";
const char* kAdLoadInProgressErrorMessage = "Ad is currently loading.";
const char* kInternalSDKErrorMesage = "An internal SDK error occurred.";

const char* GetRequestAgentString() {
  // This is a string that can be used to uniquely identify requests coming
  // from this version of the library.
  return "firebase-cpp-api." FIREBASE_VERSION_NUMBER_STRING;
}

// Create a future and update the corresponding last result.
FutureHandle CreateFuture(int fn_idx, FutureData* future_data) {
  return future_data->future_impl.Alloc<void>(fn_idx);
}

// Mark a future as complete.
void CompleteFuture(int error, const char* error_msg, FutureHandle handle,
                    FutureData* future_data) {
  future_data->future_impl.Complete(handle, error, error_msg);
}

// For calls that aren't asynchronous, we can create and complete at the
// same time.
void CreateAndCompleteFuture(int fn_idx, int error, const char* error_msg,
                             FutureData* future_data) {
  FutureHandle handle = CreateFuture(fn_idx, future_data);
  CompleteFuture(error, error_msg, handle, future_data);
}

FutureCallbackData* CreateFutureCallbackData(FutureData* future_data,
                                             int fn_idx) {
  FutureCallbackData* callback_data = new FutureCallbackData();
  callback_data->future_handle = future_data->future_impl.Alloc<void>(fn_idx);
  callback_data->future_data = future_data;
  return callback_data;
}

// Non-inline implementation of the Listeners' virtual destructors, to prevent
// their vtables from being emitted in each translation unit.
BannerView::Listener::~Listener() {}
InterstitialAd::Listener::~Listener() {}

}  // namespace admob
}  // namespace firebase
