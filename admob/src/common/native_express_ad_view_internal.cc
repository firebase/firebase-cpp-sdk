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

#include "admob/src/common/native_express_ad_view_internal.h"
#include "admob/src/include/firebase/admob/native_express_ad_view.h"
#include "app/src/include/firebase/future.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif  // __APPLE__

#if defined(__ANDROID__)
#include "admob/src/android/native_express_ad_view_internal_android.h"
#elif TARGET_OS_IPHONE
#include "admob/src/ios/native_express_ad_view_internal_ios.h"
#else
#include "admob/src/stub/native_express_ad_view_internal_stub.h"
#endif  // __ANDROID__, TARGET_OS_IPHONE

namespace firebase {
namespace admob {
namespace internal {

NativeExpressAdViewInternal::NativeExpressAdViewInternal(
    NativeExpressAdView* base)
    : base_(base),
      future_data_(kNativeExpressAdViewFnCount),
      listener_(nullptr) {}

NativeExpressAdViewInternal* NativeExpressAdViewInternal::CreateInstance(
    NativeExpressAdView* base) {
#if defined(__ANDROID__)
  return new NativeExpressAdViewInternalAndroid(base);
#elif TARGET_OS_IPHONE
  return new NativeExpressAdViewInternalIOS(base);
#else
  return new NativeExpressAdViewInternalStub(base);
#endif  // __ANDROID__, TARGET_OS_IPHONE
}

void NativeExpressAdViewInternal::SetListener(
    NativeExpressAdView::Listener* listener) {
  MutexLock lock(listener_mutex_);
  listener_ = listener;
}

void NativeExpressAdViewInternal::NotifyListenerOfPresentationStateChange(
    NativeExpressAdView::PresentationState state) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnPresentationStateChanged(base_, state);
  }
}

void NativeExpressAdViewInternal::NotifyListenerOfBoundingBoxChange(
    BoundingBox box) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnBoundingBoxChanged(base_, box);
  }
}

Future<void> NativeExpressAdViewInternal::GetLastResult(
    NativeExpressAdViewFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
