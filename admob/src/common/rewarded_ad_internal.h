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

#ifndef FIREBASE_ADMOB_SRC_COMMON_REWARDED_AD_INTERNAL_H_
#define FIREBASE_ADMOB_SRC_COMMON_REWARDED_AD_INTERNAL_H_

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/rewarded_ad.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace internal {

// Constants representing each RewardedAd function that returns a Future.
enum RewardedAdFn {
  kRewardedAdFnInitialize,
  kRewardedAdFnLoadAd,
  kRewardedAdFnShow,
  kRewardedAdFnCount
};

class RewardedAdInternal {
 public:
  // Create an instance of whichever subclass of RewardedAdInternal is
  // appropriate for the current platform.
  static RewardedAdInternal* CreateInstance(RewardedAd* base);

  // Virtual destructor is required.
  virtual ~RewardedAdInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Initiates an ad request.
  virtual Future<LoadAdResult> LoadAd(const char* ad_unit_id,
                                      const AdRequest& request) = 0;

  // Displays an interstitial ad.
  virtual Future<void> Show(UserEarnedRewardListener* listener) = 0;

  /// Sets the @ref FullScreenContentListener to receive events about UI
  // and presentation state.
  void SetFullScreenContentListener(FullScreenContentListener* listener);

  /// Sets the @ref PaidEventListener to receive information about paid events.
  void SetPaidEventListener(PaidEventListener* listener);

  /// Notifies the UserEarnedRewardListener (if one exists) than a reward
  /// event has occurred.
  void NotifyListenerOfUserEarnedReward(const std::string& type,
                                        int64_t amount);

  // Notifies the FullScreenContentListener (if one exists) that an event has
  // occurred.
  void NotifyListenerOfAdClickedFullScreenContent();
  void NotifyListenerOfAdDismissedFullScreenContent();
  void NotifyListenerOfAdFailedToShowFullScreenContent(
      const AdResult& ad_result);
  void NotifyListenerOfAdImpression();
  void NotifyListenerOfAdShowedFullScreenContent();

  // Notifies the PaidEventListener (if one exists) that a paid event has
  // occurred.
  void NotifyListenerOfPaidEvent(const AdValue& ad_value);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(RewardedAdFn fn);

  // Retrieves the most recent LoadAdResult future for the LoadAd function.
  Future<LoadAdResult> GetLoadAdLastResult();

  // Returns true if the RewardedAd has been initialized.
  virtual bool is_initialized() const = 0;

 protected:
  friend class firebase::admob::RewardedAd;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit RewardedAdInternal(RewardedAd* base);

  // A pointer back to the RewardedAd class that created us.
  RewardedAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Reference to the listener to which this object sends full screen event
  // callbacks.
  FullScreenContentListener* full_screen_content_listener_;

  // Reference to the listener to which this object sends ad payout
  // event callbacks.
  PaidEventListener* paid_event_listener_;

  // Reference to the listener to which this object sends user earned
  // reward event callbacks.
  UserEarnedRewardListener* user_earned_reward_listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_REWARDED_AD_INTERNAL_H_
