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

#ifndef FIREBASE_GMA_SRC_IOS_NATIVE_AD_INTERNAL_IOS_H_
#define FIREBASE_GMA_SRC_IOS_NATIVE_AD_INTERNAL_IOS_H_

#ifdef __OBJC__
#import "gma/src/ios/FADNativeDelegate.h"
#endif  // __OBJC__

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/native_ad_internal.h"

namespace firebase {
namespace gma {
namespace internal {

class NativeAdInternalIOS : public NativeAdInternal {
 public:
  explicit NativeAdInternalIOS(NativeAd* base);
  ~NativeAdInternalIOS();

  Future<void> Initialize(AdParent parent) override;
  Future<AdResult> LoadAd(const char* ad_unit_id,
                          const AdRequest& request) override;
  bool is_initialized() const override { return initialized_; }
  Future<void> RecordImpression(const Variant& impression_data) override;
  Future<void> PerformClick(const Variant& click_data) override;

#ifdef __OBJC__
  NSDictionary* variantmap_to_nsdictionary(const Variant &variant_data);
  void NativeAdDidReceiveAd(GADNativeAd* ad);
  void NativeAdDidFailToReceiveAdWithError(NSError* gad_error);
#endif  // __OBJC__

 private:
  /// Prevents duplicate invocations of initialize on the Native Ad.
  bool initialized_;

  /// Contains information to asynchronously complete the LoadAd Future.
  FutureCallbackData<AdResult>* ad_load_callback_data_;

  /// The GADNativeAd object. Declared as an "id" type to avoid
  /// referencing an Objective-C class in this header.
  id native_ad_;

  /// The publisher-provided view (UIView) that's the parent view of the
  /// native ad. Declared as an "id" type to avoid referencing an
  /// Objective-C class in this header.
  id parent_view_;

  /// The delegate object to listen for callbacks. Declared as an "id"
  /// type to avoid referencing an Objective-C++ class in this header.
  id native_ad_delegate_;

  /// The GADAdloader object used to load a native ad. Declared as an "id" type
  /// to avoid referencing an Objective-C class in this header. A strong
  /// reference to the GADAdLoader must be maintained during the ad loading.
  id ad_loader_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_NATIVE_AD_INTERNAL_IOS_H_
