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

#ifndef FIREBASE_GMA_SRC_IOS_AD_VIEW_INTERNAL_IOS_H_
#define FIREBASE_GMA_SRC_IOS_AD_VIEW_INTERNAL_IOS_H_

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#ifdef __OBJC__
#import <GoogleMobileAds/GoogleMobileAds.h>
#endif  // __OBJC__

#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/ad_view_internal.h"

namespace firebase {
namespace gma {
namespace internal {

class AdViewInternalIOS : public AdViewInternal {
 public:
  explicit AdViewInternalIOS(AdView* base);
  ~AdViewInternalIOS();

  Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                          const AdSize& size) override;
  Future<AdResult> LoadAd(const AdRequest& request) override;
  BoundingBox bounding_box() const override;
  Future<void> SetPosition(int x, int y) override;
  Future<void> SetPosition(AdView::Position position) override;
  Future<void> Hide() override;
  Future<void> Show() override;
  Future<void> Pause() override;
  Future<void> Resume() override;
  Future<void> Destroy() override;
  bool is_initialized() const override { return initialized_; }
  void set_bounding_box(const BoundingBox& bounding_box) {
    bounding_box_ = bounding_box;
  }

#ifdef __OBJC__
  void AdViewDidReceiveAd(int width, int height,
                          GADResponseInfo* gad_response_info);
  void AdViewDidFailToReceiveAdWithError(NSError* gad_error);
#endif  // __OBJC__

 private:
  /// Contains information to asynchronously complete the LoadAd Future.
  FutureCallbackData<AdResult>* ad_load_callback_data_;

  /// The FADAdView object. Declared as an "id" type to avoid referencing an
  /// Objective-C++ class in this header.
  id ad_view_;

  /// A cached bounding box from the last update, accessible for processes
  /// running on non-UI threads.
  BoundingBox bounding_box_;

  /// A mutex used to handle the destroy behavior, as it is asynchronous,
  /// and needs to be waited on in the destructor.
  Mutex destroy_mutex_;

  /// Prevents duplicate invocations of initialize on the AdView.
  bool initialized_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_AD_VIEW_INTERNAL_IOS_H_
