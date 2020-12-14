#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_

#include "firestore/src/android/promise_android.h"
#include "firestore/src/common/type_mapping.h"

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
      : firestore_(firestore) {
    firestore_->future_manager().AllocFutureApi(this, ApiCount());
  }

  PromiseFactory(const PromiseFactory& rhs) : firestore_(rhs.firestore_) {
    firestore_->future_manager().AllocFutureApi(this, ApiCount());
  }

  PromiseFactory(PromiseFactory&& rhs) : firestore_(rhs.firestore_) {
    firestore_->future_manager().MoveFutureApi(&rhs, this);
  }

  ~PromiseFactory() { firestore_->future_manager().ReleaseFutureApi(this); }

  PromiseFactory& operator=(const PromiseFactory&) = delete;
  PromiseFactory& operator=(PromiseFactory&&) = delete;

  // Creates a Promise representing the completion of an underlying Java Task.
  // This can be used to implement APIs that return Futures of some public type.
  // Use MakePromise<void, void>() to create a Future<void>.
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  Promise<PublicT, InternalT, EnumT> MakePromise(
      typename Promise<PublicT, InternalT, EnumT>::Completion* completion =
          nullptr) {
    return Promise<PublicT, InternalT, EnumT>{future_api(), firestore_,
                                              completion};
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  Future<PublicT> NewFuture(
      jni::Env& env, EnumT op, const jni::Object& task,
      typename Promise<PublicT, InternalT, EnumT>::Completion* completion =
          nullptr) {
    if (!env.ok()) return {};

    auto promise = MakePromise<PublicT, InternalT>(completion);
    promise.RegisterForTask(env, op, task);
    return promise.GetFuture();
  }

 private:
  // Gets the reference-counted Future implementation of this instance, which
  // can be used to create a Future.
  ReferenceCountedFutureImpl* future_api() {
    return firestore_->future_manager().GetFutureApi(this);
  }

  constexpr int ApiCount() const { return static_cast<int>(EnumT::kCount); }

  FirestoreInternal* firestore_ = nullptr;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_
