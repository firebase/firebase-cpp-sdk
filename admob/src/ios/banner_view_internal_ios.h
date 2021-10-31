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

#ifndef FIREBASE_ADMOB_SRC_IOS_BANNER_VIEW_INTERNAL_IOS_H_
#define FIREBASE_ADMOB_SRC_IOS_BANNER_VIEW_INTERNAL_IOS_H_

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#ifdef __OBJC__
#import <GoogleMobileAds/GoogleMobileAds.h>
#endif  // __OBJC__

#include "admob/src/common/banner_view_internal.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace internal {

class BannerViewInternalIOS : public BannerViewInternal {
 public:
  BannerViewInternalIOS(BannerView* base);
  ~BannerViewInternalIOS();

  Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                          const AdSize& size) override;
  Future<LoadAdResult> LoadAd(const AdRequest& request) override;
  BoundingBox bounding_box() const override;
  Future<void> SetPosition(int x, int y) override;
  Future<void> SetPosition(BannerView::Position position) override;
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
  void BannerViewDidReceiveAd();
  void BannerViewDidFailToReceiveAdWithError(NSError *gad_error);
#endif  // __OBJC__

 private:
  /// Contains information to asynchronously complete the LoadAd Future.
  FutureCallbackData<LoadAdResult>* ad_load_callback_data_;

  /// Prevents duplicate invocations of initailize on the BannerView.
  bool initialized_;

  /// The FADBannerView object. Declared as an "id" type to avoid referencing an
  /// Objective-C++ class in this header.
  id banner_view_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;

  /// A mutex used to handle the destroy behavior, as it is asynchronous,
  /// and needs to be waited on in the destructor.
  Mutex destroy_mutex_;

  /// A cached bounding box from the last update, accessible for processes
  /// running on non-UI threads.
  BoundingBox bounding_box_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_IOS_BANNER_VIEW_INTERNAL_IOS_H_
