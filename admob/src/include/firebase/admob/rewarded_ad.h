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

#ifndef FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_REWARDED_AD_H_
#define FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_REWARDED_AD_H_

#include "firebase/admob/types.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace admob {

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class RewardedAdInternal;
}  // namespace internal

/// @brief Loads and displays AdMob rewarded ads.
///
/// @ref RewardedAd is a single-use object that can load and show a
/// single AdMob rewarded ad.
///
/// RewardedAd objects provide information about their current state
/// through Futures. @ref Initialize, @ref LoadAd, and @ref Show each have a
/// corresponding @ref Future from which you can determine result of the
/// previous call.
///
/// Here's how one might initialize, load, and show an rewarded ad while
/// checking against the result of the previous action at each step:
///
/// @code
/// namespace admob = ::firebase::admob;
/// admob::RewardedAd* rewarded = new admob::RewardedAd();
/// rewarded->Initialize(ad_parent);
/// @endcode
///
/// Then, later:
///
/// @code
/// if (rewarded->InitializeLastResult().status() ==
///     ::firebase::kFutureStatusComplete &&
///     rewarded->InitializeLastResult().error() ==
///     firebase::admob::kAdMobErrorNone) {
///   rewarded->LoadAd( "YOUR_AD_UNIT_ID", my_ad_request);
/// }
/// @endcode
///
/// And after that:
///
/// @code
/// if (rewarded->LoadAdLastResult().status() ==
///     ::firebase::kFutureStatusComplete &&
///     rewarded->LoadAdLastResult().error() ==
///     firebase::admob::kAdMobErrorNone)) {
///   rewarded->Show();
/// }
/// @endcode
class RewardedAd {
 public:
  /// Creates an uninitialized @ref RewardedAd object.
  /// @ref Initialize must be called before the object is used.
  RewardedAd();

  ~RewardedAd();

  /// Initialize the @ref RewardedAd object.
  /// @param[in] parent The platform-specific UI element that will host the ad.
  Future<void> Initialize(AdParent parent);

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Initialize.
  Future<void> InitializeLastResult() const;

  /// Begins an asynchronous request for an ad.
  ///
  /// @param[in] ad_unit_id The ad unit ID to use in loading the ad.
  /// @param[in] request An AdRequest struct with information about the request
  ///                    to be made (such as targeting info).
  Future<LoadAdResult> LoadAd(const char* ad_unit_id, const AdRequest& request);

  /// Returns a @ref Future containing the status of the last call to
  /// @ref LoadAd.
  Future<LoadAdResult> LoadAdLastResult() const;

  /// Shows the @ref RewardedAd. This should not be called unless an ad has
  /// already been loaded.
  ///
  /// param[in] listener The @ref UserEarnedRewardListener to be notified when
  /// user earns a reward.
  Future<void> Show(UserEarnedRewardListener* listener);

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Show.
  Future<void> ShowLastResult() const;

  /// Sets the @ref FullScreenContentListener for this @ref RewardedAd.
  ///
  /// @param[in] listener A valid @ref FullScreenContentListener to receive
  ///                     callbacks.
  void SetFullScreenContentListener(FullScreenContentListener* listener);

  /// Registers a callback to be invoked when this ad is estimated to have
  /// earned money
  ///
  /// @param[in] listener A valid @ref PaidEventListener to receive callbacks.
  void SetPaidEventListener(PaidEventListener* listener);

 private:
  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  internal::RewardedAdInternal* internal_;
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_INTERSTITIAL_AD_H_
