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

#include "ump/src/ios/consent_info_internal_ios.h"

#import <UIKit/UIKit.h>

#include <cassert>
#include <utility>

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
  // Increment this with each created instance to ensure leftover callbacks from
  // a destroyed instance are ignored.
  s_instance_tag++;
}

ConsentInfoInternalIos::~ConsentInfoInternalIos() {
  MutexLock lock(s_instance_mutex);
  loaded_form_ = nil;
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

namespace {

bool ValidateFormParent(FormParent parent) {
  if (parent == nil) {
    return true;
  }
  id obj = parent;
  return [obj isKindOfClass:[UIViewController class]] == YES;
}

ConsentRequestError CppRequestErrorFromIosError(NSError* error) {
  if (!error) {
    return kConsentRequestSuccess;
  }
  if (error.domain && ![error.domain isEqualToString:UMPRequestErrorDomain]) {
    if ([error.domain isEqualToString:NSURLErrorDomain] &&
        error.code == NSURLErrorTimedOut) {
      return kConsentRequestErrorTimeout;
    }
    return kConsentRequestErrorUnknown;
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
      LogWarning("UMP: Unknown UMPRequestErrorCode returned by UMP iOS SDK: %d",
                 static_cast<int>(error.code));
      return kConsentRequestErrorUnknown;
  }
}

ConsentFormError CppFormErrorFromIosError(NSError* error) {
  if (!error) {
    return kConsentFormSuccess;
  }
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
      LogWarning("UMP: Unknown UMPFormErrorCode returned by UMP iOS SDK: %d",
                 static_cast<int>(error.code));
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

  UMPRequestParameters* ios_parameters = [[UMPRequestParameters alloc] init];
  ios_parameters.tagForUnderAgeOfConsent =
      params.tag_for_under_age_of_consent ? YES : NO;
  UMPDebugSettings* ios_debug_settings = [[UMPDebugSettings alloc] init];
  bool has_debug_settings = false;

  switch (params.debug_settings.debug_geography) {
    case kConsentDebugGeographyEEA:
      ios_debug_settings.geography = UMPDebugGeographyEEA;
      has_debug_settings = true;
      break;
    case kConsentDebugGeographyNonEEA:
      ios_debug_settings.geography = UMPDebugGeographyNotEEA;
      has_debug_settings = true;
      break;
    case kConsentDebugGeographyRegulatedUSState:
      ios_debug_settings.geography =
          static_cast<UMPDebugGeography>(kConsentDebugGeographyRegulatedUSState);
      has_debug_settings = true;
      break;
    case kConsentDebugGeographyOther:
      ios_debug_settings.geography =
          static_cast<UMPDebugGeography>(kConsentDebugGeographyOther);
      has_debug_settings = true;
      break;
    case kConsentDebugGeographyDisabled:
    default:
      ios_debug_settings.geography = UMPDebugGeographyDisabled;
      break;
  }
  if (!params.debug_settings.debug_device_ids.empty()) {
    ios_debug_settings.testDeviceIdentifiers =
        firebase::util::StringVectorToNSMutableArray(
            params.debug_settings.debug_device_ids);
    has_debug_settings = true;
  }
  if (has_debug_settings) {
    ios_parameters.debugSettings = ios_debug_settings;
  }

  unsigned int callback_instance_tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    callback_instance_tag = s_instance_tag;
  }

  // Do NOT hold s_instance_mutex across DispatchAsyncSafeMainQueue.
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != callback_instance_tag) {
        return;
      }
    }
    [UMPConsentInformation.sharedInstance
        requestConsentInfoUpdateWithParameters:ios_parameters
                             completionHandler:^(NSError* _Nullable error) {
                               MutexLock lock(s_instance_mutex);
                               if (s_instance &&
                                   s_instance_tag == callback_instance_tag) {
                                 if (!error) {
                                   CompleteFuture(handle,
                                                  kConsentRequestSuccess);
                                 } else {
                                   CompleteFuture(
                                       handle,
                                       CppRequestErrorFromIosError(error),
                                       error.localizedDescription.UTF8String);
                                 }
                               }
                             }];
  });

  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalIos::GetConsentStatus() {
  UMPConsentStatus ios_status =
      UMPConsentInformation.sharedInstance.consentStatus;
  switch (ios_status) {
    case UMPConsentStatusNotRequired:
      return kConsentStatusNotRequired;
    case UMPConsentStatusRequired:
      return kConsentStatusRequired;
    case UMPConsentStatusObtained:
      return kConsentStatusObtained;
    case UMPConsentStatusUnknown:
      return kConsentStatusUnknown;
    default:
      LogWarning("UMP: Unknown UMPConsentStatus returned by UMP iOS SDK: %d",
                 static_cast<int>(ios_status));
      return kConsentStatusUnknown;
  }
}

