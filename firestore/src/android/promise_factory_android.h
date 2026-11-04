// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_

#include <utility>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/jni/task.h"

namespace firebase::firestore {

template <typename EnumT>
class PromiseFactory {
 public:
  explicit PromiseFactory(FirestoreInternal* firestore)
      : firestore_ref_(firestore) {
    firestore_ref_.RunIfValid([this](FirestoreInternal& f) {
      f.future_manager().AllocFutureApi(this, ApiCount());
    });
  }

  PromiseFactory(const PromiseFactory& rhs)
      : firestore_ref_(rhs.firestore_ref_) {
    firestore_ref_.RunIfValid([this](FirestoreInternal& f) {
      f.future_manager().AllocFutureApi(this, ApiCount());
    });
  }

  PromiseFactory(PromiseFactory&& rhs) noexcept
      : firestore_ref_(std::move(rhs.firestore_ref_)) {
    firestore_ref_.RunIfValid([this, &rhs](FirestoreInternal& f) {
      f.future_manager().MoveFutureApi(&rhs, this);
    });
  }

  ~PromiseFactory() {
    firestore_ref_.RunIfValid([this](FirestoreInternal& f) {
      f.future_manager().ReleaseFutureApi(this);
    });
  }

  PromiseFactory& operator=(const PromiseFactory&) = delete;
  PromiseFactory& operator=(PromiseFactory&&) = delete;

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  Promise<PublicT, InternalT, EnumT> MakePromise(
      typename Promise<PublicT, InternalT, EnumT>::Completion* completion =
          nullptr) {
    return firestore_ref_.Run([this, completion](FirestoreInternal* f) {
      return Promise<PublicT, InternalT, EnumT>{
          firestore_ref_, f ? f->future_manager().GetFutureApi(this) : nullptr,
          completion};
    });
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  Future<PublicT> NewFuture(
      jni::Env& env, EnumT op, const jni::Task& task,
      typename Promise<PublicT, InternalT, EnumT>::Completion* completion =
          nullptr) {
    auto promise = MakePromise<PublicT, InternalT>(completion);
    if (!env.ok() || !task) {
      jni::ExceptionClearGuard block(env);
      promise.RegisterForTask(env, op, jni::Task(),
                              "Failed to create Java Task");
    } else {
      promise.RegisterForTask(env, op, task);
    }
    return promise.GetFuture();
  }

 private:
  static constexpr int ApiCount() { return static_cast<int>(EnumT::kCount); }
  FirestoreInternalWeakReference firestore_ref_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_FACTORY_ANDROID_H_
