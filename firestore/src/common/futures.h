// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_FUTURES_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_FUTURES_H_

#include "app/meta/move.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {
namespace internal {

/**
 * Returns a `ReferenceCountedFutureImpl` that can be used to create transient
 * futures not associated with any particular API.
 *
 * Use with caution: futures returned publicly should be created using the
 * `ReferenceCountedFutureImpl` associated with the actual API object.
 */
ReferenceCountedFutureImpl* GetSharedReferenceCountedFutureImpl();

}  // namespace internal

template <typename T>
Future<T> SuccessfulFuture(T&& result) {
  ReferenceCountedFutureImpl* api =
      internal::GetSharedReferenceCountedFutureImpl();
  SafeFutureHandle<T> handle = api->SafeAlloc<T>();

  // The Future API doesn't directly support completing a future with a moved
  // value. Use the callback form to work around this.
  api->Complete(handle, Error::kErrorOk, "",
                [&](T* future_value) { *future_value = Forward<T>(result); });
  return Future<T>(api, handle.get());
}

/**
 * Creates a failed future with the given error code and message.
 */
template <typename T>
Future<T> FailedFuture(Error error, const char* message) {
  ReferenceCountedFutureImpl* api =
      internal::GetSharedReferenceCountedFutureImpl();
  SafeFutureHandle<T> handle = api->SafeAlloc<T>();
  api->Complete(handle, error, message);
  return Future<T>(api, handle.get());
}

/**
 * Returns a failed future suitable for returning from an "invalid" instance.
 */
template <typename T>
Future<T> FailedFuture() {
  static auto* future = new Future<T>(FailedFuture<T>(
      Error::kErrorFailedPrecondition,
      "This instance is in an invalid state. This is because the underlying "
      "Firestore instance has been destructed."));
  return *future;
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_FUTURES_H_
