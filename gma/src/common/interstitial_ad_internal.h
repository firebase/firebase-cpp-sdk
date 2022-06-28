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

#ifndef FIREBASE_GMA_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/full_screen_ad_event_listener.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/interstitial_ad.h"

namespace firebase {
namespace gma {
namespace internal {

// Constants representing each InterstitialAd function that returns a Future.
enum InterstitialAdFn {
  kInterstitialAdFnInitialize,
  kInterstitialAdFnLoadAd,
  kInterstitialAdFnShow,
  kInterstitialAdFnCount
};

class InterstitialAdInternal : public FullScreenAdEventListener {
 public:
  // Create an instance of whichever subclass of InterstitialAdInternal is
  // appropriate for the current platform.
  static InterstitialAdInternal* CreateInstance(InterstitialAd* base);

  // Virtual destructor is required.
  virtual ~InterstitialAdInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Initiates an ad request.
  virtual Future<AdResult> LoadAd(const char* ad_unit_id,
                                  const AdRequest& request) = 0;

  // Displays an interstitial ad.
  virtual Future<void> Show() = 0;

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(InterstitialAdFn fn);

  // Retrieves the most recent AdResult future for the LoadAd function.
  Future<AdResult> GetLoadAdLastResult();

  // Returns true if the InterstitialAd has been initialized.
  virtual bool is_initialized() const = 0;

 protected:
  friend class firebase::gma::InterstitialAd;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit InterstitialAdInternal(InterstitialAd* base);

  // A pointer back to the InterstitialAd class that created us.
  InterstitialAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_
