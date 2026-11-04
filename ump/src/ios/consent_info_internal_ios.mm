// Copyright 2023 Google LLC

#include "ump/src/ios/consent_info_internal_ios.h"

#import <UIKit/UIKit.h>

#include "app/src/assert.h"
#include "app/src/thread.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace ump {
namespace internal {

ConsentInfoInternalIos* ConsentInfoInternalIos::s_instance = nullptr;
firebase::Mutex ConsentInfoInternalIos::s_instance_mutex;
unsigned int ConsentInfoInternalIos::s_instance_tag = 0;

ConsentInfoInternal* ConsentInfoInternal::CreateInstance() {
  return new ConsentInfoInternalIos();
}

ConsentInfoInternalIos::ConsentInfoInternalIos() : loaded_form_(nil) {
  MutexLock lock(s_instance_mutex);
  FIREBASE_ASSERT(s_instance == nullptr);
  s_instance = this;
  s_instance_tag++;
}

ConsentInfoInternalIos::~ConsentInfoInternalIos() {
  MutexLock lock(s_instance_mutex);
  loaded_form_ = nil;
  if (s_instance == this) s_instance = nullptr;
}

namespace {

bool ValidateFormParent(FormParent parent) {
  return parent == nil ||
         ([static_cast<id>(parent) isKindOfClass:[UIViewController class]] ==
          YES);
}

ConsentRequestError CppRequestErrorFromIosError(NSError* error) {
  if (!error) return kConsentRequestSuccess;
  if (error.domain && ![error.domain isEqualToString:UMPRequestErrorDomain]) {
    return ([error.domain isEqualToString:NSURLErrorDomain] &&
            error.code == NSURLErrorTimedOut)
               ? kConsentRequestErrorTimeout
               : kConsentRequestErrorUnknown;
  }
  switch (error.code) {
    case UMPRequestErrorCodeInternal:
      return kConsentRequestErrorInternal;
    case UMPRequestErrorCodeInvalidAppID:
      return kConsentRequestErrorInvalidAppId;
    case UMPRequestErrorCodeMisconfiguration:
      return kConsentRequestErrorMisconfiguration;
    case UMPRequestErrorCodeNetwork:
      return kConsentRequestErrorNetwork;
    default:
      return kConsentRequestErrorUnknown;
  }
}

ConsentFormError CppFormErrorFromIosError(NSError* error) {
  if (!error) return kConsentFormSuccess;
  if (error.domain && ![error.domain isEqualToString:UMPFormErrorDomain]) {
    if ([error.domain isEqualToString:UMPRequestErrorDomain] &&
        error.code == UMPRequestErrorCodeNetwork) {
      return kConsentFormErrorNetwork;
    }
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
      return error.code == NSURLErrorTimedOut ? kConsentFormErrorTimeout
                                              : kConsentFormErrorNetwork;
    }
    return kConsentFormErrorUnknown;
  }
  switch (error.code) {
    case UMPFormErrorCodeInternal:
      return kConsentFormErrorInternal;
    case UMPFormErrorCodeAlreadyUsed:
      return kConsentFormErrorAlreadyUsed;
    case UMPFormErrorCodeUnavailable:
      return kConsentFormErrorUnavailable;
    case UMPFormErrorCodeTimeout:
      return kConsentFormErrorTimeout;
    case UMPFormErrorCodeInvalidViewController:
      return kConsentFormErrorInvalidOperation;
    default:
      return kConsentFormErrorUnknown;
  }
}

}  // namespace

Future<void> ConsentInfoInternalIos::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnRequestConsentInfoUpdate,
                                  kConsentRequestErrorOperationInProgress,
                                  &handle, &in_progress)) {
    return in_progress;
  }

  UMPRequestParameters* ios_params = [[UMPRequestParameters alloc] init];
  ios_params.tagForUnderAgeOfConsent =
      params.tag_for_under_age_of_consent ? YES : NO;
  UMPDebugSettings* dbg = [[UMPDebugSettings alloc] init];
  bool has_dbg =
      params.debug_settings.debug_geography != kConsentDebugGeographyDisabled;
  switch (params.debug_settings.debug_geography) {
    case kConsentDebugGeographyEEA:
      dbg.geography = UMPDebugGeographyEEA;
      break;
    case kConsentDebugGeographyNonEEA:
      dbg.geography = UMPDebugGeographyNotEEA;
      break;
    case kConsentDebugGeographyRegulatedUSState:
    case kConsentDebugGeographyOther:
      dbg.geography = static_cast<UMPDebugGeography>(
          params.debug_settings.debug_geography);
      break;
    default:
      dbg.geography = UMPDebugGeographyDisabled;
      break;
  }
  if (!params.debug_settings.debug_device_ids.empty()) {
    dbg.testDeviceIdentifiers = firebase::util::StringVectorToNSMutableArray(
        params.debug_settings.debug_device_ids);
    has_dbg = true;
  }
  if (has_dbg) ios_params.debugSettings = dbg;

  unsigned int tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    tag = s_instance_tag;
  }
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != tag) return;
    }
    [UMPConsentInformation.sharedInstance
        requestConsentInfoUpdateWithParameters:ios_params
                             completionHandler:^(NSError* _Nullable error) {
                               MutexLock lock(s_instance_mutex);
                               if (s_instance && s_instance_tag == tag) {
                                 CompleteFuture(
                                     handle, CppRequestErrorFromIosError(error),
                                     error ? error.localizedDescription
                                                 .UTF8String
                                           : nullptr);
                               }
                             }];
  });
  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalIos::GetConsentStatus() {
  switch (UMPConsentInformation.sharedInstance.consentStatus) {
    case UMPConsentStatusNotRequired:
      return kConsentStatusNotRequired;
    case UMPConsentStatusRequired:
      return kConsentStatusRequired;
    case UMPConsentStatusObtained:
      return kConsentStatusObtained;
    default:
      return kConsentStatusUnknown;
  }
}

