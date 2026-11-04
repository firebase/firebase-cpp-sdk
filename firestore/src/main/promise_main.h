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

#include <memory>
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
      : state_{std::make_shared<State>(cleanup, future_api, identifier)} {}

  ~Promise() = default;

  Promise(const Promise& rhs) {
    std::lock_guard<std::mutex> lock(rhs.mutex_);
    state_ = rhs.state_;
  }

  Promise(Promise&& rhs) noexcept {
    std::lock_guard<std::mutex> lock(rhs.mutex_);
    state_ = std::move(rhs.state_);
  }

  Promise& operator=(const Promise& rhs) {
    if (this == &rhs) return *this;
    std::shared_ptr<State> old_state;
    {
      std::scoped_lock lock(mutex_, rhs.mutex_);
      old_state = std::move(state_);
      state_ = rhs.state_;
    }
    return *this;
  }

  Promise& operator=(Promise&& rhs) noexcept {
    if (this == &rhs) return *this;
    std::shared_ptr<State> old_state;
    {
      std::scoped_lock lock(mutex_, rhs.mutex_);
      old_state = std::move(state_);
      state_ = std::move(rhs.state_);
    }
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
    std::shared_ptr<State> state = GetState();
    if (state != nullptr) {
      state->SetValue(std::move(result));
    }
  }

  template <typename DummyT = ResultT,
            typename = absl::enable_if_t<std::is_void<DummyT>::value>>
  void SetValue() {
    std::shared_ptr<State> state = GetState();
    if (state != nullptr) {
      state->SetValue();
    }
  }

  void SetError(const util::Status& status) {
    SIMPLE_HARD_ASSERT(
        !status.ok(),
        "To fulfill a promise with 'ok' status, use Promise::SetValue.");
    std::shared_ptr<State> state = GetState();
    if (state != nullptr) {
      state->SetError(status);
    }
  }

  Future<ResultT> future() {
    std::shared_ptr<State> state = GetState();
    if (state == nullptr) {
      return Future<ResultT>{};
    }
    return state->future();
  }

  bool IsCleanedUp() const {
    std::shared_ptr<State> state = GetState();
    return state == nullptr || state->IsCleanedUp();
  }

 private:
  class State {
   public:
    State(CleanupNotifier* cleanup,
          ReferenceCountedFutureImpl* future_api,
          int identifier)
        : cleanup_{NOT_NULL(cleanup)},
          future_api_{NOT_NULL(future_api)},
          identifier_{identifier},
          handle_{future_api->SafeAlloc<ResultT>(identifier)} {
      // Note: `CleanupFn` is not used because `Promise` is a header-only class,
      // to avoid a circular dependency between headers.
      cleanup->RegisterObject(this, [](void* raw_this) {
        auto* this_ptr = static_cast<State*>(raw_this);
        std::unique_lock<std::mutex> destruction_lock(
            this_ptr->destruction_mutex_, std::try_to_lock);
        // If the destruction mutex is locked, it means the destructor is
        // currently running. In that case, leave the cleanup to destructor;
        // otherwise, trying to acquire the mutex will result in a deadlock
        // (because cleanup is currently holding the cleanup mutex which the
        // destructor will try to acquire to unregister itself from cleanup).
        if (!destruction_lock) {
          return;
        }

        std::lock_guard<std::mutex> lock(this_ptr->mutex_);
        this_ptr->Reset();
      });
    }

    ~State() {
      std::lock_guard<std::mutex> destruction_lock(destruction_mutex_);
      CleanupNotifier* cleanup = nullptr;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        cleanup = cleanup_;
        Reset();
      }
      if (cleanup != nullptr) {
        cleanup->UnregisterObject(this);
      }
    }

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    template <typename DummyT = ResultT,
              typename = absl::enable_if_t<!std::is_void<DummyT>::value>>
    void SetValue(DummyT result) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (cleanup_ == nullptr) return;
      future_api_->Complete(handle_, NoError(), /*error_message=*/"",
                            [&](ResultT* value) {
                              // Future API doesn't support moving the value,
                              // use callback to achieve this.
                              *value = std::move(result);
                            });
    }

    template <typename DummyT = ResultT,
              typename = absl::enable_if_t<std::is_void<DummyT>::value>>
    void SetValue() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (cleanup_ == nullptr) return;
      future_api_->Complete(handle_, NoError());
    }

    void SetError(const util::Status& status) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (cleanup_ == nullptr) return;
      future_api_->Complete(handle_, status.code(),
                            status.error_message().c_str());
    }

    Future<ResultT> future() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (cleanup_ == nullptr) {
        return Future<ResultT>{};
      }
      return Future<ResultT>{future_api_, handle_.get()};
    }

    bool IsCleanedUp() const {
      std::lock_guard<std::mutex> lock(mutex_);
      return cleanup_ == nullptr;
    }

   private:
    int NoError() const { return static_cast<int>(Error::kErrorOk); }

    void Reset() {
      cleanup_ = nullptr;
      future_api_ = nullptr;
      identifier_ = 0;
      handle_ = {};
    }

    CleanupNotifier* cleanup_ = nullptr;
    ReferenceCountedFutureImpl* future_api_ = nullptr;
    int identifier_ = 0;
    SafeFutureHandle<ResultT> handle_;
    std::mutex destruction_mutex_;
    mutable std::mutex mutex_;
  };

  Promise() = default;

  std::shared_ptr<State> GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
  }

  std::shared_ptr<State> state_;
  mutable std::mutex mutex_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_MAIN_H_
