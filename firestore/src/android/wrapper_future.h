#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_FUTURE_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_FUTURE_H_

#include <jni.h>

#include "app/meta/move.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/wrapper.h"

namespace firebase {
namespace firestore {

// This is a wrapper class that has Future support. EnumType is the enum class
// type that identify Future APIs for each subclass and FnCount is the last enum
// value of EnumType to represent the number of Future APIs.
template <typename EnumType, EnumType FnCount>
class WrapperFuture : public Wrapper {
 public:
  // A global reference will be created from obj. The caller is responsible for
  // cleaning up any local references to obj after the constructor returns.
  WrapperFuture(FirestoreInternal* firestore, jobject obj)
      : Wrapper(firestore, obj) {
    firestore_->future_manager().AllocFutureApi(this,
                                                static_cast<int>(FnCount));
  }

  WrapperFuture(const WrapperFuture& rhs) : Wrapper(rhs) {
    firestore_->future_manager().AllocFutureApi(this,
                                                static_cast<int>(FnCount));
  }

  WrapperFuture(WrapperFuture&& rhs) : Wrapper(firebase::Move(rhs)) {
    firestore_->future_manager().MoveFutureApi(&rhs, this);
  }

  ~WrapperFuture() override {
    firestore_->future_manager().ReleaseFutureApi(this);
  }

 protected:
  // Gets the reference-counted Future implementation of this instance, which
  // can be used to create a Future.
  ReferenceCountedFutureImpl* ref_future() {
    return firestore_->future_manager().GetFutureApi(this);
  }

  // Creates a Promise representing the completion of an underlying Java Task.
  // This can be used to implement APIs that return Futures of some public type.
  // Use MakePromise<void, void>() to create a Future<void>.
  template <typename PublicType, typename InternalType>
  Promise<PublicType, InternalType, EnumType> MakePromise() {
    return Promise<PublicType, InternalType, EnumType>{ref_future(),
                                                       firestore_};
  }

  // A helper that generalizes the logic for FooLastResult() of each Foo()
  // defined.
  template <typename ResultType>
  Future<ResultType> LastResult(EnumType index) {
    const auto& result = ref_future()->LastResult(static_cast<int>(index));
    return static_cast<const Future<ResultType>&>(result);
  }
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_FUTURE_H_
