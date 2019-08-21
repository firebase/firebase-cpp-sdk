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

#include "admob/src/common/rewarded_video_internal.h"

#include "admob/src/include/firebase/admob/rewarded_video.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if FIREBASE_PLATFORM_ANDROID
#include "admob/src/android/rewarded_video_internal_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "admob/src/ios/rewarded_video_internal_ios.h"
#else
#include "admob/src/stub/rewarded_video_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
namespace admob {
namespace rewarded_video {
namespace internal {

RewardedVideoInternal::RewardedVideoInternal()
    : future_data_(kRewardedVideoFnCount), listener_(nullptr) {}

RewardedVideoInternal* RewardedVideoInternal::CreateInstance() {
#if FIREBASE_PLATFORM_ANDROID
  return new RewardedVideoInternalAndroid();
#elif FIREBASE_PLATFORM_IOS
  return new RewardedVideoInternalIOS();
#else
  return new RewardedVideoInternalStub();
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS
}

void RewardedVideoInternal::SetListener(Listener* listener) {
  MutexLock lock(listener_mutex_);
  listener_ = listener;
}

void RewardedVideoInternal::NotifyListenerOfReward(RewardItem reward) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnRewarded(reward);
  }
}

void RewardedVideoInternal::NotifyListenerOfPresentationStateChange(
    PresentationState state) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnPresentationStateChanged(state);
  }
}

Future<void> RewardedVideoInternal::GetLastResult(RewardedVideoFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase
