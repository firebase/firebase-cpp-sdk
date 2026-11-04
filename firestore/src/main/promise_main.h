// Copyright 2021 Google LLC
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

namespace firebase::firestore {

template <typename ResultT>
class Promise {
 public:
  Promise(CleanupNotifier* cleanup, ReferenceCountedFutureImpl* future_api, int identifier)
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
      old_state = std::exchange(state_, rhs.state_);
    }
    return *this;
  }
  Promise& operator=(Promise&& rhs) noexcept {
    if (this == &rhs) return *this;
    std::shared_ptr<State> old_state;
    {
      std::scoped_lock lock(mutex_, rhs.mutex_);
      old_state = std::exchange(state_, std::move(rhs.state_));
    }
    return *this;
  }

  template <typename DummyT = ResultT, typename = absl::enable_if_t<!std::is_void<DummyT>::value>>
  void SetValue(DummyT result) {
    if (auto state = GetState()) state->SetValue(std::move(result));
  }

  template <typename DummyT = ResultT, typename = absl::enable_if_t<std::is_void<DummyT>::value>>
  void SetValue() {
    if (auto state = GetState()) state->SetValue();
  }

  void SetError(const util::Status& status) {
    SIMPLE_HARD_ASSERT(!status.ok(),
                       "To fulfill a promise with 'ok' status, use Promise::SetValue.");
    if (auto state = GetState()) state->SetError(status);
  }

  Future<ResultT> future() {
    auto state = GetState();
    return state ? state->future() : Future<ResultT>{};
  }

  bool IsCleanedUp() const {
    auto state = GetState();
    return state == nullptr || state->IsCleanedUp();
  }

 private:
  class State {
   public:
    State(CleanupNotifier* cleanup, ReferenceCountedFutureImpl* future_api, int identifier)
        : cleanup_{NOT_NULL(cleanup)},
          future_api_{NOT_NULL(future_api)},
          identifier_{identifier},
          handle_{future_api->SafeAlloc<ResultT>(identifier)} {
      cleanup->RegisterObject(this, [](void* raw_this) {
        auto* this_ptr = static_cast<State*>(raw_this);
        std::unique_lock<std::mutex> destruction_lock(this_ptr->destruction_mutex_,
                                                      std::try_to_lock);
        if (!destruction_lock) return;
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
      if (cleanup != nullptr) cleanup->UnregisterObject(this);
    }

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    template <typename DummyT = ResultT, typename = absl::enable_if_t<!std::is_void<DummyT>::value>>
    void SetValue(DummyT result) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!cleanup_) return;
      future_api_->Complete(handle_, NoError(), /*error_message=*/"",
                            [&](ResultT* value) { *value = std::move(result); });
    }

    template <typename DummyT = ResultT, typename = absl::enable_if_t<std::is_void<DummyT>::value>>
    void SetValue() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!cleanup_) return;
      future_api_->Complete(handle_, NoError());
    }

    void SetError(const util::Status& status) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!cleanup_) return;
      future_api_->Complete(handle_, status.code(), status.error_message().c_str());
    }

    Future<ResultT> future() {
      std::lock_guard<std::mutex> lock(mutex_);
      return cleanup_ ? Future<ResultT>{future_api_, handle_.get()} : Future<ResultT>{};
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

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_MAIN_H_
