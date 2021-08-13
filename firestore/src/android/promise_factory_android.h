/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_

#include "firestore/src/android/promise_android.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {

// Wraps a `FutureManager` and allows creating `Promise`s. `EnumT` must be an
// enumeration that lists the async API methods each of which must be backed by
// a future; it is expected to contain a member called `kCount` that stands for
// the total number of the async APIs.
template <typename EnumT>
class PromiseFactory {
 public:
  explicit PromiseFactory(FirestoreInternal* firestore)
      : firestore_ref_(firestore) {
    firestore_ref_.RunIfValid([this](FirestoreInternal& firestore) {
      firestore.future_manager().AllocFutureApi(this, ApiCount());
    });
  }

  PromiseFactory(const PromiseFactory& rhs)
      : firestore_ref_(rhs.firestore_ref_) {
    firestore_ref_.RunIfValid([this](FirestoreInternal& firestore) {
      firestore.future_manager().AllocFutureApi(this, ApiCount());
    });
  }

  PromiseFactory(PromiseFactory&& rhs)
      : firestore_ref_(Move(rhs.firestore_ref_)) {
    firestore_ref_.RunIfValid([this, &rhs](FirestoreInternal& firestore) {
      firestore.future_manager().MoveFutureApi(&rhs, this);
    });
  }

  ~PromiseFactory() {
    firestore_ref_.RunIfValid([this](FirestoreInternal& firestore) {
      firestore.future_manager().ReleaseFutureApi(this);
    });
  }

  PromiseFactory& operator=(const PromiseFactory&) = delete;
  PromiseFactory& operator=(PromiseFactory&&) = delete;

  // Creates a Promise representing the completion of an underlying Java Task.
  // This can be used to implement APIs that return Futures of some public type.
  // Use MakePromise<void, void>() to create a Future<void>.
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  Promise<PublicT, InternalT, EnumT> MakePromise(
      typename Promise<PublicT, InternalT, EnumT>::Completion* completion =
          nullptr) {
    return firestore_ref_.Run([this, completion](FirestoreInternal* firestore) {
      ReferenceCountedFutureImpl* future_api = nullptr;
      if (firestore) {
        future_api = firestore->future_manager().GetFutureApi(this);
      }
      return Promise<PublicT, InternalT, EnumT>{firestore_ref_, future_api,
                                                completion};
    });
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  Future<PublicT> NewFuture(
      jni::Env& env,
      EnumT op,
      const jni::Task& task,
      typename Promise<PublicT, InternalT, EnumT>::Completion* completion =
          nullptr) {
    if (!env.ok()) return {};

    auto promise = MakePromise<PublicT, InternalT>(completion);
    promise.RegisterForTask(env, op, task);
    return promise.GetFuture();
  }

 private:
  constexpr int ApiCount() const { return static_cast<int>(EnumT::kCount); }

  FirestoreInternalWeakReference firestore_ref_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_
