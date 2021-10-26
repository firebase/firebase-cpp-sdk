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

#ifndef FIREBASE_ADMOB_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_
#define FIREBASE_ADMOB_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace internal {

// Constants representing each InterstitialAd function that returns a Future.
enum InterstitialAdFn {
  kInterstitialAdFnInitialize,
  kInterstitialAdFnLoadAd,
  kInterstitialAdFnShow,
  kInterstitialAdFnCount
};

class InterstitialAdInternal {
 public:
  // Create an instance of whichever subclass of InterstitialAdInternal is
  // appropriate for the current platform.
  static InterstitialAdInternal* CreateInstance(InterstitialAd* base);

  // Virtual destructor is required.
  virtual ~InterstitialAdInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Initiates an ad request.
  virtual Future<LoadAdResult> LoadAd(const char* ad_unit_id,
                                      const AdRequest& request) = 0;

  // Displays an interstitial ad.
  virtual Future<void> Show() = 0;

  /// Sets the @ref FullScreenContentListener to receive events about UI
  // and presentation state.
  void SetFullScreenContentListener(FullScreenContentListener* listener);

  /// Sets the @ref PaidEventListener to receive information about paid events.
  void SetPaidEventListener(PaidEventListener* listener);

  // Notifies the FullScreenContentListener (if one exists) that an even has
  // occurred.
  void NotifyListenerOfAdDismissedFullScreenContent();
  void NotifyListenerOfAdFailedToShowFullScreenContent(
      const AdResult& ad_result);
  void NotifyListenerOfAdImpression();
  void NotifyListenerOfAdShowedFullScreenContent();

  // Notifies the Paid Event listener (if one exists) that a paid event has
  // occurred.
  void NotifyListenerOfPaidEvent(const AdValue& ad_value);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(InterstitialAdFn fn);

  // Retrieves the most recent LoadAdResult future for the LoadAd function.
  Future<LoadAdResult> GetLoadAdLastResult();

  // Returns if the InterstitialAd has been initialized.
  virtual bool is_initialized() const = 0;

 protected:
  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit InterstitialAdInternal(InterstitialAd* base);

  // A pointer back to the InterstitialAd class that created us.
  InterstitialAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Reference to the listener to which this object sends callbacks for screen
  // events.
  FullScreenContentListener* full_screen_content_listener_;

  // Reference to the listener to which this object sends callbacks for ad
  // payout events.
  PaidEventListener* paid_event_listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_
