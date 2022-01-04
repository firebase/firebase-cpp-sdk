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
      "The object that issued this future is in an invalid state. This can be "
      "because the object was default-constructed and never reassigned, "
      "the object was moved from, or the Firestore instance with which the "
      "object was associated has been destroyed."));
  return *future;
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_FUTURES_H_
