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

#ifndef FIREBASE_ADMOB_SRC_ANDROID_BANNER_VIEW_INTERNAL_ANDROID_H_
#define FIREBASE_ADMOB_SRC_ANDROID_BANNER_VIEW_INTERNAL_ANDROID_H_

#include "admob/src/common/banner_view_internal.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

// Used to set up the cache of BannerViewHelper class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define BANNERVIEWHELPER_METHODS(X)                                            \
  X(Constructor, "<init>", "(JLcom/google/android/gms/ads/AdView;)V"),         \
  X(Initialize, "initialize", "(Landroid/app/Activity;)V"),                    \
  X(LoadAd, "loadAd", "(JLcom/google/android/gms/ads/AdRequest;)V"),           \
  X(Hide, "hide", "(J)V"),                                                     \
  X(Show, "show", "(J)V"),                                                     \
  X(Pause, "pause", "(J)V"),                                                   \
  X(Resume, "resume", "(J)V"),                                                 \
  X(Destroy, "destroy", "(JZ)V"),                                              \
  X(MoveToPosition, "moveTo", "(JI)V"),                                        \
  X(MoveToXY, "moveTo", "(JII)V"),                                             \
  X(GetBoundingBox, "getBoundingBox", "()[I"),                                 \
  X(GetPosition, "getPosition", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(banner_view_helper, BANNERVIEWHELPER_METHODS);

#define BANNERVIEWHELPER_ADVIEWLISTENER_METHODS(X) \
  X(Constructor, "<init>",                         \
    "(Lcom/google/firebase/admob/internal/cpp/BannerViewHelper;)V")

METHOD_LOOKUP_DECLARATION(banner_view_helper_ad_view_listener,
                          BANNERVIEWHELPER_ADVIEWLISTENER_METHODS);

// clang-format off
#define AD_VIEW_METHODS(X)                                             \
  X(Constructor, "<init>", "(Landroid/content/Context;)V"),            \
  X(GetAdUnitId, "getAdUnitId", "()Ljava/lang/String;"),               \
  X(SetAdSize, "setAdSize", "(Lcom/google/android/gms/ads/AdSize;)V"), \
  X(SetAdUnitId, "setAdUnitId", "(Ljava/lang/String;)V"),              \
  X(SetAdListener, "setAdListener",                                    \
    "(Lcom/google/android/gms/ads/AdListener;)V"),                     \
  X(SetOnPaidEventListener, "setOnPaidEventListener",                  \
    "(Lcom/google/android/gms/ads/OnPaidEventListener;)V")
// clang-format on

METHOD_LOOKUP_DECLARATION(ad_view, AD_VIEW_METHODS);

namespace internal {

class BannerViewInternalAndroid : public BannerViewInternal {
 public:
  BannerViewInternalAndroid(BannerView* base);
  ~BannerViewInternalAndroid() override;

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

 private:
  // Reference to the Java helper object used to interact with the Mobile Ads
  // SDK.
  jobject helper_;

  // Reference to the Android AdView object used to display BannerView ads.
  jobject ad_view_;

  // Tracks if this BannerView has been initialized.
  bool initialized_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;

  // Convenience method to "dry" the JNI calls that don't take parameters beyond
  // the future callback pointer.
  Future<void> InvokeNullary(BannerViewFn fn,
                             banner_view_helper::Method method);
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_ANDROID_BANNER_VIEW_INTERNAL_ANDROID_H_
