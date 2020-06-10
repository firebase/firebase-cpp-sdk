#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_IOS_H_

#include <utility>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "absl/meta/type_traits.h"
#include "firebase/firestore/firestore_errors.h"
#include "Firestore/core/src/util/status.h"
#include "Firestore/core/src/util/statusor.h"

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
  Promise(CleanupNotifier* cleanup, ReferenceCountedFutureImpl* future_api,
          int identifier)
      : cleanup_{NOT_NULL(cleanup)},
        future_api_{NOT_NULL(future_api)},
        identifier_{identifier},
        handle_{future_api->SafeAlloc<ResultT>(identifier)} {
    RegisterForCleanup();
  }

  ~Promise() { UnregisterForCleanup(); }

  Promise(const Promise& rhs)
      : cleanup_{rhs.cleanup_},
        future_api_{rhs.future_api_},
        identifier_{rhs.identifier_},
        handle_{rhs.handle_} {
    RegisterForCleanup();
  }

  Promise(Promise&& rhs) noexcept
      : cleanup_{rhs.cleanup_},
        future_api_{rhs.future_api_},
        identifier_{rhs.identifier_},
        handle_{rhs.handle_} {
    rhs.UnregisterForCleanup();
    rhs.cleanup_ = nullptr;
    rhs.future_api_ = nullptr;
    rhs.identifier_ = 0;
    rhs.handle_ = {};

    RegisterForCleanup();
  }

  Promise& operator=(const Promise& rhs) {
    UnregisterForCleanup();

    cleanup_ = rhs.cleanup_;
    future_api_ = rhs.future_api_;
    identifier_ = rhs.identifier_;
    handle_ = rhs.handle_;

    RegisterForCleanup();

    return *this;
  }

  Promise& operator=(Promise&& rhs) noexcept {
    rhs.UnregisterForCleanup();
    UnregisterForCleanup();

    cleanup_ = rhs.cleanup_;
    future_api_ = rhs.future_api_;
    identifier_ = rhs.identifier_;
    handle_ = rhs.handle_;

    RegisterForCleanup();

    rhs.cleanup_ = nullptr;
    rhs.future_api_ = nullptr;
    rhs.identifier_ = 0;
    rhs.handle_ = {};

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
    if (IsCleanedUp()) {
      return;
    }
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
    if (IsCleanedUp()) {
      return;
    }
    future_api_->Complete(handle_, NoError());
  }

  void SetError(const util::Status& status) {
    HARD_ASSERT_IOS(
        !status.ok(),
        "To fulfill a promise with 'ok' status, use Promise::SetValue.");
    if (IsCleanedUp()) {
      return;
    }

    future_api_->Complete(handle_, status.code(),
                          status.error_message().c_str());
  }

  Future<ResultT> future() {
    if (IsCleanedUp()) {
      return Future<ResultT>{};
    }

    return Future<ResultT>{future_api_, handle_.get()};
  }

 private:
  Promise() = default;

  int NoError() const { return static_cast<int>(Error::kErrorOk); }

  // Note: `CleanupFn` is not used because `Promise` is a header-only class, to
  // avoid a circular dependency between headers.
  void RegisterForCleanup() {
    if (IsCleanedUp()) {
      return;
    }

    cleanup_->RegisterObject(this, [](void* raw_this) {
      auto* this_ptr = static_cast<Promise*>(raw_this);
      *this_ptr = {};
    });
  }

  void UnregisterForCleanup() {
    if (IsCleanedUp()) {
      return;
    }
    cleanup_->UnregisterObject(this);
  }

  bool IsCleanedUp() const { return cleanup_ == nullptr; }

  CleanupNotifier* cleanup_ = nullptr;
  ReferenceCountedFutureImpl* future_api_ = nullptr;
  int identifier_ = 0;
  SafeFutureHandle<ResultT> handle_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_PROMISE_IOS_H_
