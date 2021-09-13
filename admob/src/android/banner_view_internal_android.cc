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
#include "app/src/mutex.h"
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

METHOD_LOOKUP_DEFINITION(ad_size, "com/google/android/gms/ads/AdSize",
                         AD_SIZE_METHODS);

namespace internal {

BannerViewInternalAndroid::BannerViewInternalAndroid(BannerView* base)
    : BannerViewInternal(base),
      helper_(nullptr),
      initialized_(false),
      bounding_box_() {
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
  DestroyInternalData();

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

struct BannerViewInternalInitializeData {
  // Thread-safe call data.
  BannerViewInternalInitializeData()
      : activity_global(nullptr),
        ad_view(nullptr),
        banner_view_helper(nullptr) {}
  ~BannerViewInternalInitializeData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(activity_global);
    env->DeleteGlobalRef(ad_view);
    env->DeleteGlobalRef(banner_view_helper);
  }

  jobject activity_global;
  AdSize ad_size;
  std::string ad_unit_id;
  jobject ad_view;
  jobject banner_view_helper;
  FutureCallbackData* callback_data;
};

// This function is run on the main thread and is called in the
// BannerViewInternalAndroid::Initialize() method.
void InitializeBannerViewOnMainThread(void* data) {
  BannerViewInternalInitializeData* call_data =
      reinterpret_cast<BannerViewInternalInitializeData*>(data);
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env != nullptr);

  jstring ad_unit_id_str =
      static_cast<jstring>(::firebase::admob::GetJNI()->CallObjectMethod(
          call_data->ad_view, ad_view::GetMethodId(ad_view::kGetAdUnitId)));
  if (ad_unit_id_str != nullptr) {
    CompleteFuture(kAdMobErrorAlreadyInitialized, "",
                   call_data->callback_data->future_handle,
                   call_data->callback_data->future_data);
    return;
  }

  ad_unit_id_str = env->NewStringUTF(call_data->ad_unit_id.c_str());
  env->CallVoidMethod(call_data->ad_view,
                      ad_view::GetMethodId(ad_view::kSetAdUnitId),
                      ad_unit_id_str);
  env->DeleteLocalRef(ad_unit_id_str);

  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  jobject ad_size = env->NewObject(
      ad_size::GetClass(), ad_size::GetMethodId(ad_size::kConstructor),
      call_data->ad_size.width, call_data->ad_size.height);

  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  env->CallVoidMethod(call_data->ad_view,
                      ad_view::GetMethodId(ad_view::kSetAdSize), ad_size);

  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  env->CallVoidMethod(
      call_data->banner_view_helper,
      banner_view_helper::GetMethodId(banner_view_helper::kInitialize),
      call_data->activity_global);

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

  env->DeleteLocalRef(ad_listener);

  CompleteFuture(kAdMobErrorNone, "", call_data->callback_data->future_handle,
                 call_data->callback_data->future_data);

  delete call_data;
}

Future<void> BannerViewInternalAndroid::Initialize(AdParent parent,
                                                   const char* ad_unit_id,
                                                   AdSize size) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kBannerViewFnInitialize);

  if (initialized_) {
    future_data_.future_impl.Complete(callback_data->future_handle,
                                      kAdMobErrorAlreadyInitialized,
                                      "Ad is already initialized.");
    return Future<void>(&future_data_.future_impl,
                        callback_data->future_handle);
  }

  initialized_ = true;

  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject activity = ::firebase::admob::GetActivity();

  BannerViewInternalInitializeData* call_data =
      new BannerViewInternalInitializeData();
  call_data->activity_global = env->NewGlobalRef(activity);
  call_data->ad_size = size;
  call_data->ad_unit_id = ad_unit_id;
  call_data->ad_view = env->NewGlobalRef(ad_view_);
  call_data->banner_view_helper = env->NewGlobalRef(helper_);
  call_data->callback_data = callback_data;
  util::RunOnMainThread(env, activity, InitializeBannerViewOnMainThread,
                        call_data);

  return GetLastResult(kBannerViewFnInitialize);
}

Future<void> BannerViewInternalAndroid::LoadAd(const AdRequest& request) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kBannerViewFnLoadAd);

  AdRequestConverter converter(request);
  jobject request_ref = converter.GetJavaRequestObject();

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, banner_view_helper::GetMethodId(banner_view_helper::kLoadAd),
      reinterpret_cast<jlong>(callback_data), request_ref);
  return GetLastResult(kBannerViewFnLoadAd);
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
  DestroyInternalData();
  return InvokeNullary(kBannerViewFnDestroy, banner_view_helper::kDestroy);
}

Future<void> BannerViewInternalAndroid::MoveTo(int x, int y) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kBannerViewFnMoveTo);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, banner_view_helper::GetMethodId(banner_view_helper::kMoveToXY),
      reinterpret_cast<jlong>(callback_data), x, y);

  return GetLastResult(kBannerViewFnMoveTo);
}

Future<void> BannerViewInternalAndroid::MoveTo(BannerView::Position position) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kBannerViewFnMoveTo);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_,
      banner_view_helper::GetMethodId(banner_view_helper::kMoveToPosition),
      reinterpret_cast<jlong>(callback_data), static_cast<int>(position));

  return GetLastResult(kBannerViewFnMoveTo);
}

BannerView::PresentationState BannerViewInternalAndroid::GetPresentationState()
    const {
  jint state = ::firebase::admob::GetJNI()->CallIntMethod(
      helper_, banner_view_helper::GetMethodId(
                   banner_view_helper::kGetPresentationState));
  assert((static_cast<int>(state) >= 0));
  return static_cast<BannerView::PresentationState>(state);
}

BoundingBox BannerViewInternalAndroid::GetBoundingBox() const {
  // If the banner view is hidden and the publisher polls the bounding box, we
  // return the current bounding box.
  if (GetPresentationState() == BannerView::kPresentationStateHidden) {
    return bounding_box_;
  }

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
  bounding_box_ = box;

  env->ReleaseIntArrayElements(jni_int_array, bounding_box_elements, 0);
  env->DeleteLocalRef(jni_int_array);

  return box;
}

Future<void> BannerViewInternalAndroid::InvokeNullary(
    BannerViewFn fn, banner_view_helper::Method method) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, fn);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, banner_view_helper::GetMethodId(method),
      reinterpret_cast<jlong>(callback_data));

  return GetLastResult(fn);
}

void BannerViewInternalAndroid::DestroyInternalData() {
  // The bounding box is zeroed on destroy.
  bounding_box_ = {};
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
