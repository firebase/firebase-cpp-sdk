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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_

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
  virtual Future<void> Initialize(AdParent parent, const char* ad_unit_id) = 0;

  // Initiates an ad request.
  virtual Future<void> LoadAd(const AdRequest& request) = 0;

  // Displays an interstitial ad.
  virtual Future<void> Show() = 0;

  // Returns the current presentation state of the interstitial ad.
  virtual InterstitialAd::PresentationState GetPresentationState() const = 0;

  // Sets the listener that should be informed of presentation state changes.
  void SetListener(InterstitialAd::Listener* listener);

  // Notifies the listener (if one exists) that the presentation state has
  // changed.
  void NotifyListenerOfPresentationStateChange(
      InterstitialAd::PresentationState state);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(InterstitialAdFn fn);

 protected:
  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit InterstitialAdInternal(InterstitialAd* base);

  // A pointer back to the InterstitialAd class that created us.
  InterstitialAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Reference to the listener to which this object sends callbacks.
  InterstitialAd::Listener* listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_INTERSTITIAL_AD_INTERNAL_H_
