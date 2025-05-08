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
#include "firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#endif

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// Constants representing each ConsentInfo function that returns a Future.
enum ConsentInfoFn {
  kConsentInfoFnRequestConsentInfoUpdate,
  kConsentInfoFnLoadConsentForm,
  kConsentInfoFnShowConsentForm,
  kConsentInfoFnLoadAndShowConsentFormIfRequired,
  kConsentInfoFnShowPrivacyOptionsForm,
  kConsentInfoFnCount
};

class ConsentInfoInternal {
 public:
  virtual ~ConsentInfoInternal();

  // Implemented in platform-specific code to instantiate a
  // platform-specific subclass.
#if FIREBASE_PLATFORM_ANDROID
  static ConsentInfoInternal* CreateInstance(JNIEnv* jni_env, jobject activity);
#else
  static ConsentInfoInternal* CreateInstance();
#endif

  virtual ConsentStatus GetConsentStatus() = 0;
  virtual ConsentFormStatus GetConsentFormStatus() = 0;

  virtual Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params) = 0;
  Future<void> RequestConsentInfoUpdateLastResult() {
    return static_cast<const Future<void>&>(
        futures()->LastResult(kConsentInfoFnRequestConsentInfoUpdate));
  }
  virtual Future<void> LoadConsentForm() = 0;

  Future<void> LoadConsentFormLastResult() {
    return static_cast<const Future<void>&>(
        futures()->LastResult(kConsentInfoFnLoadConsentForm));
  }

  virtual Future<void> ShowConsentForm(FormParent parent) = 0;

  Future<void> ShowConsentFormLastResult() {
    return static_cast<const Future<void>&>(
        futures()->LastResult(kConsentInfoFnShowConsentForm));
  }

  virtual Future<void> LoadAndShowConsentFormIfRequired(FormParent parent) = 0;

  Future<void> LoadAndShowConsentFormIfRequiredLastResult() {
    return static_cast<const Future<void>&>(
        futures()->LastResult(kConsentInfoFnLoadAndShowConsentFormIfRequired));
  }

  virtual PrivacyOptionsRequirementStatus
  GetPrivacyOptionsRequirementStatus() = 0;

  virtual Future<void> ShowPrivacyOptionsForm(FormParent parent) = 0;

  Future<void> ShowPrivacyOptionsFormLastResult() {
    return static_cast<const Future<void>&>(
        futures()->LastResult(kConsentInfoFnShowPrivacyOptionsForm));
  }

  virtual bool CanRequestAds() = 0;

  virtual void Reset() = 0;

 protected:
  ConsentInfoInternal();

  static const char* GetConsentRequestErrorMessage(
      ConsentRequestError error_code);

  static const char* GetConsentFormErrorMessage(ConsentFormError error_code);

  SafeFutureHandle<void> CreateFuture() { return futures()->SafeAlloc<void>(); }
  SafeFutureHandle<void> CreateFuture(ConsentInfoFn fn_idx) {
    return futures()->SafeAlloc<void>(fn_idx);
  }

  // Complete a Future<void> with the given error code.
  void CompleteFuture(SafeFutureHandle<void> handle, ConsentRequestError error,
                      const char* message = nullptr) {
    return futures()->Complete(
        handle, error,
        message ? message : GetConsentRequestErrorMessage(error));
  }
  // Complete the future with the given error code.
  void CompleteFuture(SafeFutureHandle<void> handle, ConsentFormError error,
                      const char* message = nullptr) {
    return futures()->Complete(
        handle, error, message ? message : GetConsentFormErrorMessage(error));
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
