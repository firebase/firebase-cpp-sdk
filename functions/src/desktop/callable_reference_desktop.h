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

#ifndef FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_
#define FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_

#include "app/rest/transport_curl.h"
#include "app/rest/transport_interface.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase {
namespace functions {
namespace internal {

class HttpsCallableRequest : public rest::Request {
 public:
  // Mark the transfer completed.
  void MarkCompleted() override;

  // Mark the transfer failed.
  void MarkFailed() override;

  void set_future_impl(ReferenceCountedFutureImpl* impl) {
    future_impl_ = impl;
  }

  void set_future_handle(SafeFutureHandle<HttpsCallableResult> handle) {
    future_handle_ = handle;
  }

  void set_response(rest::Response* response) { response_ = response; }

 private:
  ReferenceCountedFutureImpl* future_impl_;
  SafeFutureHandle<HttpsCallableResult> future_handle_;
  rest::Response* response_;
};

class HttpsCallableReferenceInternal {
 public:
  HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                 const char* name);
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
  // TODO(klimt): Do we really need this method?
  Functions* functions() const;

  // Asynchronously calls this CallableReference.
  Future<HttpsCallableResult> Call() { return Call(Variant::Null()); }
  Future<HttpsCallableResult> Call(const Variant& data);
  Future<HttpsCallableResult> CallLastResult();

  // This is a static method so that the Request can construct an
  // HttpsCallableResult, since this is a friend class for it.
  static void ResolveFuture(ReferenceCountedFutureImpl* future_impl,
                            SafeFutureHandle<HttpsCallableResult> future_handle,
                            rest::Response* response);

  // Pointer to the FunctionsInternal instance we are a part of.
  FunctionsInternal* functions_internal() const { return functions_; }

 private:
  // Returns the auth token for the current user, if there is a current user,
  // and they have a token, and auth exists as part of the app.
  // Otherwise, returns an empty string.
  std::string GetAuthToken() const;

  // Get the Future for the HttpsCallableReferenceInternal.
  ReferenceCountedFutureImpl* future();

  // Keep track of the Functions object for managing Futures.
  FunctionsInternal* functions_;

  // The name of the endpoint this reference points to.
  std::string name_;

  rest::TransportCurl transport_;
  // For now, we only allow one request per reference at a time in C++.
  // TODO(klimt): Figure out the lifetime issues enough to allow multiple
  // simultaneous requests.
  HttpsCallableRequest request_;
  rest::Response response_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_
