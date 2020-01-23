#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_IOS_H_

#include <utility>

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "absl/meta/type_traits.h"
#include "Firestore/core/include/firebase/firestore/firestore_errors.h"
#include "Firestore/core/src/firebase/firestore/util/status.h"
#include "Firestore/core/src/firebase/firestore/util/statusor.h"

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
template <typename ResultT>
class Promise {
 public:
  // Creates a future backed by `LastResults` cache.
  Promise(ReferenceCountedFutureImpl* future_api, int identifier)
      : future_api_{NOT_NULL(future_api)},
        identifier_{identifier},
        handle_{future_api->SafeAlloc<ResultT>(identifier)} {}

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
    future_api_->Complete(handle_, NoError());
  }

  void SetError(const util::Status& status) {
    HARD_ASSERT_IOS(
        !status.ok(),
        "To fulfill a promise with 'ok' status, use Promise::SetValue.");
    future_api_->Complete(handle_, status.code(),
                          status.error_message().c_str());
  }

  Future<ResultT> future() {
    return Future<ResultT>{future_api_, handle_.get()};
  }

 private:
  int NoError() const { return static_cast<int>(Error::Ok); }

  ReferenceCountedFutureImpl* future_api_ = nullptr;
  int identifier_ = 0;
  SafeFutureHandle<ResultT> handle_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_IOS_H_
