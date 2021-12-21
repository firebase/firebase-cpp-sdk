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

#ifndef FIREBASE_ADMOB_SRC_IOS_NATIVE_EXPRESS_AD_VIEW_INTERNAL_IOS_H_
#define FIREBASE_ADMOB_SRC_IOS_NATIVE_EXPRESS_AD_VIEW_INTERNAL_IOS_H_

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include "admob/src/common/native_express_ad_view_internal.h"

namespace firebase {
namespace admob {
namespace internal {

class NativeExpressAdViewInternalIOS : public NativeExpressAdViewInternal {
 public:
  NativeExpressAdViewInternalIOS(NativeExpressAdView* base);
  ~NativeExpressAdViewInternalIOS();

  Future<void> Initialize(AdParent parent, const char* ad_unit_id, AdSize size)
      override;
  Future<void> LoadAd(const AdRequest& request) override;
  Future<void> Hide() override;
  Future<void> Show() override;
  Future<void> Pause() override;
  Future<void> Resume() override;
  Future<void> Destroy() override;
  Future<void> MoveTo(int x, int y) override;
  Future<void> MoveTo(NativeExpressAdView::Position position) override;

  NativeExpressAdView::PresentationState GetPresentationState() const override;
  BoundingBox GetBoundingBox() const override;

  /// Completes the future for the LoadAd function.
  void CompleteLoadFuture(admob::AdMobError error, const char* error_msg);

 private:
  /// The handle to the future for the last call to LoadAd. This call is
  /// different than the other asynchronous calls because it's completed in
  /// separate functions (the others are completed by blocks a.k.a. lambdas or
  /// closures).
  FutureHandle future_handle_for_load_;

  /// The FADNativeExpressAdView object. Declared as an "id" type to avoid
  /// referencing an Objective-C++ class in this header.
  id native_express_ad_view_;

  /// A mutex used to handle the destroy behavior, as it is asynchronous,
  /// and needs to be waited on in the destructor.
  Mutex destroy_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_IOS_NATIVE_EXPRESS_AD_VIEW_INTERNAL_IOS_H_
