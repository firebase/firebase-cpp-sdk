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

#include "gma/src/android/ad_view_internal_android.h"

#include <assert.h>
#include <jni.h>

#include <cstdarg>
#include <cstddef>
#include <string>

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "gma/src/android/ad_request_converter.h"
#include "gma/src/android/gma_android.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/ad_view.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

METHOD_LOOKUP_DEFINITION(ad_view_helper,
                         "com/google/firebase/gma/internal/cpp/AdViewHelper",
                         ADVIEWHELPER_METHODS);

METHOD_LOOKUP_DEFINITION(
    ad_view_helper_ad_view_listener,
    "com/google/firebase/gma/internal/cpp/AdViewHelper$AdViewListener",
    ADVIEWHELPER_ADVIEWLISTENER_METHODS);

METHOD_LOOKUP_DEFINITION(ad_view, "com/google/android/gms/ads/AdView",
                         AD_VIEW_METHODS);

namespace internal {

// Contains data to invoke Initialize from the main thread.
struct InitializeOnMainThreadData {
  InitializeOnMainThreadData()
      : ad_parent(nullptr),
        ad_size(0, 0),
        ad_unit_id(),
        ad_view(nullptr),
        ad_view_helper(nullptr),
        callback_data(nullptr) {}
  ~InitializeOnMainThreadData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(ad_parent);
    env->DeleteGlobalRef(ad_view);
    env->DeleteGlobalRef(ad_view_helper);
  }

  AdParent ad_parent;
  AdSize ad_size;
  std::string ad_unit_id;
  jobject ad_view;
  jobject ad_view_helper;
  FutureCallbackData<void>* callback_data;
};

// Contains data to invoke LoadAd from the main thread.
struct LoadAdOnMainThreadData {
  // Thread-safe call data.
  LoadAdOnMainThreadData()
      : ad_request(), callback_data(nullptr), ad_view_helper(nullptr) {}
  ~LoadAdOnMainThreadData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(ad_view_helper);
  }

  AdRequest ad_request;
  FutureCallbackData<AdResult>* callback_data;
  jobject ad_view_helper;
};

// Contains data to facilitate various method calls on the main thraed.
// Thoe corresponding methods have no patamters and return Future<void>
// results.
struct NulleryInvocationOnMainThreadData {
  // Thread-safe call data.
  NulleryInvocationOnMainThreadData()
      : callback_data(nullptr), ad_view_helper(nullptr) {}
  ~NulleryInvocationOnMainThreadData() {
    JNIEnv* env = GetJNI();
    env->DeleteGlobalRef(ad_view_helper);
  }

  FutureCallbackData<void>* callback_data;
  jobject ad_view_helper;
  ad_view_helper::Method method;
};

AdViewInternalAndroid::AdViewInternalAndroid(AdView* base)
    : AdViewInternal(base),
      destroyed_(false),
      helper_(nullptr),
      initialized_(false) {
  firebase::MutexLock lock(mutex_);

  JNIEnv* env = ::firebase::gma::GetJNI();
  FIREBASE_ASSERT(env);

  jobject activity = ::firebase::gma::GetActivity();
  FIREBASE_ASSERT(activity);

  jobject adview_ref =
      env->NewObject(ad_view::GetClass(),
                     ad_view::GetMethodId(ad_view::kConstructor), activity);

  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(adview_ref);

  jobject helper_ref =
      env->NewObject(ad_view_helper::GetClass(),
                     ad_view_helper::GetMethodId(ad_view_helper::kConstructor),
                     reinterpret_cast<jlong>(this), adview_ref);

  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(helper_ref);

  ad_view_ = env->NewGlobalRef(adview_ref);
  env->DeleteLocalRef(adview_ref);

  helper_ = env->NewGlobalRef(helper_ref);
  env->DeleteLocalRef(helper_ref);
}

AdViewInternalAndroid::~AdViewInternalAndroid() {
  firebase::MutexLock lock(mutex_);
  JNIEnv* env = ::firebase::gma::GetJNI();
  if (!destroyed_) {
    // The application should have invoked Destroy already, but
    // invoke it to prevent leaking memory.
    LogWarning(
        "Warning: AdView destructor invoked before the application "
        " called Destroy() on the object.");

    env->CallVoidMethod(helper_,
                        ad_view_helper::GetMethodId(ad_view_helper::kDestroy),
                        /*callbackDataPtr=*/reinterpret_cast<jlong>(ad_view_),
                        /*destructor_invocation=*/true);
  } else {
    env->DeleteGlobalRef(ad_view_);
  }

  ad_view_ = nullptr;

  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
}