ConsentFormStatus ConsentInfoInternalIos::GetConsentFormStatus() {
  switch (UMPConsentInformation.sharedInstance.formStatus) {
    case UMPFormStatusAvailable:
      return kConsentFormStatusAvailable;
    case UMPFormStatusUnavailable:
      return kConsentFormStatusUnavailable;
    default:
      return kConsentFormStatusUnknown;
  }
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalIos::GetPrivacyOptionsRequirementStatus() {
  switch (UMPConsentInformation.sharedInstance
              .privacyOptionsRequirementStatus) {
    case UMPPrivacyOptionsRequirementStatusRequired:
      return kPrivacyOptionsRequirementStatusRequired;
    case UMPPrivacyOptionsRequirementStatusNotRequired:
      return kPrivacyOptionsRequirementStatusNotRequired;
    default:
      return kPrivacyOptionsRequirementStatusUnknown;
  }
}

Future<void> ConsentInfoInternalIos::LoadConsentForm() {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnLoadConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }
  unsigned int tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    loaded_form_ = nil;
    tag = s_instance_tag;
  }
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != tag) return;
    }
    [UMPConsentForm
        loadWithCompletionHandler:^(UMPConsentForm* _Nullable form,
                                    NSError* _Nullable error) {
          MutexLock lock(s_instance_mutex);
          if (s_instance && s_instance_tag == tag) {
            if (form) {
              loaded_form_ = form;
              CompleteFuture(handle, kConsentFormSuccess, "Success");
            } else {
              CompleteFuture(handle,
                             error ? CppFormErrorFromIosError(error)
                                   : kConsentFormErrorUnknown,
                             error ? error.localizedDescription.UTF8String
                                   : "An unknown error occurred.");
            }
          }
        }];
  });
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::ShowConsentForm(FormParent parent) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }
  if (!ValidateFormParent(parent)) {
    CompleteFuture(handle, kConsentFormErrorInvalidOperation,
                   "FormParent must be a UIViewController or nil.");
    return MakeFuture<void>(futures(), handle);
  }
  UMPConsentForm* form = nil;
  unsigned int tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    form = loaded_form_;
    loaded_form_ = nil;
    tag = s_instance_tag;
  }
  if (!form) {
    CompleteFuture(
        handle, kConsentFormErrorUnavailable,
        "You must call LoadConsentForm() prior to calling ShowConsentForm().");
    return MakeFuture<void>(futures(), handle);
  }
  UIViewController* vc = static_cast<UIViewController*>(parent);
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != tag) return;
    }
    [form presentFromViewController:vc
                  completionHandler:^(NSError* _Nullable error) {
                    MutexLock lock(s_instance_mutex);
                    if (s_instance && s_instance_tag == tag) {
                      CompleteFuture(
                          handle, CppFormErrorFromIosError(error),
                          error ? error.localizedDescription.UTF8String
                                : nullptr);
                    }
                  }];
  });
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(
          kConsentInfoFnLoadAndShowConsentFormIfRequired,
          kConsentFormErrorOperationInProgress, &handle, &in_progress)) {
    return in_progress;
  }
  if (!ValidateFormParent(parent)) {
    CompleteFuture(handle, kConsentFormErrorInvalidOperation,
                   "FormParent must be a UIViewController or nil.");
    return MakeFuture<void>(futures(), handle);
  }
  unsigned int tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    tag = s_instance_tag;
  }
  UIViewController* vc = static_cast<UIViewController*>(parent);
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != tag) return;
    }
    [UMPConsentForm
        loadAndPresentIfRequiredFromViewController:vc
                                 completionHandler:^(NSError* _Nullable error) {
                                   MutexLock lock(s_instance_mutex);
                                   if (s_instance && s_instance_tag == tag) {
                                     CompleteFuture(
                                         handle,
                                         CppFormErrorFromIosError(error),
                                         error ? error.localizedDescription
                                                     .UTF8String
                                               : nullptr);
                                   }
                                 }];
  });
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::ShowPrivacyOptionsForm(FormParent parent) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowPrivacyOptionsForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }
  if (!ValidateFormParent(parent)) {
    CompleteFuture(handle, kConsentFormErrorInvalidOperation,
                   "FormParent must be a UIViewController or nil.");
    return MakeFuture<void>(futures(), handle);
  }
  unsigned int tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    tag = s_instance_tag;
  }
  UIViewController* vc = static_cast<UIViewController*>(parent);
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != tag) return;
    }
    [UMPConsentForm
        presentPrivacyOptionsFormFromViewController:vc
                                  completionHandler:^(NSError* _Nullable error) {
                                    MutexLock lock(s_instance_mutex);
                                    if (s_instance && s_instance_tag == tag) {
                                      CompleteFuture(
                                          handle,
                                          CppFormErrorFromIosError(error),
                                          error ? error.localizedDescription
                                                      .UTF8String
                                                : nullptr);
                                    }
                                  }];
  });
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalIos::CanRequestAds() {
  return UMPConsentInformation.sharedInstance.canRequestAds == YES;
}

void ConsentInfoInternalIos::Reset() {
  {
    MutexLock lock(s_instance_mutex);
    loaded_form_ = nil;
  }
  [UMPConsentInformation.sharedInstance reset];
}

}  // namespace internal
}  // namespace ump
}  // namespace firebase
