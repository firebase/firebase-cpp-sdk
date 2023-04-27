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

#include "gma/src/android/interstitial_ad_internal_android.h"

#include <assert.h>
#include <jni.h>

#include <cstdarg>
#include <cstddef>

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "gma/src/android/ad_request_converter.h"
#include "gma/src/android/gma_android.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/interstitial_ad.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

METHOD_LOOKUP_DEFINITION(
    interstitial_ad_helper,
    "com/google/firebase/gma/internal/cpp/InterstitialAdHelper",
    INTERSTITIALADHELPER_METHODS);

namespace internal {

InterstitialAdInternalAndroid::InterstitialAdInternalAndroid(
    InterstitialAd* base)
    : InterstitialAdInternal(base), helper_(nullptr), initialized_(false) {
  firebase::MutexLock lock(mutex_);
  JNIEnv* env = ::firebase::gma::GetJNI();
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
  firebase::MutexLock lock(mutex_);
  JNIEnv* env = ::firebase::gma::GetJNI();
  // Since it's currently not possible to destroy the intersitial ad, just
  // disconnect from it so the listener doesn't initiate callbacks with stale
  // data.
  env->CallVoidMethod(helper_, interstitial_ad_helper::GetMethodId(
                                   interstitial_ad_helper::kDisconnect));
  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
}

Future<void> InterstitialAdInternalAndroid::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);

  if (initialized_) {
    const SafeFutureHandle<void> future_handle =
        future_data_.future_impl.SafeAlloc<void>(kInterstitialAdFnInitialize);
    Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAdErrorCodeAlreadyInitialized,
                   kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
    return future;
  }

  initialized_ = true;
  JNIEnv* env = ::firebase::gma::GetJNI();
  FIREBASE_ASSERT(env);

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kInterstitialAdFnInitialize, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);
  env->CallVoidMethod(
      helper_,
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kInitialize),
      reinterpret_cast<jlong>(callback_data), parent);
  return future;
}

Future<AdResult> InterstitialAdInternalAndroid::LoadAd(
    const char* ad_unit_id, const AdRequest& request) {
  firebase::MutexLock lock(mutex_);

  if (!initialized_) {
    SafeFutureHandle<AdResult> future_handle =
        CreateFuture<AdResult>(kInterstitialAdFnLoadAd, &future_data_);
    Future<AdResult> future =
        MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
                   future_handle, &future_data_, AdResult());
    return future;
  }

  gma::AdErrorCode error = kAdErrorCodeNone;
  jobject j_request = GetJavaAdRequestFromCPPAdRequest(request, &error);
  if (j_request == nullptr) {
    if (error == kAdErrorCodeNone) {
      error = kAdErrorCodeInternalError;
    }
    return CreateAndCompleteFutureWithResult(
        kInterstitialAdFnLoadAd, error, kAdCouldNotParseAdRequestErrorMessage,
        &future_data_, AdResult());
  }
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FutureCallbackData<AdResult>* callback_data =
      CreateAdResultFutureCallbackData(kInterstitialAdFnLoadAd, &future_data_);
  Future<AdResult> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  jstring j_ad_unit_str = env->NewStringUTF(ad_unit_id);
  ::firebase::gma::GetJNI()->CallVoidMethod(
      helper_,
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kLoadAd),
      reinterpret_cast<jlong>(callback_data), j_ad_unit_str, j_request);
  env->DeleteLocalRef(j_ad_unit_str);
  env->DeleteLocalRef(j_request);
  return future;
}

Future<void> InterstitialAdInternalAndroid::Show() {
  firebase::MutexLock lock(mutex_);
  if (!initialized_) {
    const SafeFutureHandle<void> future_handle =
        future_data_.future_impl.SafeAlloc<void>(kInterstitialAdFnShow);
    Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
                   future_handle, &future_data_);
    return future;
  }

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kInterstitialAdFnShow, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  ::firebase::gma::GetJNI()->CallVoidMethod(
      helper_,
      interstitial_ad_helper::GetMethodId(interstitial_ad_helper::kShow),
      reinterpret_cast<jlong>(callback_data));

  return future;
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