// This function is run on the main thread and is called in the
// AdViewInternalAndroid::Initialize() method.
void InitializeAdViewOnMainThread(void* data) {
  InitializeOnMainThreadData* call_data =
      reinterpret_cast<InitializeOnMainThreadData*>(data);
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(call_data);
  FIREBASE_ASSERT(call_data->ad_view);
  FIREBASE_ASSERT(call_data->ad_view_helper);
  FIREBASE_ASSERT(call_data->callback_data);

  jstring ad_unit_id_str =
      static_cast<jstring>(::firebase::gma::GetJNI()->CallObjectMethod(
          call_data->ad_view, ad_view::GetMethodId(ad_view::kGetAdUnitId)));

  if (ad_unit_id_str != nullptr) {
    env->DeleteLocalRef(ad_unit_id_str);

    call_data->callback_data->future_data->future_impl.Complete(
        call_data->callback_data->future_handle, kAdErrorCodeAlreadyInitialized,
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

  env->CallVoidMethod(call_data->ad_view_helper,
                      ad_view_helper::GetMethodId(ad_view_helper::kInitialize),
                      call_data->ad_parent);
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  jobject ad_listener = nullptr;
  ad_listener =
      env->NewObject(ad_view_helper_ad_view_listener::GetClass(),
                     ad_view_helper_ad_view_listener::GetMethodId(
                         ad_view_helper_ad_view_listener::kConstructor),
                     call_data->ad_view_helper);
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
      call_data->callback_data->future_handle, kAdErrorCodeNone, "");

  delete call_data->callback_data;
  call_data->callback_data = nullptr;
  delete call_data;
}

Future<void> AdViewInternalAndroid::Initialize(AdParent parent,
                                               const char* ad_unit_id,
                                               const AdSize& size) {
  firebase::MutexLock lock(mutex_);

  if (initialized_) {
    const SafeFutureHandle<void> future_handle =
        future_data_.future_impl.SafeAlloc<void>(kAdViewFnInitialize);
    Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAdErrorCodeAlreadyInitialized,
                   kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
    return future;
  }

  initialized_ = true;
  ad_size_ = size;

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kAdViewFnInitialize, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  JNIEnv* env = ::firebase::gma::GetJNI();
  FIREBASE_ASSERT(env);

  jobject activity = ::firebase::gma::GetActivity();
  InitializeOnMainThreadData* call_data = new InitializeOnMainThreadData();
  call_data->ad_parent = env->NewGlobalRef(parent);
  call_data->ad_size = size;
  call_data->ad_unit_id = ad_unit_id;
  call_data->ad_view = env->NewGlobalRef(ad_view_);
  call_data->ad_view_helper = env->NewGlobalRef(helper_);
  call_data->callback_data = callback_data;
  util::RunOnMainThread(env, activity, InitializeAdViewOnMainThread, call_data);
  return future;
}

// This function is run on the main thread and is called in the
// AdViewInternalAndroid::LoadAd() method.
void LoadAdOnMainThread(void* data) {
  LoadAdOnMainThreadData* call_data =
      reinterpret_cast<LoadAdOnMainThreadData*>(data);
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  gma::AdErrorCode error = kAdErrorCodeNone;
  jobject j_ad_request =
      GetJavaAdRequestFromCPPAdRequest(call_data->ad_request, &error);

  if (j_ad_request == nullptr) {
    if (error == kAdErrorCodeNone) {
      error = kAdErrorCodeInternalError;
    }
    call_data->callback_data->future_data->future_impl.CompleteWithResult(
        call_data->callback_data->future_handle, error,
        kAdCouldNotParseAdRequestErrorMessage, AdResult());
    delete call_data->callback_data;
    call_data->callback_data = nullptr;
  } else {
    ::firebase::gma::GetJNI()->CallVoidMethod(
        call_data->ad_view_helper,
        ad_view_helper::GetMethodId(ad_view_helper::kLoadAd),
        reinterpret_cast<jlong>(call_data->callback_data), j_ad_request);
    env->DeleteLocalRef(j_ad_request);
  }
  delete call_data;
}

Future<AdResult> AdViewInternalAndroid::LoadAd(const AdRequest& request) {
  firebase::MutexLock lock(mutex_);

  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  FutureCallbackData<AdResult>* callback_data =
      CreateAdResultFutureCallbackData(kAdViewFnLoadAd, &future_data_);
  Future<AdResult> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  LoadAdOnMainThreadData* call_data = new LoadAdOnMainThreadData();
  call_data->ad_request = request;
  call_data->callback_data = callback_data;
  call_data->ad_view_helper = env->NewGlobalRef(helper_);

  jobject activity = ::firebase::gma::GetActivity();
  util::RunOnMainThread(env, activity, LoadAdOnMainThread, call_data);

  return future;
}

BoundingBox AdViewInternalAndroid::bounding_box() const {
  // This JNI integer array consists of the BoundingBox's width, height,
  // x-coordinate, and y-coordinate.
  JNIEnv* env = ::firebase::gma::GetJNI();
  jintArray jni_int_array = (jintArray)env->CallObjectMethod(
      helper_, ad_view_helper::GetMethodId(ad_view_helper::kGetBoundingBox));

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
      helper_, ad_view_helper::GetMethodId(ad_view_helper::kGetPosition));
  box.position = static_cast<AdView::Position>(j_position);
  env->ReleaseIntArrayElements(jni_int_array, bounding_box_elements, 0);
  env->DeleteLocalRef(jni_int_array);

  return box;
}

