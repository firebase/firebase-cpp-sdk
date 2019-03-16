// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_PROMISE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_PROMISE_H_

#include <memory>
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "auth/src/data.h"
#include "auth/src/include/firebase/auth/types.h"

namespace firebase {
namespace auth {

// Simplifies working with a Firebase future.
//
// Promise preallocates a result in the constructor and keeps track of the
// handle. Promise doesn't own any memory and can be freely copied. The given
// ReferenceCountedFutureImpl is presumed to stay valid for the whole lifetime
// of this Promise.
//
// Promise guarantees that it refers to a valid future backed by last_results
// array.
template <typename ResultT>
class Promise {
 public:
  // Creates a future backed by LastResults cache.
  Promise(ReferenceCountedFutureImpl* const future_manager,
          const int identifier)
      : future_manager_(*future_manager),
        identifier_(identifier),
        handle_(future_manager->SafeAlloc<ResultT>(identifier, ResultT())) {
    FutureBase future_base(&future_manager_, handle_.get());
    future_ = static_cast<firebase::Future<ResultT>&>(future_base);
  }

  void CompleteWithResult(const ResultT& result) {
    future_manager_.CompleteWithResult(handle_, kAuthErrorNone, "", result);
  }

  void Fail(AuthError error, const char* const message) {
    if (message) {
      future_manager_.Complete(handle_, error, message);
    } else {
      future_manager_.Complete(handle_, error);
    }
  }

  void Fail(AuthError error, const std::string& message) {
    Fail(error, message.c_str());
  }

  Future<ResultT> InvalidateLastResult() {
    future_manager_.InvalidateLastResult(identifier_);
    return LastResult();
  }

  Future<ResultT> LastResult() {
    return static_cast<const firebase::Future<ResultT>&>(
        future_manager_.LastResult(identifier_));
  }

  Future<ResultT> future() { return future_; }

 private:
  ReferenceCountedFutureImpl& future_manager_;
  const int identifier_;
  const SafeFutureHandle<ResultT> handle_;
  firebase::Future<ResultT> future_;
};

template <>
class Promise<void> {
 public:
  Promise(ReferenceCountedFutureImpl* const future_manager,
          const int identifier)
      : future_manager_(*future_manager),
        identifier_(identifier),
        handle_(future_manager->SafeAlloc<void>(identifier)) {
    FutureBase future_base(&future_manager_, handle_.get());
    future_ = static_cast<firebase::Future<void>&>(future_base);
  }

  void Complete() { future_manager_.Complete(handle_, kAuthErrorNone, ""); }

  void Fail(AuthError error, const char* const message) {
    if (message) {
      future_manager_.Complete(handle_, error, message);
    } else {
      future_manager_.Complete(handle_, error);
    }
  }

  void Fail(AuthError error, const std::string& message) {
    Fail(error, message.c_str());
  }

  Future<void> InvalidateLastResult() {
    future_manager_.InvalidateLastResult(identifier_);
    return LastResult();
  }

  Future<void> LastResult() {
    return static_cast<const firebase::Future<void>&>(
        future_manager_.LastResult(identifier_));
  }

  Future<void> future() { return future_; }

 private:
  ReferenceCountedFutureImpl& future_manager_;
  const int identifier_;
  const SafeFutureHandle<void> handle_;
  firebase::Future<void> future_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_PROMISE_H_
