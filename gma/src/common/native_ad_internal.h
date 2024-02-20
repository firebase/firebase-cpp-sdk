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

#ifndef FIREBASE_GMA_SRC_COMMON_NATIVE_AD_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_NATIVE_AD_INTERNAL_H_

#include <string>
#include <vector>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"

namespace firebase {
namespace gma {
namespace internal {

// Constants representing each NativeAd function that returns a Future.
enum NativeAdFn {
  kNativeAdFnInitialize,
  kNativeAdFnLoadAd,
  kNativeAdFnRecordImpression,
  kNativeAdFnPerformClick,
  kNativeAdFnCount
};

class NativeAdInternal {
 public:
  // Create an instance of whichever subclass of NativeAdInternal is
  // appropriate for the current platform.
  static NativeAdInternal* CreateInstance(NativeAd* base);

  // Virtual destructor is required.
  virtual ~NativeAdInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Initiates an ad request.
  virtual Future<AdResult> LoadAd(const char* ad_unit_id,
                                  const AdRequest& request) = 0;

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(NativeAdFn fn);

  // Retrieves the most recent AdResult future for the LoadAd function.
  Future<AdResult> GetLoadAdLastResult();

  // Sets an AdListener for this ad view.
  virtual void SetAdListener(AdListener* listener);

  // Notifies the Ad listener (if one exists) that an event has occurred.
  void NotifyListenerAdClicked();
  void NotifyListenerAdClosed();
  void NotifyListenerAdImpression();
  void NotifyListenerAdOpened();

  // Returns true if the NativeAd has been initialized.
  virtual bool is_initialized() const = 0;

  // Returns the associated icon asset of the native ad.
  const NativeAdImage& icon() const { return icon_; }

  // Returns the associated image assets of the native ad.
  const std::vector<NativeAdImage>& images() const { return images_; }

  // Returns the associated icon asset of the native ad.
  const NativeAdImage& adchoices_icon() const { return adchoices_icon_; }

  // Only used by allowlisted ad units.
  virtual Future<void> RecordImpression(const Variant& impression_data) = 0;

  // Only used by allowlisted ad units.
  virtual Future<void> PerformClick(const Variant& click_data) = 0;

 protected:
  friend class firebase::gma::NativeAd;
  friend class firebase::gma::NativeAdImage;
  friend class firebase::gma::GmaInternal;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit NativeAdInternal(NativeAd* base);

  // Invoked after a native ad has been loaded to fill native ad image assets.
  void insert_image(const NativeAdImage& ad_image,
                    const std::string& image_type);

  // Invoked before filling native ad image assets.
  void clear_existing_images();

  // A pointer back to the NativeAd class that created us.
  NativeAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Listener for NativeAd Lifecycle event callbacks.
  AdListener* ad_listener_;

  // Tracks the native ad icon asset.
  NativeAdImage icon_;

  // Tracks the native ad image assets.
  std::vector<NativeAdImage> images_;

  // Tracks the native ad choices icon asset.
  NativeAdImage adchoices_icon_;

  // Lock object for accessing ad_listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_NATIVE_AD_INTERNAL_H_
