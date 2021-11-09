/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_ADMOB_SRC_COMMON_FULL_SCREEN_AD_EVENT_LISTENER_H_
#define FIREBASE_ADMOB_SRC_COMMON_FULL_SCREEN_AD_EVENT_LISTENER_H_

#include "admob/src/common/admob_common.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace internal {

// Listener class used by both InterstitialAds and RewardedAds.
class FullScreenAdEventListener {
 public:
  FullScreenAdEventListener()
      : full_screen_content_listener_(nullptr), paid_event_listener_(nullptr) {}

  // Sets the @ref FullScreenContentListener to receive events about UI
  // and presentation state.
  void SetFullScreenContentListener(FullScreenContentListener* listener);

  // Sets the @ref PaidEventListener to receive information about paid events.
  void SetPaidEventListener(PaidEventListener* listener);

  // Notifies the FullScreenContentListener (if one exists) that an event has
  // occurred.
  virtual void NotifyListenerOfAdClickedFullScreenContent();
  virtual void NotifyListenerOfAdDismissedFullScreenContent();
  virtual void NotifyListenerOfAdFailedToShowFullScreenContent(
      const AdResult& ad_result);
  virtual void NotifyListenerOfAdImpression();
  virtual void NotifyListenerOfAdShowedFullScreenContent();

  // Notify the PaidEventListener (if one exists) that a paid event has
  // occurred.
  virtual void NotifyListenerOfPaidEvent(const AdValue& ad_value);

 protected:
  // Reference to the listener to which this object sends full screen event
  // callbacks.
  FullScreenContentListener* full_screen_content_listener_;

  // Reference to the listener to which this object sends ad payout
  // event callbacks.
  PaidEventListener* paid_event_listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_FULL_SCREEN_AD_EVENT_LISTENER_H_
