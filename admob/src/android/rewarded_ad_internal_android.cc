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

#include "admob/src/android/rewarded_ad_internal_android.h"

#include <assert.h>
#include <jni.h>

#include <cstdarg>
#include <cstddef>

#include "admob/admob_resources.h"
#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/assert.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(
    rewarded_ad_helper,
    "com/google/firebase/admob/internal/cpp/RewardedAdHelper",
    REWARDEDADHELPER_METHODS);

namespace internal {

RewardedAdInternalAndroid::RewardedAdInternalAndroid(RewardedAd* base)
    : RewardedAdInternal(base), helper_(nullptr), initialized_(false) {
  firebase::MutexLock lock(mutex_);
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject helper_ref = env->NewObject(
      rewarded_ad_helper::GetClass(),
      rewarded_ad_helper::GetMethodId(rewarded_ad_helper::kConstructor),
      reinterpret_cast<jlong>(this));

  FIREBASE_ASSERT(helper_ref);
  helper_ = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(helper_);
  env->DeleteLocalRef(helper_ref);
}

RewardedAdInternalAndroid::~RewardedAdInternalAndroid() {
  firebase::MutexLock lock(mutex_);
  JNIEnv* env = ::firebase::admob::GetJNI();
  // Since it's currently not possible to destroy the rewarded ad, just
  // disconnect from it so the listener doesn't initiate callbacks with stale
  // data.
  env->CallVoidMethod(helper_, rewarded_ad_helper::GetMethodId(
                                   rewarded_ad_helper::kDisconnect));
  env->DeleteGlobalRef(helper_);

  helper_ = nullptr;
  full_screen_content_listener_ = nullptr;
  paid_event_listener_ = nullptr;
  user_earned_reward_listener_ = nullptr;
}

Future<void> RewardedAdInternalAndroid::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);

  if (initialized_) {
    const SafeFutureHandle<void> future_handle =
        future_data_.future_impl.SafeAlloc<void>(kRewardedAdFnInitialize);
    CompleteFuture(kAdMobErrorAlreadyInitialized,
                   kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
    return MakeFuture(&future_data_.future_impl, future_handle);
  }

  initialized_ = true;
  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kRewardedAdFnInitialize, &future_data_);
  env->CallVoidMethod(
      helper_, rewarded_ad_helper::GetMethodId(rewarded_ad_helper::kInitialize),
      reinterpret_cast<jlong>(callback_data), parent);
  return MakeFuture(&future_data_.future_impl, callback_data->future_handle);
}

Future<AdResult> RewardedAdInternalAndroid::LoadAd(const char* ad_unit_id,
                                                   const AdRequest& request) {
  firebase::MutexLock lock(mutex_);

  if (!initialized_) {
    SafeFutureHandle<AdResult> handle =
        future_data_.future_impl.SafeAlloc<AdResult>(kRewardedAdFnLoadAd,
                                                     AdResult());
    CompleteFuture(kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
                   handle, &future_data_, AdResult());
    return MakeFuture(&future_data_.future_impl, handle);
  }

  admob::AdMobError error = kAdMobErrorNone;
  jobject j_request = GetJavaAdRequestFromCPPAdRequest(request, &error);
  if (j_request == nullptr) {
    if (error == kAdMobErrorNone) {
      error = kAdMobErrorInternalError;
    }
    return CreateAndCompleteFutureWithResult(
        kRewardedAdFnLoadAd, error, kAdCouldNotParseAdRequestErrorMessage,
        &future_data_, AdResult());
  }
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FutureCallbackData<AdResult>* callback_data =
      CreateAdResultFutureCallbackData(kRewardedAdFnLoadAd, &future_data_);

  jstring j_ad_unit_str = env->NewStringUTF(ad_unit_id);
  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, rewarded_ad_helper::GetMethodId(rewarded_ad_helper::kLoadAd),
      reinterpret_cast<jlong>(callback_data), j_ad_unit_str, j_request);
  env->DeleteLocalRef(j_ad_unit_str);
  env->DeleteLocalRef(j_request);

  return MakeFuture(&future_data_.future_impl, callback_data->future_handle);
}

Future<void> RewardedAdInternalAndroid::Show(
    UserEarnedRewardListener* listener) {
  firebase::MutexLock lock(mutex_);
  if (!initialized_) {
    const SafeFutureHandle<void> future_handle =
        future_data_.future_impl.SafeAlloc<void>(kRewardedAdFnShow);
    CompleteFuture(kAdMobErrorUninitialized, kAdUninitializedErrorMessage,
                   future_handle, &future_data_);
    return MakeFuture(&future_data_.future_impl, future_handle);
  }

  user_earned_reward_listener_ = listener;

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kRewardedAdFnShow, &future_data_);
  SafeFutureHandle<void> future_handle = callback_data->future_handle;

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, rewarded_ad_helper::GetMethodId(rewarded_ad_helper::kShow),
      reinterpret_cast<jlong>(callback_data));

  return MakeFuture(&future_data_.future_impl, future_handle);
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
