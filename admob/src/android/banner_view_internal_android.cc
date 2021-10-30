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

#include "admob/src/android/banner_view_internal_android.h"

#include <assert.h>
#include <jni.h>

#include <cstdarg>
#include <cstddef>
#include <string>

#include "admob/admob_resources.h"
#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/banner_view.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/assert.h"
#include "app/src/semaphore.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(
    banner_view_helper,
    "com/google/firebase/admob/internal/cpp/BannerViewHelper",
    BANNERVIEWHELPER_METHODS);

METHOD_LOOKUP_DEFINITION(
    banner_view_helper_ad_view_listener,
    "com/google/firebase/admob/internal/cpp/BannerViewHelper$AdViewListener",
    BANNERVIEWHELPER_ADVIEWLISTENER_METHODS);

METHOD_LOOKUP_DEFINITION(ad_view, "com/google/android/gms/ads/AdView",
                         AD_VIEW_METHODS);

namespace internal {

// Contains data to invoke Initailize from the main thread.
struct InitializeOnMainThreadData {
  // Thread-safe call data.
  InitializeOnMainThreadData()
      : ad_parent(nullptr),
        ad_size(0, 0),
        ad_unit_id(),
        ad_view(nullptr),
        banner_view_helper(nullptr),
        callback_data(nullptr) {}
  ~InitializeOnMainThreadData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(ad_parent);
    env->DeleteGlobalRef(ad_view);
    env->DeleteGlobalRef(banner_view_helper);
  }

  AdParent ad_parent;
  AdSize ad_size;
  std::string ad_unit_id;
  jobject ad_view;
  jobject banner_view_helper;
  FutureCallbackData<void>* callback_data;
};

// Contains data to invoke LoadAd from the main thread.
struct LoadAdOnMainThreadData {
  // Thread-safe call data.
  LoadAdOnMainThreadData()
      : ad_request(), callback_data(nullptr), banner_view_helper(nullptr) {}
  ~LoadAdOnMainThreadData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(banner_view_helper);
  }

  AdRequest ad_request;
  FutureCallbackData<LoadAdResult>* callback_data;
  jobject banner_view_helper;
};

// Data to process various method calls which don't have prameters and
// return Future<void> results.
struct NulleryInvocationOnMainThreadData {
  // Thread-safe call data.
  NulleryInvocationOnMainThreadData()
      : callback_data(nullptr), banner_view_helper(nullptr) {}
  ~NulleryInvocationOnMainThreadData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(banner_view_helper);
  }

  FutureCallbackData<void>* callback_data;
  jobject banner_view_helper;
  banner_view_helper::Method method;
};

BannerViewInternalAndroid::BannerViewInternalAndroid(BannerView* base)
    : BannerViewInternal(base), helper_(nullptr), initialized_(false) {
  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);

  jobject activity = ::firebase::admob::GetActivity();
  FIREBASE_ASSERT(activity);

  jobject adview_ref =
      env->NewObject(ad_view::GetClass(),
                     ad_view::GetMethodId(ad_view::kConstructor), activity);

  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(adview_ref);

  jobject helper_ref = env->NewObject(
      banner_view_helper::GetClass(),
      banner_view_helper::GetMethodId(banner_view_helper::kConstructor),
      reinterpret_cast<jlong>(this), adview_ref);

  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(helper_ref);

  ad_view_ = env->NewGlobalRef(adview_ref);
  env->DeleteLocalRef(adview_ref);

  helper_ = env->NewGlobalRef(helper_ref);
  env->DeleteLocalRef(helper_ref);
}

void DestroyOnDeleteCallback(const Future<void>& result, void* sem_data) {
  if (sem_data != nullptr) {
    Semaphore* semaphore = static_cast<Semaphore*>(sem_data);
    semaphore->Post();
  }
}

BannerViewInternalAndroid::~BannerViewInternalAndroid() {
  Semaphore semaphore(0);
  InvokeNullary(kBannerViewFnDestroyOnDelete, banner_view_helper::kDestroy)
      .OnCompletion(DestroyOnDeleteCallback, &semaphore);
  semaphore.Wait();

  JNIEnv* env = ::firebase::admob::GetJNI();

  env->DeleteGlobalRef(ad_view_);
  ad_view_ = nullptr;

  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
}