ConsentFormStatus ConsentInfoInternalIos::GetConsentFormStatus() {
  UMPFormStatus ios_status = UMPConsentInformation.sharedInstance.formStatus;
  switch (ios_status) {
    case UMPFormStatusAvailable:
      return kConsentFormStatusAvailable;
    case UMPFormStatusUnavailable:
      return kConsentFormStatusUnavailable;
    case UMPFormStatusUnknown:
      return kConsentFormStatusUnknown;
    default:
      LogWarning(
          "UMP: Unknown UMPFormStatus returned by UMP iOS SDK: %d",
          static_cast<int>(ios_status));
      return kConsentFormStatusUnknown;
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

  unsigned int callback_instance_tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    loaded_form_ = nil;
    callback_instance_tag = s_instance_tag;
  }

  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != callback_instance_tag) {
        return;
      }
    }
    [UMPConsentForm
        loadWithCompletionHandler:^(UMPConsentForm* _Nullable form,
                                    NSError* _Nullable error) {
          MutexLock lock(s_instance_mutex);
          if (s_instance && s_instance_tag == callback_instance_tag) {
            if (form) {
              loaded_form_ = form;
              CompleteFuture(handle, kConsentFormSuccess, "Success");
            } else if (error) {
              CompleteFuture(handle, CppFormErrorFromIosError(error),
                             error.localizedDescription.UTF8String);
            } else {
              CompleteFuture(handle, kConsentFormErrorUnknown,
                             "An unknown error occurred.");
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

  UMPConsentForm* form_to_present = nil;
  unsigned int callback_instance_tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    form_to_present = loaded_form_;
    loaded_form_ = nil;
    callback_instance_tag = s_instance_tag;
  }

  if (!form_to_present) {
    CompleteFuture(handle, kConsentFormErrorUnavailable,
                   "You must call LoadConsentForm() prior to calling "
                   "ShowConsentForm().");
    return MakeFuture<void>(futures(), handle);
  }

  UIViewController* view_controller = static_cast<UIViewController*>(parent);
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != callback_instance_tag) {
        return;
      }
    }
    [form_to_present
        presentFromViewController:view_controller
                completionHandler:^(NSError* _Nullable error) {
                  MutexLock lock(s_instance_mutex);
                  if (s_instance && s_instance_tag == callback_instance_tag) {
                    if (!error) {
                      CompleteFuture(handle, kConsentFormSuccess);
                    } else {
                      CompleteFuture(handle, CppFormErrorFromIosError(error),
                                     error.localizedDescription.UTF8String);
                    }
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

  unsigned int callback_instance_tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    callback_instance_tag = s_instance_tag;
  }

  UIViewController* view_controller = static_cast<UIViewController*>(parent);
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != callback_instance_tag) {
        return;
      }
    }
    [UMPConsentForm
        loadAndPresentIfRequiredFromViewController:view_controller
                                 completionHandler:^(NSError* _Nullable error) {
                                   MutexLock lock(s_instance_mutex);
                                   if (s_instance &&
                                       s_instance_tag == callback_instance_tag) {
                                     if (!error) {
                                       CompleteFuture(handle,
                                                      kConsentFormSuccess);
                                     } else {
                                       CompleteFuture(
                                           handle,
                                           CppFormErrorFromIosError(error),
                                           error.localizedDescription
                                               .UTF8String);
                                     }
                                   }
                                 }];
  });
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalIos::GetPrivacyOptionsRequirementStatus() {
  UMPPrivacyOptionsRequirementStatus ios_status =
      UMPConsentInformation.sharedInstance.privacyOptionsRequirementStatus;
  switch (ios_status) {
    case UMPPrivacyOptionsRequirementStatusRequired:
      return kPrivacyOptionsRequirementStatusRequired;
    case UMPPrivacyOptionsRequirementStatusNotRequired:
      return kPrivacyOptionsRequirementStatusNotRequired;
    case UMPPrivacyOptionsRequirementStatusUnknown:
      return kPrivacyOptionsRequirementStatusUnknown;
    default:
      LogWarning(
          "UMP: Unknown UMPPrivacyOptionsRequirementStatus returned by UMP "
          "iOS SDK: %d",
          static_cast<int>(ios_status));
      return kPrivacyOptionsRequirementStatusUnknown;
  }
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

  unsigned int callback_instance_tag = 0;
  {
    MutexLock lock(s_instance_mutex);
    callback_instance_tag = s_instance_tag;
  }

  UIViewController* view_controller = static_cast<UIViewController*>(parent);
  util::DispatchAsyncSafeMainQueue(^{
    {
      MutexLock lock(s_instance_mutex);
      if (!s_instance || s_instance_tag != callback_instance_tag) {
        return;
      }
    }
    [UMPConsentForm
        presentPrivacyOptionsFormFromViewController:view_controller
                                  completionHandler:^(NSError* _Nullable error) {
                                    MutexLock lock(s_instance_mutex);
                                    if (s_instance &&
                                        s_instance_tag ==
                                            callback_instance_tag) {
                                      if (!error) {
                                        CompleteFuture(handle,
                                                       kConsentFormSuccess);
                                      } else {
                                        CompleteFuture(
                                            handle,
                                            CppFormErrorFromIosError(error),
                                            error.localizedDescription
                                                .UTF8String);
                                      }
                                    }
                                  }];
  });
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalIos::CanRequestAds() {
  return (UMPConsentInformation.sharedInstance.canRequestAds == YES);
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
