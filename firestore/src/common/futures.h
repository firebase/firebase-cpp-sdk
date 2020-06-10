#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_FUTURES_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_FUTURES_H_

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

namespace internal {
/**
 * Do not use this directly. Originally, this was inlined as a lambda. But that
 * breaks on older compilers, i.e. at least gcc-4.8.5.
 */
template <typename T>
Future<T> CreateFailedFuture(
    ReferenceCountedFutureImpl* ref_counted_future_impl) {
  SafeFutureHandle<T> handle = ref_counted_future_impl->SafeAlloc<T>();
  ref_counted_future_impl->Complete(
      handle, Error::kErrorFailedPrecondition,
      "This instance is in an invalid state. This could either because the "
      "underlying Firestore instance has been destructed or because you're "
      "running on an unsupported platform. Currently the Firestore C++/Unity "
      "SDK only supports iOS / Android devices.");
  return Future<T>(ref_counted_future_impl, handle.get());
}

}  // namespace internal

/**
 * Returns a failed future suitable for returning from a stub or "invalid"
 * instance.
 *
 * Note that without proper Desktop support, we use firsetore_stub.cc which
 * uses FailedFuture() for its own methods but constructs "invalid" instances
 * of DocumentReference, etc. which also use FailedFuture(). So the wrapped
 * error must be generic enough to cover both unimplemented desktop support as
 * well as normal "invalid" instances (i.e. the underlying Firestore instance
 * has been destructed).
 */
template <typename T>
Future<T> FailedFuture() {
  static ReferenceCountedFutureImpl ref_counted_future_impl(
      /*last_result_count=*/0);
  static Future<T> future =
      internal::CreateFailedFuture<T>(&ref_counted_future_impl);
  return future;
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_FUTURES_H_
