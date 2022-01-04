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
// TODO(b/173819915): fix data races with cleanup.
template <typename ResultT>
class Promise {
 public:
  // Creates a future backed by `LastResults` cache.
  Promise(CleanupNotifier* cleanup,
          ReferenceCountedFutureImpl* future_api,
          int identifier)
      : cleanup_{NOT_NULL(cleanup)},
        future_api_{NOT_NULL(future_api)},
        identifier_{identifier},
        handle_{future_api->SafeAlloc<ResultT>(identifier)} {
    RegisterForCleanup();
  }

  ~Promise() {
    std::lock_guard<std::mutex> lock(destruction_mutex_);
    UnregisterForCleanup();
  }

  Promise(const Promise& rhs)
      : cleanup_{rhs.cleanup_},
        future_api_{rhs.future_api_},
        identifier_{rhs.identifier_},
        handle_{rhs.handle_} {
    RegisterForCleanup();
  }

  Promise(Promise&& rhs) noexcept
      : cleanup_{rhs.cleanup_},
        future_api_{rhs.future_api_},
        identifier_{rhs.identifier_},
        handle_{rhs.handle_} {
    rhs.UnregisterForCleanup();
    rhs.Reset();

    RegisterForCleanup();
  }

  Promise& operator=(const Promise& rhs) {
    UnregisterForCleanup();

    cleanup_ = rhs.cleanup_;
    future_api_ = rhs.future_api_;
    identifier_ = rhs.identifier_;
    handle_ = rhs.handle_;

    RegisterForCleanup();

    return *this;
  }

  Promise& operator=(Promise&& rhs) noexcept {
    rhs.UnregisterForCleanup();
    UnregisterForCleanup();

    cleanup_ = rhs.cleanup_;
    future_api_ = rhs.future_api_;
    identifier_ = rhs.identifier_;
    handle_ = rhs.handle_;

    RegisterForCleanup();

    rhs.Reset();

    return *this;
  }

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
    if (IsCleanedUp()) {
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
    if (IsCleanedUp()) {
      return;
    }
    future_api_->Complete(handle_, NoError());
  }

  void SetError(const util::Status& status) {
    SIMPLE_HARD_ASSERT(
        !status.ok(),
        "To fulfill a promise with 'ok' status, use Promise::SetValue.");
    if (IsCleanedUp()) {
      return;
    }

    future_api_->Complete(handle_, status.code(),
                          status.error_message().c_str());
  }

  Future<ResultT> future() {
    if (IsCleanedUp()) {
      return Future<ResultT>{};
    }

    return Future<ResultT>{future_api_, handle_.get()};
  }

 private:
  Promise() = default;

  int NoError() const { return static_cast<int>(Error::kErrorOk); }

  void Reset() {
    cleanup_ = nullptr;
    future_api_ = nullptr;
    identifier_ = 0;
    handle_ = {};
  }

  // Note: `CleanupFn` is not used because `Promise` is a header-only class, to
  // avoid a circular dependency between headers.
  void RegisterForCleanup() {
    if (IsCleanedUp()) {
      return;
    }

    cleanup_->RegisterObject(this, [](void* raw_this) {
      auto* this_ptr = static_cast<Promise*>(raw_this);
      std::unique_lock<std::mutex> lock(this_ptr->destruction_mutex_,
                                        std::try_to_lock_t());
      // If the destruction mutex is locked, it means the destructor is
      // currently running. In that case, leave the cleanup to destructor;
      // otherwise, trying to acquire the mutex will result in a deadlock
      // (because cleanup is currently holding the cleanup mutex which the
      // destructor will try to acquire to unregister itself from cleanup).
      if (!lock) {
        return;
      }

      this_ptr->Reset();
    });
  }

  void UnregisterForCleanup() {
    if (IsCleanedUp()) {
      return;
    }
    cleanup_->UnregisterObject(this);
  }

  bool IsCleanedUp() const { return cleanup_ == nullptr; }

  CleanupNotifier* cleanup_ = nullptr;
  ReferenceCountedFutureImpl* future_api_ = nullptr;
  int identifier_ = 0;
  SafeFutureHandle<ResultT> handle_;
  std::mutex destruction_mutex_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_MAIN_H_
