/*
 * Copyright 2023 Google LLC
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

#ifndef FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_

#include "app/src/cleanup_notifier.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/future.h"
#include "firebase/gma/ump.h"
#include "firebase/gma/ump/types.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// Constants representing each ConsentInfo function that returns a Future.
enum ConsentInfoFn {
  kConsentInfoFnRequestConsentStatus,
  kConsentInfoFnLoadConsentForm,
  kConsentInfoFnShowConsentForm,
  kConsentInfoFnCount
};

class ConsentInfoInternal {
 public:
  virtual ~ConsentInfoInternal();

  // Implemented in platform-specific code to instantiate a
  // platform-specific subclass.
  static ConsentInfoInternal* CreateInstance();

  virtual ConsentStatus GetConsentStatus() const = 0;
  virtual ConsentFormStatus GetConsentFormStatus() const = 0;

  virtual Future<ConsentStatus> RequestConsentStatus(
      const ConsentRequestParameters& params) = 0;
  Future<ConsentStatus> RequestConsentStatusLastResult() {
    return static_cast<const Future<ConsentStatus>&>(
        futures()->LastResult(kConsentInfoFnRequestConsentStatus));
  }
  virtual Future<ConsentFormStatus> LoadConsentForm() = 0;

  Future<ConsentFormStatus> LoadConsentFormLastResult() {
    return static_cast<const Future<ConsentFormStatus>&>(
        futures()->LastResult(kConsentInfoFnLoadConsentForm));
  }

  virtual Future<ConsentStatus> ShowConsentForm(FormParent parent) = 0;

  Future<ConsentStatus> ShowConsentFormLastResult() {
    return static_cast<const Future<ConsentStatus>&>(
        futures()->LastResult(kConsentInfoFnShowConsentForm));
  }

  virtual void Reset() = 0;

 protected:
  ConsentInfoInternal();

  template <typename T>
  SafeFutureHandle<T> CreateFuture() {
    return futures()->SafeAlloc<T>();
  }
  template <typename T>
  SafeFutureHandle<T> CreateFuture(ConsentInfoFn fn_idx) {
    return futures()->SafeAlloc<T>(fn_idx);
  }

  // With result and automatically generated error message.
  template <typename T>
  void CompleteFuture(SafeFutureHandle<T> handle, ConsentRequestError error,
                      T& result) {
    return futures()->CompleteWithResult<T>(
        handle, error, GetConsentRequestErrorMessage(error), result);
  }
  template <typename T>
  void CompleteFuture(SafeFutureHandle<T> handle, ConsentFormError error,
                      T& result) {
    return futures()->CompleteWithResult<T>(
        handle, error, GetConsentFormErrorMessage(error), result);
  }

  // No result data, just an error code.
  template <typename T>
  void CompleteFuture(SafeFutureHandle<T> handle, ConsentRequestError error) {
    return futures()->Complete(handle, error,
                               GetConsentRequestErrorMessage(error));
  }
  // No result data, just an error code.
  template <typename T>
  void CompleteFuture(SafeFutureHandle<T> handle, ConsentFormError error) {
    return futures()->Complete(handle, error,
                               GetConsentFormErrorMessage(error));
  }

  ReferenceCountedFutureImpl* futures() { return &futures_; }
  CleanupNotifier* cleanup() { return &cleanup_; }

 private:
  ReferenceCountedFutureImpl futures_;
  CleanupNotifier cleanup_;
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_
