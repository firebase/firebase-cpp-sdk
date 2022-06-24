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

#ifndef FIREBASE_GMA_SRC_COMMON_REWARDED_AD_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_REWARDED_AD_INTERNAL_H_

#include <string>

#include "app/src/include/firebase/future.h"
#include "gma/src/common/full_screen_ad_event_listener.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/rewarded_ad.h"

namespace firebase {
namespace gma {
namespace internal {

// Constants representing each RewardedAd function that returns a Future.
enum RewardedAdFn {
  kRewardedAdFnInitialize,
  kRewardedAdFnLoadAd,
  kRewardedAdFnShow,
  kRewardedAdFnCount
};

class RewardedAdInternal : public FullScreenAdEventListener {
 public:
  // Create an instance of whichever subclass of RewardedAdInternal is
  // appropriate for the current platform.
  static RewardedAdInternal* CreateInstance(RewardedAd* base);

  // Virtual destructor is required.
  virtual ~RewardedAdInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Initiates an ad request.
  virtual Future<AdResult> LoadAd(const char* ad_unit_id,
                                  const AdRequest& request) = 0;

  // Displays an interstitial ad.
  virtual Future<void> Show(UserEarnedRewardListener* listener) = 0;

  /// Notifies the UserEarnedRewardListener (if one exists) than a reward
  /// event has occurred.
  void NotifyListenerOfUserEarnedReward(const std::string& type,
                                        int64_t amount);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(RewardedAdFn fn);

  // Retrieves the most recent AdResult future for the LoadAd function.
  Future<AdResult> GetLoadAdLastResult();

  // Returns true if the RewardedAd has been initialized.
  virtual bool is_initialized() const = 0;

  /// Sets the server side verification options.
  void SetServerSideVerificationOptions(
      const RewardedAd::ServerSideVerificationOptions&
          serverSideVerificationOptions);

 protected:
  friend class firebase::gma::RewardedAd;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit RewardedAdInternal(RewardedAd* base);

  // A pointer back to the RewardedAd class that created us.
  RewardedAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Reference to the listener to which this object sends user earned
  // reward event callbacks.
  UserEarnedRewardListener* user_earned_reward_listener_;

  // Options for RewardedAd server-side verification callbacks.
  RewardedAd::ServerSideVerificationOptions serverSideVerificationOptions_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_REWARDED_AD_INTERNAL_H_
