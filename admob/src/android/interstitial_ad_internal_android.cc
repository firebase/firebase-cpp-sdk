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

#include <cstdarg>
#include <cstddef>

#include "admob/admob_resources.h"
#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/android/interstitial_ad_internal_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/assert.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(
    interstitial_ad_helper,
    "com/google/firebase/admob/internal/cpp/InterstitialAdHelper",
    INTERSTITIALADHELPER_METHODS);

namespace internal {

InterstitialAdInternalAndroid::InterstitialAdInternalAndroid(
    InterstitialAd* base)
    : InterstitialAdInternal(base), helper_(nullptr) {
  JNIEnv* env = ::firebase::admob::GetJNI();

  jobject helper_ref = env->NewObject(
      interstitial_ad_helper::GetClass(),
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kConstructor),
      reinterpret_cast<jlong>(this));

  FIREBASE_ASSERT(helper_ref);

  helper_ = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(helper_);
  env->DeleteLocalRef(helper_ref);
}

InterstitialAdInternalAndroid::~InterstitialAdInternalAndroid() {
  JNIEnv* env = ::firebase::admob::GetJNI();

  // Since it's currently not possible to destroy the intersitial ad, just
  // disconnect from it so the listener doesn't initiate callbacks with stale
  // data.
  env->CallVoidMethod(helper_, interstitial_ad_helper::GetMethodId(
                                   interstitial_ad_helper::kDisconnect));
  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
}

Future<void> InterstitialAdInternalAndroid::Initialize(AdParent parent,
                                                       const char* ad_unit_id) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kInterstitialAdFnInitialize);
  JNIEnv* env = ::firebase::admob::GetJNI();

  jstring ad_unit_str = env->NewStringUTF(ad_unit_id);
  env->CallVoidMethod(
      helper_,
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kInitialize),
      reinterpret_cast<jlong>(callback_data), parent, ad_unit_str);

  env->DeleteLocalRef(ad_unit_str);
  return GetLastResult(kInterstitialAdFnInitialize);
}

Future<void> InterstitialAdInternalAndroid::LoadAd(const AdRequest& request) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kInterstitialAdFnLoadAd);

  AdRequestConverter converter(request);
  jobject request_ref = converter.GetJavaRequestObject();

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_,
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kLoadAd),
      reinterpret_cast<jlong>(callback_data), request_ref);
  return GetLastResult(kInterstitialAdFnLoadAd);
}

Future<void> InterstitialAdInternalAndroid::Show() {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kInterstitialAdFnShow);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_,
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kShow),
      reinterpret_cast<jlong>(callback_data));
  return GetLastResult(kInterstitialAdFnShow);
}

InterstitialAd::PresentationState
InterstitialAdInternalAndroid::GetPresentationState() const {
  jint state = ::firebase::admob::GetJNI()->CallIntMethod(
      helper_, interstitial_ad_helper::GetMethodId(
                   interstitial_ad_helper::kGetPresentationState));
  assert((static_cast<int>(state) >= 0));
  return static_cast<InterstitialAd::PresentationState>(state);
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
