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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_REWARDED_VIDEO_INTERNAL_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_REWARDED_VIDEO_INTERNAL_H_

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/rewarded_video.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace rewarded_video {
namespace internal {

// Constants representing each RewardedVideo function that returns a Future.
enum RewardedVideoFn {
  kRewardedVideoFnInitialize,
  kRewardedVideoFnLoadAd,
  kRewardedVideoFnShow,
  kRewardedVideoFnPause,
  kRewardedVideoFnResume,
  kRewardedVideoFnDestroy,
  kRewardedVideoFnCount
};

class RewardedVideoInternal {
 public:
  // Create an instance of whichever subclass of RewardedVideoInternal is
  // appropriate for the current platform.
  static RewardedVideoInternal* CreateInstance();

  // The next time an instance would be created via a call to CreateInstance(),
  // return this instance instead. Use this for testing (to stub the
  // platform-specific subclass of RewardedVideoInternal). It will be deleted
  // normally when rewarded_video::Destroy() is called.
  static void SetNextCreatedInstance(RewardedVideoInternal* new_instance);

  // Virtual destructor is required.
  virtual ~RewardedVideoInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize() = 0;

  // Initiates an ad request.
  virtual Future<void> LoadAd(const char* ad_unit_id,
                              const AdRequest& request) = 0;

  // Displays a rewarded video ad.
  virtual Future<void> Show(AdParent parent) = 0;

  // Pauses any background processes associated with rewarded video.
  virtual Future<void> Pause() = 0;

  // Resumes from a pause.
  virtual Future<void> Resume() = 0;

  // Cleans up any resources used by this object in preparation for a delete.
  virtual Future<void> Destroy() = 0;

  // Returns the current presentation state of rewarded video.
  virtual PresentationState GetPresentationState() = 0;

  // Sets the listener that should be informed of presentation state changes
  // and reward events.
  void SetListener(Listener* listener);

  // Notifies the listener (if one exists) that a reward should be granted.
  void NotifyListenerOfReward(RewardItem reward);

  // Notifies the listener (if one exists) that the presentation state has
  // changed.
  void NotifyListenerOfPresentationStateChange(PresentationState state);

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(RewardedVideoFn fn);

 protected:
  // Uses CreateInstance() to create an appropriate one for the current
  // platform.
  RewardedVideoInternal();

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Reference to the listener to which this object sends callbacks.
  Listener* listener_;

  // Lock object for accessing listener_.
  Mutex listener_mutex_;
};

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_REWARDED_VIDEO_INTERNAL_H_
