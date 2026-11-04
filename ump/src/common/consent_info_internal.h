// Copyright 2023 Google LLC

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
  virtual Future<void> LoadConsentForm() = 0;
  virtual Future<void> ShowConsentForm(FormParent parent) = 0;
  virtual Future<void> LoadAndShowConsentFormIfRequired(FormParent parent) = 0;
  virtual PrivacyOptionsRequirementStatus
  GetPrivacyOptionsRequirementStatus() = 0;
  virtual Future<void> ShowPrivacyOptionsForm(FormParent parent) = 0;
  virtual bool CanRequestAds() = 0;
  virtual void Reset() = 0;

  Future<void> LastResult(ConsentInfoFn fn) {
    MutexLock lock(futures_mutex_);
    return static_cast<const Future<void>&>(futures_.LastResult(fn));
  }
  Future<void> RequestConsentInfoUpdateLastResult() {
    return LastResult(kConsentInfoFnRequestConsentInfoUpdate);
  }
  Future<void> LoadConsentFormLastResult() {
    return LastResult(kConsentInfoFnLoadConsentForm);
  }
  Future<void> ShowConsentFormLastResult() {
    return LastResult(kConsentInfoFnShowConsentForm);
  }
  Future<void> LoadAndShowConsentFormIfRequiredLastResult() {
    return LastResult(kConsentInfoFnLoadAndShowConsentFormIfRequired);
  }
  Future<void> ShowPrivacyOptionsFormLastResult() {
    return LastResult(kConsentInfoFnShowPrivacyOptionsForm);
  }

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

  template <typename ErrorEnum>
  bool BeginSingleFlightOperation(ConsentInfoFn fn_idx,
                                  ErrorEnum in_progress_error,
                                  SafeFutureHandle<void>* handle_out,
                                  Future<void>* in_progress_future_out) {
    if (!handle_out || !in_progress_future_out) return false;
    SafeFutureHandle<void> error_handle;
    {
      MutexLock lock(futures_mutex_);
      if (futures_.LastResult(fn_idx).status() == kFutureStatusPending) {
        error_handle = futures_.SafeAlloc<void>();
      } else {
        *handle_out = futures_.SafeAlloc<void>(fn_idx);
        return true;
      }
    }
    CompleteFuture(error_handle, in_progress_error, nullptr);
    *in_progress_future_out = MakeFuture<void>(&futures_, error_handle);
    return false;
  }

  void CompleteFuture(SafeFutureHandle<void> handle, ConsentRequestError error,
                      const char* message = nullptr) {
    futures_.Complete(
        handle, error,
        message ? message : GetConsentRequestErrorMessage(error));
  }
  void CompleteFuture(SafeFutureHandle<void> handle, ConsentFormError error,
                      const char* message = nullptr) {
    futures_.Complete(handle, error,
                      message ? message : GetConsentFormErrorMessage(error));
  }

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
