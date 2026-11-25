// Copyright 2018 Google LLC
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

#ifndef FIREBASE_FUNCTIONS_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_
#define FIREBASE_FUNCTIONS_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "app/rest/transport_curl.h"
#include "app/rest/transport_interface.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase {
namespace functions {
namespace internal {

// Heap-allocated per-call operation state so Call() never captures `this` or
// stack references, supports concurrent calls on the same reference, and
// drains safely when FunctionsInternal tears down.
class CallOperation {
 public:
  CallOperation(std::shared_ptr<detail::FutureImplCore> future_core,
                SafeFutureHandle<HttpsCallableResult> future_handle);

  rest::Request& request() { return request_; }

  // Launches the synchronous curl request on a detached worker thread unless
  // CancelAndWait() has already been called.
  void Launch(const std::shared_ptr<CallOperation>& self,
              const std::string* app_check_token = nullptr);

  // Marks the operation cancelled and blocks until any in-flight curl transfer
  // has returned (without blocking if the worker has already moved on to user
  // completion callbacks).
  void CancelAndWait();

  bool is_completed() const {
    return completed_.load(std::memory_order_acquire);
  }

 private:
  void RunOnWorker();

  std::shared_ptr<detail::FutureImplCore> future_core_;
  SafeFutureHandle<HttpsCallableResult> future_handle_;
  rest::TransportCurl transport_;
  rest::Request request_;
  rest::Response response_;

  std::mutex state_mutex_;
  std::condition_variable state_cv_;
  bool cancelled_ = false;
  bool in_flight_ = false;
  std::atomic<bool> completed_{false};
};

class HttpsCallableReferenceInternal {
 public:
  HttpsCallableReferenceInternal(std::shared_ptr<FunctionsInternal> functions,
                                 const char* url,
                                 const HttpsCallableOptions& options);
  ~HttpsCallableReferenceInternal();

  // Copy constructor. It's totally okay (and efficient) to copy
  // HttpsCallableReferenceInternal instances, as they simply point to the same
  // location.
  HttpsCallableReferenceInternal(const HttpsCallableReferenceInternal& other);

  // Copy assignment operator. It's totally okay (and efficient) to copy
  // HttpsCallableReferenceInternal instances, as they simply point to the same
  // location.
  HttpsCallableReferenceInternal& operator=(
      const HttpsCallableReferenceInternal& other);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  // Move constructor. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal(HttpsCallableReferenceInternal&& other);

  // Move assignment operator. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal& operator=(
      HttpsCallableReferenceInternal&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  // Gets the functions to which we refer.
  Functions functions() const;

  // Asynchronously calls this CallableReference.
  Future<HttpsCallableResult> Call() { return Call(Variant::Null()); }
  Future<HttpsCallableResult> Call(const Variant& data);
  Future<HttpsCallableResult> CallLastResult();

  // Parses the response and completes `future_core` for `future_handle`.
  // Static friend helper so CallOperation can construct HttpsCallableResult.
  static void ResolveFuture(
      const std::shared_ptr<detail::FutureImplCore>& future_core,
      SafeFutureHandle<HttpsCallableResult> future_handle, Error error,
      const std::string& error_description, Variant data);

  // Pointer to the FunctionsInternal instance we are a part of.
  FunctionsInternal* functions_internal() const { return functions_.get(); }

  bool Equals(const HttpsCallableReferenceInternal& other) const {
    return functions_ == other.functions_ && url_ == other.url_ &&
           options_.limited_use_app_check_token ==
               other.options_.limited_use_app_check_token;
  }

 private:
  // Returns the auth token for the current user, if there is a current user,
  // and they have a token, and auth exists as part of the app.
  // Otherwise, returns an empty string.
  std::string GetAuthToken() const;

  // Get the Future for the HttpsCallableReferenceInternal.
  ReferenceCountedFutureImpl* future();

  // Keep track of the Functions object for managing Futures and keeping
  // AppState alive.
  std::shared_ptr<FunctionsInternal> functions_;

  // The URL of the endpoint this reference points to.
  std::string url_;

  // Options for the callable reference.
  HttpsCallableOptions options_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_
