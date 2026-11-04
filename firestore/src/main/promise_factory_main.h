// Copyright 2021 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_FACTORY_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_FACTORY_MAIN_H_

#include <utility>
#include "app/src/future_manager.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/main/promise_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
class CleanupNotifier;
namespace firestore {

template <typename ApiEnum>
class PromiseFactory {
 public:
  template <typename T>
  static PromiseFactory Create(T* object) {
    auto* firestore = NOT_NULL(NOT_NULL(object)->firestore_internal());
    return PromiseFactory(&firestore->cleanup(), &firestore->future_manager());
  }

  PromiseFactory(CleanupNotifier* cleanup, FutureManager* future_manager)
      : cleanup_{NOT_NULL(cleanup)}, future_manager_{NOT_NULL(future_manager)} {
    future_manager_->AllocFutureApi(this, ApisCount());
  }

  ~PromiseFactory() {
    if (future_manager_) future_manager_->ReleaseFutureApi(this);
  }

  PromiseFactory(const PromiseFactory& rhs)
      : cleanup_{rhs.cleanup_}, future_manager_{rhs.future_manager_} {
    if (future_manager_) future_manager_->AllocFutureApi(this, ApisCount());
  }

  PromiseFactory(PromiseFactory&& rhs) noexcept
      : cleanup_{std::exchange(rhs.cleanup_, nullptr)},
        future_manager_{std::exchange(rhs.future_manager_, nullptr)} {
    if (future_manager_) future_manager_->MoveFutureApi(&rhs, this);
  }

  PromiseFactory& operator=(const PromiseFactory&) = delete;
  PromiseFactory& operator=(PromiseFactory&&) = delete;

  template <typename T>
  Promise<T> CreatePromise(ApiEnum index) {
    return Promise<T>{cleanup_, future_api(), static_cast<int>(index)};
  }

 private:
  ReferenceCountedFutureImpl* future_api() { return future_manager_->GetFutureApi(this); }
  int ApisCount() const { return static_cast<int>(ApiEnum::kCount); }

  CleanupNotifier* cleanup_ = nullptr;
  FutureManager* future_manager_ = nullptr;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_FACTORY_MAIN_H_
