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

#include "admob/src/include/firebase/admob/rewarded_video.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/common/rewarded_video_internal.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {
namespace rewarded_video {

// An internal, platform-specific implementation object that this class uses to
// interact with the Google Mobile Ads SDKs for iOS and Android.
static internal::RewardedVideoInternal* g_internal = nullptr;

const char kUninitializedError[] =
    "rewarded_video::Initialize() must be called before this method.";
const char kCannotInitTwiceError[] =
    "rewarded_video::Initialize cannot be called twice.";

PollableRewardListener::PollableRewardListener() : mutex_(new Mutex()) {}

PollableRewardListener::~PollableRewardListener() { delete mutex_; }

bool PollableRewardListener::PollReward(RewardItem* reward) {
  FIREBASE_ASSERT(reward != nullptr);
  MutexLock lock(*mutex_);
  if (rewards_.empty()) {
    return false;
  }
  reward->amount = rewards_.front().amount;
  reward->reward_type = rewards_.front().reward_type;
  rewards_.pop();
  return true;
}

void PollableRewardListener::OnRewarded(RewardItem reward) {
  MutexLock lock(*mutex_);
  RewardItem new_item;
  new_item.amount = reward.amount;
  new_item.reward_type = reward.reward_type;
  rewards_.push(new_item);
}

void PollableRewardListener::OnPresentationStateChanged(
    PresentationState state) {
  // Nothing done here. The publisher already has the ability to poll
  // rewarded_video::presentation_state for presentation state info.
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize has been invoked and allowed to complete.
static bool CheckIsInitialized() {
  bool initialized =
      g_internal != nullptr &&
      g_internal->GetLastResult(internal::kRewardedVideoFnInitialize)
              .status() == kFutureStatusComplete;
  FIREBASE_ASSERT_MESSAGE_RETURN(false, initialized, kUninitializedError);
  return true;
}

Future<void> Initialize() {
  FIREBASE_ASSERT_RETURN(Future<void>(), admob::IsInitialized());
  // Initialize cannot be called more than once.
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), g_internal == nullptr,
                                 kCannotInitTwiceError);
  g_internal = internal::RewardedVideoInternal::CreateInstance();
  GetOrCreateCleanupNotifier()->RegisterObject(g_internal, [](void*) {
    // Since there is no way to shut down this module after initialization
    LogWarning(
        "rewardedvideo::Destroy should be called before "
        "admob::Terminate.");
    Destroy();
  });
  return g_internal->Initialize();
}

Future<void> InitializeLastResult() {
  // This result can't be checked unless the RewardedVideoInternal has been
  // created, but it must be available to publishers *before* Initialize has
  // completed (so they can know when it's done). That's why this result uses a
  // different conditional than the others.
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), g_internal != nullptr,
                                 kUninitializedError);
  return g_internal->GetLastResult(internal::kRewardedVideoFnInitialize);
}

Future<void> LoadAd(const char* ad_unit_id, const AdRequest& request) {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->LoadAd(ad_unit_id, request);
}

Future<void> LoadAdLastResult() {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->GetLastResult(internal::kRewardedVideoFnLoadAd);
}

Future<void> Show(AdParent parent) {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->Show(parent);
}

Future<void> ShowLastResult() {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->GetLastResult(internal::kRewardedVideoFnShow);
}

Future<void> Pause() {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->Pause();
}

Future<void> PauseLastResult() {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->GetLastResult(internal::kRewardedVideoFnPause);
}

Future<void> Resume() {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->Resume();
}

Future<void> ResumeLastResult() {
  if (!CheckIsInitialized()) return Future<void>();
  return g_internal->GetLastResult(internal::kRewardedVideoFnResume);
}

// This method is synchronous. It does not return a future, but instead waits
// until the internal Destroy has completed, so that it's safe to delete
// g_internal.
void Destroy() {
  if (!CheckIsInitialized()) return;

  Mutex mutex(Mutex::kModeNonRecursive);
  mutex.Acquire();

  GetOrCreateCleanupNotifier()->UnregisterObject(g_internal);

  g_internal->Destroy().OnCompletion(
      [](const Future<void>&, void* mutex) {
        (reinterpret_cast<Mutex*>(mutex))->Release();
      },
      &mutex);

  mutex.Acquire();
  mutex.Release();
  delete g_internal;
  g_internal = nullptr;
}

PresentationState presentation_state() {
  if (!CheckIsInitialized()) return kPresentationStateHidden;
  return g_internal->GetPresentationState();
}

void SetListener(Listener* listener) {
  if (!CheckIsInitialized()) return;
  g_internal->SetListener(listener);
}

}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase
