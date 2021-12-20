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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_MAIN_H_

#include <mutex>  // NOLINT(build/c++11)
#include <utility>

#include "Firestore/core/src/util/status.h"
#include "Firestore/core/src/util/statusor.h"
#include "absl/meta/type_traits.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/hard_assert_common.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

// Simplifies working with a Firebase future.
//
// `Promise` preallocates a result in its constructor and keeps track of the
// handle. `Promise` doesn't own any memory and can be freely copied. The given
// `ReferenceCountedFutureImpl` is presumed to stay valid for the whole lifetime
// of this `Promise`.
//
// `Promise` guarantees that it refers to a valid future backed by `LastResults`
// array.
template <typename ResultT>
class Promise {
 public:
  Promise(CleanupNotifier* cleanup,
          ReferenceCountedFutureImpl* future_api,
          int identifier)
      : identifier_(identifier),
        cleanup_(NOT_NULL(cleanup)),
        future_api_(NOT_NULL(future_api)),
        handle_(future_api->SafeAlloc<ResultT>(identifier)) {
    cleanup_->RegisterObject(this, CleanUp);
  }

  ~Promise() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cleanup_) {
      cleanup_->UnregisterObject(this);
    }
  }

  Promise(const Promise& rhs) : identifier_(rhs.identifier_) {
    std::lock_guard<std::mutex> lock(rhs.mutex_);
    if (rhs.cleanup_) {
      cleanup_ = rhs.cleanup_;
      future_api_ = rhs.future_api_;
      handle_ = rhs.handle_;

      cleanup_->RegisterObject(this, CleanUp);
    }
  }

  Promise(Promise&& rhs) noexcept : identifier_(std::move(rhs.identifier_)) {
    std::lock_guard<std::mutex> lock(rhs.mutex_);
    if (rhs.cleanup_) {
      cleanup_ = std::move(rhs.cleanup_);
      future_api_ = std::move(rhs.future_api_);
      handle_ = std::move(rhs.handle_);

      rhs.cleanup_->UnregisterObject(&rhs);
      rhs.cleanup_ = nullptr;
      rhs.future_api_ = nullptr;
      rhs.handle_ = {};

      cleanup_->RegisterObject(this, CleanUp);
    }
  }

  Promise& operator=(const Promise&) = delete;
  Promise& operator=(Promise&& rhs) = delete;

  // This is only a template function to enable SFINAE. The `Promise` will have
  // either `SetValue(ResultT)` or `SetValue()` defined, based on whether
  // `ResultT` is `void`.
  //
  // Note that `DummyT` is necessary because SFINAE only applies if template
  // arguments are being substituted during _instantiation_. If `ResultT` were
  // used directly, the unnamed template argument would have been known at the
  // point of declaration, and SFINAE wouldn't apply, resulting in an error.
  template <typename DummyT = ResultT,
            typename = absl::enable_if_t<!std::is_void<DummyT>::value>>
  void SetValue(DummyT result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!future_api_) {
      return;
    }

    future_api_->Complete(handle_, NoError(), /*error_message=*/"",
                          [&](ResultT* value) {
                            // Future API doesn't support moving the value, use
                            // callback to achieve this.
                            *value = std::move(result);
                          });
  }

  template <typename DummyT = ResultT,
            typename = absl::enable_if_t<std::is_void<DummyT>::value>>
  void SetValue() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!future_api_) {
      return;
    }

    future_api_->Complete(handle_, NoError());
  }

  void SetError(const util::Status& status) {
    SIMPLE_HARD_ASSERT(
        !status.ok(),
        "To fulfill a promise with 'ok' status, use Promise::SetValue.");

    std::lock_guard<std::mutex> lock(mutex_);
    if (!future_api_) {
      return;
    }

    future_api_->Complete(handle_, status.code(),
                          status.error_message().c_str());
  }

  Future<ResultT> future() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!future_api_) {
      return Future<ResultT>();
    }

    return Future<ResultT>(future_api_, handle_.get());
  }

 private:
  Promise() = default;

  int NoError() const { return static_cast<int>(Error::kErrorOk); }

  static void CleanUp(void* ptr) {
    auto* instance = static_cast<Promise*>(ptr);
    std::lock_guard<std::mutex> lock(instance->mutex_);
    instance->cleanup_ = nullptr;
    instance->future_api_ = nullptr;
    instance->handle_ = {};
  }

  mutable std::mutex mutex_;
  int identifier_ = 0;
  CleanupNotifier* cleanup_ = nullptr;
  ReferenceCountedFutureImpl* future_api_ = nullptr;
  SafeFutureHandle<ResultT> handle_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_MAIN_H_