// This function is run on the main thread and is called in the
// BannerViewInternalAndroid::Initialize() method.
void InitializeBannerViewOnMainThread(void* data) {
  InitializeOnMainThreadData* call_data =
      reinterpret_cast<InitializeOnMainThreadData*>(data);
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(call_data);
  FIREBASE_ASSERT(call_data->ad_view);
  FIREBASE_ASSERT(call_data->banner_view_helper);
  FIREBASE_ASSERT(call_data->callback_data);

  jstring ad_unit_id_str =
      static_cast<jstring>(::firebase::admob::GetJNI()->CallObjectMethod(
          call_data->ad_view, ad_view::GetMethodId(ad_view::kGetAdUnitId)));

  if (ad_unit_id_str != nullptr) {
    env->DeleteLocalRef(ad_unit_id_str);

    call_data->callback_data->future_data->future_impl.Complete(
        call_data->callback_data->future_handle, kAdMobErrorAlreadyInitialized,
        kAdAlreadyInitializedErrorMessage);
    delete call_data->callback_data;
    call_data->callback_data = nullptr;
    delete call_data;
    return;
  }

  ad_unit_id_str = env->NewStringUTF(call_data->ad_unit_id.c_str());
  env->CallVoidMethod(call_data->ad_view,
                      ad_view::GetMethodId(ad_view::kSetAdUnitId),
                      ad_unit_id_str);
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  env->DeleteLocalRef(ad_unit_id_str);

  jobject ad_size =
      CreateJavaAdSize(env, call_data->ad_parent, call_data->ad_size);
  FIREBASE_ASSERT(ad_size);
  env->CallVoidMethod(call_data->ad_view,
                      ad_view::GetMethodId(ad_view::kSetAdSize), ad_size);
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  env->DeleteLocalRef(ad_size);

  env->CallVoidMethod(
      call_data->banner_view_helper,
      banner_view_helper::GetMethodId(banner_view_helper::kInitialize),
      call_data->ad_parent);
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  jobject ad_listener = nullptr;
  ad_listener =
      env->NewObject(banner_view_helper_ad_view_listener::GetClass(),
                     banner_view_helper_ad_view_listener::GetMethodId(
                         banner_view_helper_ad_view_listener::kConstructor),
                     call_data->banner_view_helper);
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  env->CallVoidMethod(call_data->ad_view,
                      ad_view::GetMethodId(ad_view::kSetAdListener),
                      ad_listener);
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  env->CallVoidMethod(call_data->ad_view,
                      ad_view::GetMethodId(ad_view::kSetOnPaidEventListener),
                      ad_listener);
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  env->DeleteLocalRef(ad_listener);

  call_data->callback_data->future_data->future_impl.Complete(
      call_data->callback_data->future_handle, kAdMobErrorNone, "");

  delete call_data->callback_data;
  call_data->callback_data = nullptr;
  delete call_data;
}

Future<void> BannerViewInternalAndroid::Initialize(AdParent parent,
                                                   const char* ad_unit_id,
                                                   const AdSize& size) {
  firebase::MutexLock lock(mutex_);

  if (initialized_) {
    const SafeFutureHandle<void> future_handle =
        future_data_.future_impl.SafeAlloc<void>(kBannerViewFnInitialize);
    CompleteFuture(kAdMobErrorAlreadyInitialized,
                   kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
    return MakeFuture(&future_data_.future_impl, future_handle);
  }

  initialized_ = true;

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kBannerViewFnSetPosition, &future_data_);

  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);

  jobject activity = ::firebase::admob::GetActivity();
  InitializeOnMainThreadData* call_data = new InitializeOnMainThreadData();
  call_data->ad_parent = env->NewGlobalRef(parent);
  call_data->ad_size = size;
  call_data->ad_unit_id = ad_unit_id;
  call_data->ad_view = env->NewGlobalRef(ad_view_);
  call_data->banner_view_helper = env->NewGlobalRef(helper_);
  call_data->callback_data = callback_data;
  util::RunOnMainThread(env, activity, InitializeBannerViewOnMainThread,
                        call_data);

  return MakeFuture(&future_data_.future_impl, callback_data->future_handle);
}

// This function is run on the main thread and is called in the
// BannerViewInternalAndroid::LoadAd() method.
void LoadAdOnMainThread(void* data) {
  LoadAdOnMainThreadData* call_data =
      reinterpret_cast<LoadAdOnMainThreadData*>(data);
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  admob::AdMobError error = kAdMobErrorNone;
  jobject j_ad_request =
      GetJavaAdRequestFromCPPAdRequest(call_data->ad_request, &error);

  if (j_ad_request == nullptr) {
    if (error == kAdMobErrorNone) {
      error = kAdMobErrorInternalError;
    }
    call_data->callback_data->future_data->future_impl.CompleteWithResult(
        call_data->callback_data->future_handle, error,
        kAdCouldNotParseAdRequestErrorMessage, LoadAdResult());
    delete call_data->callback_data;
    call_data->callback_data = nullptr;
  } else {
    ::firebase::admob::GetJNI()->CallVoidMethod(
        call_data->banner_view_helper,
        banner_view_helper::GetMethodId(banner_view_helper::kLoadAd),
        reinterpret_cast<jlong>(call_data->callback_data), j_ad_request);
    env->DeleteLocalRef(j_ad_request);
  }
  delete call_data;
}

