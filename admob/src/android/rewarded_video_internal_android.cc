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

#include <assert.h>
#include <jni.h>

#include "admob/admob_resources.h"
#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/android/rewarded_video_internal_android.h"
#include "admob/src/common/rewarded_video_internal.h"
#include "admob/src/include/firebase/admob/rewarded_video.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/assert.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {
namespace rewarded_video {

METHOD_LOOKUP_DEFINITION(
    rewarded_video_helper,
    "com/google/firebase/admob/internal/cpp/RewardedVideoHelper",
    REWARDEDVIDEOHELPER_METHODS);

namespace internal {

RewardedVideoInternalAndroid::RewardedVideoInternalAndroid()
    : RewardedVideoInternal() {
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject activity = ::firebase::admob::GetActivity();

  jobject helper_ref = env->NewObject(
      rewarded_video_helper::GetClass(),
      rewarded_video_helper::GetMethodId(rewarded_video_helper::kConstructor),
      reinterpret_cast<jlong>(this), activity);
  FIREBASE_ASSERT(helper_ref);
  helper_ = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(helper_);
  env->DeleteLocalRef(helper_ref);
}

RewardedVideoInternalAndroid::~RewardedVideoInternalAndroid() {
  // Release the reference to the helper so it gets GCed.
  JNIEnv* env = ::firebase::admob::GetJNI();
  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
}

Future<void> RewardedVideoInternalAndroid::Initialize() {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kRewardedVideoFnInitialize);

  JNIEnv* env = ::firebase::admob::GetJNI();

  env->CallVoidMethod(helper_, rewarded_video_helper::GetMethodId(
                                   rewarded_video_helper::kInitialize),
                      reinterpret_cast<jlong>(callback_data));

  return GetLastResult(kRewardedVideoFnInitialize);
}

Future<void> RewardedVideoInternalAndroid::LoadAd(const char* ad_unit_id,
                                                  const AdRequest& request) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kRewardedVideoFnLoadAd);

  JNIEnv* env = ::firebase::admob::GetJNI();

  jstring ad_unit_id_str = env->NewStringUTF(ad_unit_id);

  AdRequestConverter converter(request);
  jobject request_ref = converter.GetJavaRequestObject();

  env->CallVoidMethod(
      helper_,
      rewarded_video_helper::GetMethodId(rewarded_video_helper::kLoadAd),
      reinterpret_cast<jlong>(callback_data), ad_unit_id_str, request_ref);

  env->DeleteLocalRef(ad_unit_id_str);

  return GetLastResult(kRewardedVideoFnLoadAd);
}

Future<void> RewardedVideoInternalAndroid::Show(AdParent parent) {
  // AdParent is a reference to an Android Activity, however, it is not used by
  // the Android rewarded video Show method implementation.
  return InvokeNullary(kRewardedVideoFnShow, rewarded_video_helper::kShow);
}

Future<void> RewardedVideoInternalAndroid::Pause() {
  return InvokeNullary(kRewardedVideoFnPause, rewarded_video_helper::kPause);
}

Future<void> RewardedVideoInternalAndroid::Resume() {
  return InvokeNullary(kRewardedVideoFnResume, rewarded_video_helper::kResume);
}

Future<void> RewardedVideoInternalAndroid::Destroy() {
  return InvokeNullary(kRewardedVideoFnDestroy,
                       rewarded_video_helper::kDestroy);
}

PresentationState RewardedVideoInternalAndroid::GetPresentationState() {
  jint state = ::firebase::admob::GetJNI()->CallIntMethod(
      helper_, rewarded_video_helper::GetMethodId(
                   rewarded_video_helper::kGetPresentationState));
  assert((static_cast<int>(state) >= 0));
  return static_cast<PresentationState>(state);
}

Future<void> RewardedVideoInternalAndroid::InvokeNullary(
    RewardedVideoFn fn, rewarded_video_helper::Method method) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, fn);

  JNIEnv* env = ::firebase::admob::GetJNI();

  env->CallVoidMethod(helper_, rewarded_video_helper::GetMethodId(method),
                      reinterpret_cast<jlong>(callback_data));

  return GetLastResult(fn);
}

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase
