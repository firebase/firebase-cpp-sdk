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
#include "admob/src/android/native_express_ad_view_internal_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/native_express_ad_view.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/assert.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(
    native_express_ad_view_helper,
    "com/google/firebase/admob/internal/cpp/NativeExpressAdViewHelper",
    NATIVEEXPRESSADVIEWHELPER_METHODS);

namespace internal {

NativeExpressAdViewInternalAndroid::NativeExpressAdViewInternalAndroid(
    NativeExpressAdView* base)
    : NativeExpressAdViewInternal(base), helper_(nullptr), bounding_box_() {
  JNIEnv* env = ::firebase::admob::GetJNI();

  jobject helper_ref =
      env->NewObject(native_express_ad_view_helper::GetClass(),
                     native_express_ad_view_helper::GetMethodId(
                         native_express_ad_view_helper::kConstructor),
                     reinterpret_cast<jlong>(this));
  FIREBASE_ASSERT(helper_ref);
  helper_ = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(helper_);
  env->DeleteLocalRef(helper_ref);
}

NativeExpressAdViewInternalAndroid::~NativeExpressAdViewInternalAndroid() {
  JNIEnv* env = ::firebase::admob::GetJNI();

  // Destroy the native express ad view so all pending futures / callbacks
  // complete.
  {
    Mutex mutex(Mutex::kModeNonRecursive);
    mutex.Acquire();
    Destroy().OnCompletion(
        [](const Future<void>&, void* mutex) {
          reinterpret_cast<Mutex*>(mutex)->Release();
        },
        &mutex);
    // Acquire a second Mutex lock to block until the Future for the last call
    // to Destroy() completes at which point the lambda function in OnCompletion
    // is called and the Mutex lock is released.
    mutex.Acquire();
    mutex.Release();
  }
  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
}

Future<void> NativeExpressAdViewInternalAndroid::Initialize(
    AdParent parent, const char* ad_unit_id, AdSize size) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kNativeExpressAdViewFnInitialize);

  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject activity = ::firebase::admob::GetActivity();

  jstring ad_unit_id_str = env->NewStringUTF(ad_unit_id);

  env->CallVoidMethod(helper_, native_express_ad_view_helper::GetMethodId(
                                   native_express_ad_view_helper::kInitialize),
                      reinterpret_cast<jlong>(callback_data), activity,
                      ad_unit_id_str, static_cast<jint>(size.ad_size_type),
                      static_cast<jint>(size.width),
                      static_cast<jint>(size.height));

  env->DeleteLocalRef(ad_unit_id_str);

  return GetLastResult(kNativeExpressAdViewFnInitialize);
}

Future<void> NativeExpressAdViewInternalAndroid::LoadAd(
    const AdRequest& request) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kNativeExpressAdViewFnLoadAd);

  AdRequestConverter converter(request);
  jobject request_ref = converter.GetJavaRequestObject();

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, native_express_ad_view_helper::GetMethodId(
                   native_express_ad_view_helper::kLoadAd),
      reinterpret_cast<jlong>(callback_data), request_ref);

  return GetLastResult(kNativeExpressAdViewFnLoadAd);
}

Future<void> NativeExpressAdViewInternalAndroid::Hide() {
  return InvokeNullary(kNativeExpressAdViewFnHide,
                       native_express_ad_view_helper::kHide);
}

Future<void> NativeExpressAdViewInternalAndroid::Show() {
  return InvokeNullary(kNativeExpressAdViewFnShow,
                       native_express_ad_view_helper::kShow);
}

Future<void> NativeExpressAdViewInternalAndroid::Pause() {
  return InvokeNullary(kNativeExpressAdViewFnPause,
                       native_express_ad_view_helper::kPause);
}

Future<void> NativeExpressAdViewInternalAndroid::Resume() {
  return InvokeNullary(kNativeExpressAdViewFnResume,
                       native_express_ad_view_helper::kResume);
}

Future<void> NativeExpressAdViewInternalAndroid::Destroy() {
  // The bounding box is zeroed on destroy.
  bounding_box_ = {};
  return InvokeNullary(kNativeExpressAdViewFnDestroy,
                       native_express_ad_view_helper::kDestroy);
}

Future<void> NativeExpressAdViewInternalAndroid::MoveTo(int x, int y) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kNativeExpressAdViewFnMoveTo);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, native_express_ad_view_helper::GetMethodId(
                   native_express_ad_view_helper::kMoveToXY),
      reinterpret_cast<jlong>(callback_data), x, y);

  return GetLastResult(kNativeExpressAdViewFnMoveTo);
}

Future<void> NativeExpressAdViewInternalAndroid::MoveTo(
    NativeExpressAdView::Position position) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, kNativeExpressAdViewFnMoveTo);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, native_express_ad_view_helper::GetMethodId(
                   native_express_ad_view_helper::kMoveToPosition),
      reinterpret_cast<jlong>(callback_data), static_cast<int>(position));

  return GetLastResult(kNativeExpressAdViewFnMoveTo);
}

NativeExpressAdView::PresentationState
NativeExpressAdViewInternalAndroid::GetPresentationState() const {
  jint state = ::firebase::admob::GetJNI()->CallIntMethod(
      helper_, native_express_ad_view_helper::GetMethodId(
                   native_express_ad_view_helper::kGetPresentationState));
  assert((static_cast<int>(state) >= 0));
  return static_cast<NativeExpressAdView::PresentationState>(state);
}

BoundingBox NativeExpressAdViewInternalAndroid::GetBoundingBox() const {
  // If the native express ad view is hidden and the publisher polls the
  // bounding box, we return the current bounding box.
  if (GetPresentationState() == NativeExpressAdView::kPresentationStateHidden) {
    return bounding_box_;
  }

  // This JNI integer array consists of the BoundingBox's width, height,
  // x-coordinate, and y-coordinate.
  JNIEnv* env = ::firebase::admob::GetJNI();
  jintArray jni_int_array = (jintArray)env->CallObjectMethod(
      helper_, native_express_ad_view_helper::GetMethodId(
                   native_express_ad_view_helper::kGetBoundingBox));

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

Future<void> NativeExpressAdViewInternalAndroid::InvokeNullary(
    NativeExpressAdViewFn fn, native_express_ad_view_helper::Method method) {
  FutureCallbackData* callback_data =
      CreateFutureCallbackData(&future_data_, fn);

  ::firebase::admob::GetJNI()->CallVoidMethod(
      helper_, native_express_ad_view_helper::GetMethodId(method),
      reinterpret_cast<jlong>(callback_data));

  return GetLastResult(fn);
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