Future<LoadAdResult> BannerViewInternalAndroid::LoadAd(
    const AdRequest& request) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  FutureCallbackData<LoadAdResult>* callback_data =
      CreateLoadAdResultFutureCallbackData(kBannerViewFnLoadAd, &future_data_);
  SafeFutureHandle<LoadAdResult> future_handle = callback_data->future_handle;

  LoadAdOnMainThreadData* call_data = new LoadAdOnMainThreadData();
  call_data->ad_request = request;
  call_data->callback_data = callback_data;
  call_data->banner_view_helper = env->NewGlobalRef(helper_);

  jobject activity = ::firebase::admob::GetActivity();
  util::RunOnMainThread(env, activity, LoadAdOnMainThread, call_data);

  return MakeFuture(&future_data_.future_impl, future_handle);
}

BoundingBox BannerViewInternalAndroid::bounding_box() const {
  // This JNI integer array consists of the BoundingBox's width, height,
  // x-coordinate, and y-coordinate.
  JNIEnv* env = ::firebase::admob::GetJNI();
  jintArray jni_int_array = (jintArray)env->CallObjectMethod(
      helper_,
      banner_view_helper::GetMethodId(banner_view_helper::kGetBoundingBox));

  // The bounding box array must consist of 4 integers: width, height,
  // x-coordinate, and y-coordinate.
  int count = static_cast<int>(env->GetArrayLength(jni_int_array));
  assert(count == 4);
  (void)count;

  jint* bounding_box_elements =
      env->GetIntArrayElements(jni_int_array, nullptr);
  BoundingBox box;
  box.width = static_cast<int>(bounding_box_elements[0]);
  box.height = static_cast<int>(bounding_box_elements[1]);
  box.x = static_cast<int>(bounding_box_elements[2]);
  box.y = static_cast<int>(bounding_box_elements[3]);
  jint j_position = env->CallIntMethod(
      helper_,
      banner_view_helper::GetMethodId(banner_view_helper::kGetPosition));
  box.position = static_cast<AdView::Position>(j_position);
  env->ReleaseIntArrayElements(jni_int_array, bounding_box_elements, 0);
  env->DeleteLocalRef(jni_int_array);

  return box;
}

Future<void> BannerViewInternalAndroid::Hide() {
  return InvokeNullary(kBannerViewFnHide, banner_view_helper::kHide);
}

Future<void> BannerViewInternalAndroid::Show() {
  return InvokeNullary(kBannerViewFnShow, banner_view_helper::kShow);
}

Future<void> BannerViewInternalAndroid::Pause() {
  return InvokeNullary(kBannerViewFnPause, banner_view_helper::kPause);
}

Future<void> BannerViewInternalAndroid::Resume() {
  return InvokeNullary(kBannerViewFnResume, banner_view_helper::kResume);
}

Future<void> BannerViewInternalAndroid::Destroy() {
  return InvokeNullary(kBannerViewFnDestroy, banner_view_helper::kDestroy);
}

Future<void> BannerViewInternalAndroid::SetPosition(int x, int y) {
  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kBannerViewFnSetPosition, &future_data_);
  SafeFutureHandle<void> future_handle = callback_data->future_handle;

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, banner_view_helper::GetMethodId(banner_view_helper::kMoveToXY),
      reinterpret_cast<jlong>(callback_data), x, y);

  return MakeFuture(&future_data_.future_impl, future_handle);
}

Future<void> BannerViewInternalAndroid::SetPosition(
    BannerView::Position position) {
  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kBannerViewFnSetPosition, &future_data_);
  SafeFutureHandle<void> future_handle = callback_data->future_handle;

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_,
      banner_view_helper::GetMethodId(banner_view_helper::kMoveToPosition),
      reinterpret_cast<jlong>(callback_data), static_cast<int>(position));

  return MakeFuture(&future_data_.future_impl, future_handle);
}

// This function is run on the main thread and is called in the
// BannerViewInternalAndroid::InvokeNullary() method.
void InvokeNulleryOnMainThread(void* data) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  NulleryInvocationOnMainThreadData* call_data =
      reinterpret_cast<NulleryInvocationOnMainThreadData*>(data);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      call_data->banner_view_helper,
      banner_view_helper::GetMethodId(call_data->method),
      reinterpret_cast<jlong>(call_data->callback_data));
  call_data->callback_data = nullptr;
  delete call_data;
}

Future<void> BannerViewInternalAndroid::InvokeNullary(
    BannerViewFn fn, banner_view_helper::Method method) {
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject activity = ::firebase::admob::GetActivity();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(activity);

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(fn, &future_data_);
  SafeFutureHandle<void> future_handle = callback_data->future_handle;

  NulleryInvocationOnMainThreadData* call_data =
      new NulleryInvocationOnMainThreadData();
  call_data->callback_data = callback_data;
  call_data->banner_view_helper = env->NewGlobalRef(helper_);
  call_data->method = method;

  util::RunOnMainThread(env, activity, InvokeNulleryOnMainThread, call_data);

  return MakeFuture(&future_data_.future_impl, future_handle);
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
