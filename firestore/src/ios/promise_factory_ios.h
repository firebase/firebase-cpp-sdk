#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_FACTORY_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_FACTORY_IOS_H_

#include <utility>

#include "app/src/future_manager.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "firestore/src/ios/promise_ios.h"

namespace firebase {
class CleanupNotifier;

namespace firestore {

// Wraps a `FutureManager` and allows creating `Promise`s and getting
// `LastResult`s. `ApiEnum` must be an enumeration that lists the async API
// methods each of which must be backed by a future; it is expected to contain
// a member called `kCount` that stands for the total number of the async APIs.
template <typename ApiEnum>
class PromiseFactory {
 public:
  // Extracts the `FutureManager` from the given `object`, relying on the
  // convention that the object has `firestore_internal` member function.
  template <typename T>
  static PromiseFactory Create(T* object) {
    return PromiseFactory(&object->firestore_internal()->cleanup(),
                          &object->firestore_internal()->future_manager());
  }

  PromiseFactory(CleanupNotifier* cleanup, FutureManager* future_manager)
      : cleanup_{NOT_NULL(cleanup)}, future_manager_{NOT_NULL(future_manager)} {
    future_manager_->AllocFutureApi(this, ApisCount());
  }

  ~PromiseFactory() { future_manager_->ReleaseFutureApi(this); }

  PromiseFactory(const PromiseFactory& rhs)
      : cleanup_{rhs.cleanup_}, future_manager_{rhs.future_manager_} {
    future_manager_->AllocFutureApi(this, ApisCount());
  }

  PromiseFactory(PromiseFactory&& rhs)
      : cleanup_{rhs.cleanup_}, future_manager_{rhs.future_manager_} {
    future_manager_->MoveFutureApi(&rhs, this);
  }

  PromiseFactory& operator=(const PromiseFactory&) = delete;
  PromiseFactory& operator=(PromiseFactory&&) = delete;

  template <typename T>
  Promise<T> CreatePromise(ApiEnum index) {
    return Promise<T>{cleanup_, future_api(), static_cast<int>(index)};
  }

 private:
  ReferenceCountedFutureImpl* future_api() {
    return future_manager_->GetFutureApi(this);
  }

  int ApisCount() const { return static_cast<int>(ApiEnum::kCount); }

  CleanupNotifier* cleanup_ = nullptr;
  FutureManager* future_manager_ = nullptr;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_FACTORY_IOS_H_
