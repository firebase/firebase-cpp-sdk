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

#ifndef FIREBASE_UMP_SRC_COMMON_CONSENT_INFO_INTERNAL_H_
#define FIREBASE_UMP_SRC_COMMON_CONSENT_INFO_INTERNAL_H_

#include "app/src/assert.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/future.h"
#include "firebase/internal/mutex.h"
#include "firebase/internal/platform.h"
#include "firebase/ump.h"
#include "firebase/ump/types.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#endif

namespace firebase {
namespace ump {
namespace internal {

// Constants representing each ConsentInfo function that returns a Future.
enum ConsentInfoFn {
  kConsentInfoFnRequestConsentInfoUpdate = 0,
  kConsentInfoFnLoadConsentForm,
  kConsentInfoFnShowConsentForm,
  kConsentInfoFnLoadAndShowConsentFormIfRequired,
  kConsentInfoFnShowPrivacyOptionsForm,
  kConsentInfoFnCount
};

class ConsentInfoInternal {
 public:
  virtual ~ConsentInfoInternal();

  ConsentInfoInternal(const ConsentInfoInternal&) = delete;
  ConsentInfoInternal& operator=(const ConsentInfoInternal&) = delete;

  // Implemented in platform-specific code to instantiate a
  // platform-specific subclass.
#if FIREBASE_PLATFORM_ANDROID
  static ConsentInfoInternal* CreateInstance(JNIEnv* jni_env, jobject activity);
  virtual void SetActivity(JNIEnv* jni_env, jobject activity) = 0;
#else
  static ConsentInfoInternal* CreateInstance();
#endif

  virtual ConsentStatus GetConsentStatus() = 0;
  virtual ConsentFormStatus GetConsentFormStatus() = 0;

  virtual Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params) = 0;
  Future<void> RequestConsentInfoUpdateLastResult() {
    MutexLock lock(futures_mutex_);
    return static_cast<const Future<void>&>(
        futures_.LastResult(kConsentInfoFnRequestConsentInfoUpdate));
  }
  virtual Future<void> LoadConsentForm() = 0;

  Future<void> LoadConsentFormLastResult() {
    MutexLock lock(futures_mutex_);
    return static_cast<const Future<void>&>(
        futures_.LastResult(kConsentInfoFnLoadConsentForm));
  }

  virtual Future<void> ShowConsentForm(FormParent parent) = 0;

  Future<void> ShowConsentFormLastResult() {
    MutexLock lock(futures_mutex_);
    return static_cast<const Future<void>&>(
        futures_.LastResult(kConsentInfoFnShowConsentForm));
  }

  virtual Future<void> LoadAndShowConsentFormIfRequired(FormParent parent) = 0;

  Future<void> LoadAndShowConsentFormIfRequiredLastResult() {
    MutexLock lock(futures_mutex_);
    return static_cast<const Future<void>&>(
        futures_.LastResult(kConsentInfoFnLoadAndShowConsentFormIfRequired));
  }

  virtual PrivacyOptionsRequirementStatus
  GetPrivacyOptionsRequirementStatus() = 0;

  virtual Future<void> ShowPrivacyOptionsForm(FormParent parent) = 0;

  Future<void> ShowPrivacyOptionsFormLastResult() {
    MutexLock lock(futures_mutex_);
    return static_cast<const Future<void>&>(
        futures_.LastResult(kConsentInfoFnShowPrivacyOptionsForm));
  }

  virtual bool CanRequestAds() = 0;

  virtual void Reset() = 0;

 protected:
  ConsentInfoInternal();

  static const char* GetConsentRequestErrorMessage(
      ConsentRequestError error_code);

  static const char* GetConsentFormErrorMessage(ConsentFormError error_code);

  SafeFutureHandle<void> CreateFuture() {
    MutexLock lock(futures_mutex_);
    return futures_.SafeAlloc<void>();
  }
  SafeFutureHandle<void> CreateFuture(ConsentInfoFn fn_idx) {
    MutexLock lock(futures_mutex_);
    return futures_.SafeAlloc<void>(fn_idx);
  }

  // Atomically checks whether `fn_idx` already has a pending Future, returning
  // false and populating `in_progress_future_out` if so, or allocating a new
  // `SafeFutureHandle<void>` in `handle_out` and returning true.
  template <typename ErrorEnum>
  bool BeginSingleFlightOperation(ConsentInfoFn fn_idx,
                                  ErrorEnum in_progress_error,
                                  SafeFutureHandle<void>* handle_out,
                                  Future<void>* in_progress_future_out) {
    FIREBASE_ASSERT(handle_out != nullptr && in_progress_future_out != nullptr);
    if (!handle_out || !in_progress_future_out) {
      return false;
    }
    SafeFutureHandle<void> error_handle;
    {
      MutexLock lock(futures_mutex_);
      const FutureBase& last = futures_.LastResult(fn_idx);
      if (last.status() == kFutureStatusPending) {
        error_handle = futures_.SafeAlloc<void>();
      } else {
        *handle_out = futures_.SafeAlloc<void>(fn_idx);
        return true;
      }
    }
    // Complete outside futures_mutex_ so user OnCompletion callbacks never
    // execute while holding futures_mutex_.
    CompleteFuture(error_handle, in_progress_error, nullptr);
    *in_progress_future_out = MakeFuture<void>(&futures_, error_handle);
    return false;
  }

  // Complete a Future<void> with the given error code.
  // ReferenceCountedFutureImpl::Complete() is internally synchronized by its
  // own mutex and releases it before running user OnCompletion callbacks, so
  // we must NOT hold futures_mutex_ across Complete().
  void CompleteFuture(SafeFutureHandle<void> handle, ConsentRequestError error,
                      const char* message = nullptr) {
    futures_.Complete(
        handle, error,
        message ? message : GetConsentRequestErrorMessage(error));
  }
  // Complete the future with the given error code.
  void CompleteFuture(SafeFutureHandle<void> handle, ConsentFormError error,
                      const char* message = nullptr) {
    futures_.Complete(handle, error,
                      message ? message : GetConsentFormErrorMessage(error));
  }

  // Returns the underlying ReferenceCountedFutureImpl. Callers using
  // fn_idx-indexed LastResult/SafeAlloc must synchronize via futures_mutex().
  ReferenceCountedFutureImpl* futures() { return &futures_; }
  Mutex& futures_mutex() { return futures_mutex_; }

 private:
  Mutex futures_mutex_;
  ReferenceCountedFutureImpl futures_;
};

}  // namespace internal
}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_COMMON_CONSENT_INFO_INTERNAL_H_