Future<void> AdViewInternalAndroid::Hide() {
  firebase::MutexLock lock(mutex_);
  return InvokeNullary(kAdViewFnHide, ad_view_helper::kHide);
}

Future<void> AdViewInternalAndroid::Show() {
  firebase::MutexLock lock(mutex_);
  return InvokeNullary(kAdViewFnShow, ad_view_helper::kShow);
}

Future<void> AdViewInternalAndroid::Pause() {
  firebase::MutexLock lock(mutex_);
  return InvokeNullary(kAdViewFnPause, ad_view_helper::kPause);
}

Future<void> AdViewInternalAndroid::Resume() {
  firebase::MutexLock lock(mutex_);
  return InvokeNullary(kAdViewFnResume, ad_view_helper::kResume);
}

Future<void> AdViewInternalAndroid::Destroy() {
  firebase::MutexLock lock(mutex_);
  destroyed_ = true;
  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kAdViewFnDestroy, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);
  ::firebase::gma::GetJNI()->CallVoidMethod(
      helper_, ad_view_helper::GetMethodId(ad_view_helper::kDestroy),
      reinterpret_cast<jlong>(callback_data), /*destructor_invocation=*/false);
  return future;
}

Future<void> AdViewInternalAndroid::SetPosition(int x, int y) {
  firebase::MutexLock lock(mutex_);

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kAdViewFnSetPosition, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  ::firebase::gma::GetJNI()->CallVoidMethod(
      helper_, ad_view_helper::GetMethodId(ad_view_helper::kMoveToXY),
      reinterpret_cast<jlong>(callback_data), x, y);

  return future;
}

Future<void> AdViewInternalAndroid::SetPosition(AdView::Position position) {
  firebase::MutexLock lock(mutex_);

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(kAdViewFnSetPosition, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  ::firebase::gma::GetJNI()->CallVoidMethod(
      helper_, ad_view_helper::GetMethodId(ad_view_helper::kMoveToPosition),
      reinterpret_cast<jlong>(callback_data), static_cast<int>(position));

  return future;
}

// This function is run on the main thread and is called in the
// AdViewInternalAndroid::InvokeNullary() method.
void InvokeNulleryOnMainThread(void* data) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  NulleryInvocationOnMainThreadData* call_data =
      reinterpret_cast<NulleryInvocationOnMainThreadData*>(data);

  ::firebase::gma::GetJNI()->CallVoidMethod(
      call_data->ad_view_helper, ad_view_helper::GetMethodId(call_data->method),
      reinterpret_cast<jlong>(call_data->callback_data));
  call_data->callback_data = nullptr;
  delete call_data;
}

Future<void> AdViewInternalAndroid::InvokeNullary(
    AdViewFn fn, ad_view_helper::Method method) {
  JNIEnv* env = ::firebase::gma::GetJNI();
  jobject activity = ::firebase::gma::GetActivity();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(activity);

  FutureCallbackData<void>* callback_data =
      CreateVoidFutureCallbackData(fn, &future_data_);
  Future<void> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  NulleryInvocationOnMainThreadData* call_data =
      new NulleryInvocationOnMainThreadData();
  call_data->callback_data = callback_data;
  call_data->ad_view_helper = env->NewGlobalRef(helper_);
  call_data->method = method;

  util::RunOnMainThread(env, activity, InvokeNulleryOnMainThread, call_data);

  return future;
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
