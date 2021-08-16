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

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_PROMISE_FACTORY_MAIN_H_
