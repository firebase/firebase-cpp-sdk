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

#include <cassert>
#include <memory>
#include <string>

#include "app/src/assert.h"
#include "app/src/thread.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace ump {
namespace internal {

// This explicitly implements the constructor for the outer class,
// ConsentInfoInternal.
ConsentInfoInternal* ConsentInfoInternal::CreateInstance() { return new ConsentInfoInternalIos(); }

ConsentInfoInternalIos::ConsentInfoInternalIos() : loaded_form_(nil) {}

ConsentInfoInternalIos::~ConsentInfoInternalIos() {}

static ConsentRequestError CppRequestErrorFromIosRequestError(NSInteger code) {
  switch (code) {
    case UMPRequestErrorCodeInternal:
      return kConsentRequestErrorInternal;
    case UMPRequestErrorCodeInvalidAppID:
      return kConsentRequestErrorInvalidAppId;
    case UMPRequestErrorCodeMisconfiguration:
      return kConsentRequestErrorMisconfiguration;
    case UMPRequestErrorCodeNetwork:
      return kConsentRequestErrorNetwork;
    default:
      LogWarning("UMP: Unknown UMPRequestErrorCode returned by UMP iOS SDK: %d", (int)code);
      return kConsentRequestErrorUnknown;
  }
}

static ConsentFormError CppFormErrorFromIosFormError(NSInteger code) {
  switch (code) {
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
      LogWarning("UMP: Unknown UMPFormErrorCode returned by UMP iOS SDK: %d", (int)code);
      return kConsentFormErrorUnknown;
  }
}

Future<void> ConsentInfoInternalIos::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<void> handle;
  {
    MutexLock lock(mutex_);
    if (RequestConsentInfoUpdateLastResult().status() == kFutureStatusPending) {
      // This operation is already in progress.
      // Return a future with an error - this will not override the Fn entry.
      SafeFutureHandle<void> error_handle = CreateFuture();
      CompleteFuture(error_handle, kConsentRequestErrorOperationInProgress);
      return MakeFuture<void>(futures(), error_handle);
    }
    handle = CreateFuture(kConsentInfoFnRequestConsentInfoUpdate);
  }

  UMPRequestParameters* ios_parameters = [[UMPRequestParameters alloc] init];
  ios_parameters.tagForUnderAgeOfConsent = params.tag_for_under_age_of_consent ? YES : NO;
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
    case kConsentDebugGeographyDisabled:
      ios_debug_settings.geography = UMPDebugGeographyDisabled;
      break;
  }
  if (params.debug_settings.debug_device_ids.size() > 0) {
    ios_debug_settings.testDeviceIdentifiers =
        firebase::util::StringVectorToNSMutableArray(params.debug_settings.debug_device_ids);
    has_debug_settings = true;
  }
  if (has_debug_settings) {
    ios_parameters.debugSettings = ios_debug_settings;
  }

  std::weak_ptr<ConsentInfoInternal> weak_self = weak_from_this();

  util::DispatchAsyncSafeMainQueue(^{
    auto self = std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
    if (!self) {
      return;
    }
    [UMPConsentInformation.sharedInstance
        requestConsentInfoUpdateWithParameters:ios_parameters
                             completionHandler:^(NSError* _Nullable error) {
                               auto strong_self =
                                   std::static_pointer_cast<ConsentInfoInternalIos>(
                                       weak_self.lock());
                               if (!strong_self) return;
                               auto core = strong_self->future_core();
                               ConsentRequestError code =
                                   error ? CppRequestErrorFromIosRequestError(error.code)
                                         : kConsentRequestSuccess;
                               std::string msg =
                                   error && error.localizedDescription
                                       ? std::string(error.localizedDescription.UTF8String)
                                       : std::string(GetConsentRequestErrorMessage(code));
                               strong_self.reset();
                               ReferenceCountedFutureImpl::CompleteCore(
                                   core, handle, static_cast<int>(code), msg.c_str());
                             }];
  });

  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalIos::GetConsentStatus() {
  UMPConsentStatus ios_status = UMPConsentInformation.sharedInstance.consentStatus;
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
      LogWarning("UMP: Unknown UMPConsentStatus returned by UMP iOS SDK: %d", (int)ios_status);
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
      LogWarning("UMP: Unknown UMPFormConsentStatus returned by UMP iOS SDK: %d", (int)ios_status);
      return kConsentFormStatusUnknown;
  }
}

Future<void> ConsentInfoInternalIos::LoadConsentForm() {
  SafeFutureHandle<void> handle;
  {
    MutexLock lock(mutex_);
    if (LoadConsentFormLastResult().status() == kFutureStatusPending) {
      // This operation is already in progress.
      // Return a future with an error - this will not override the Fn entry.
      SafeFutureHandle<void> error_handle = CreateFuture();
      CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
      return MakeFuture<void>(futures(), error_handle);
    }
    handle = CreateFuture(kConsentInfoFnLoadConsentForm);
    loaded_form_ = nil;
  }

  std::weak_ptr<ConsentInfoInternal> weak_self = weak_from_this();

  util::DispatchAsyncSafeMainQueue(^{
    auto self = std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
    if (!self) {
      return;
    }
    [UMPConsentForm
        loadWithCompletionHandler:^(UMPConsentForm* _Nullable form, NSError* _Nullable error) {
          auto strong_self =
              std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
          if (!strong_self) return;
          auto core = strong_self->future_core();
          ConsentFormError code = kConsentFormErrorUnknown;
          std::string msg;
          if (form) {
            {
              MutexLock lock(strong_self->mutex_);
              strong_self->SetLoadedForm(form);
            }
            code = kConsentFormSuccess;
            msg = "Success";
          } else if (error) {
            code = CppFormErrorFromIosFormError(error.code);
            msg = error.localizedDescription
                      ? std::string(error.localizedDescription.UTF8String)
                      : std::string(GetConsentFormErrorMessage(code));
          } else {
            code = kConsentFormErrorUnknown;
            msg = "An unknown error occurred.";
          }
          strong_self.reset();
          ReferenceCountedFutureImpl::CompleteCore(core, handle, static_cast<int>(code),
                                                   msg.c_str());
        }];
  });
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::ShowConsentForm(FormParent parent) {
  SafeFutureHandle<void> handle;
  UMPConsentForm* form_to_present = nil;
  {
    MutexLock lock(mutex_);
    if (ShowConsentFormLastResult().status() == kFutureStatusPending) {
      // This operation is already in progress.
      // Return a future with an error - this will not override the Fn entry.
      SafeFutureHandle<void> error_handle = CreateFuture();
      CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
      return MakeFuture<void>(futures(), error_handle);
    }
    handle = CreateFuture(kConsentInfoFnShowConsentForm);
    form_to_present = loaded_form_;
  }

  if (!form_to_present) {
    CompleteFuture(handle, kConsentFormErrorInvalidOperation,
                   "You must call LoadConsentForm() prior to calling ShowConsentForm().");
  } else {
    std::weak_ptr<ConsentInfoInternal> weak_self = weak_from_this();

    util::DispatchAsyncSafeMainQueue(^{
      auto self = std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
      if (!self) {
        return;
      }
      [form_to_present
          presentFromViewController:parent
                  completionHandler:^(NSError* _Nullable error) {
                    auto strong_self =
                        std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
                    if (!strong_self) return;
                    auto core = strong_self->future_core();
                    int code = 0;
                    std::string msg;
                    if (!error) {
                      code = static_cast<int>(kConsentRequestSuccess);
                      msg = GetConsentRequestErrorMessage(kConsentRequestSuccess);
                    } else {
                      ConsentFormError form_err = CppFormErrorFromIosFormError(error.code);
                      code = static_cast<int>(form_err);
                      msg = error.localizedDescription
                                ? std::string(error.localizedDescription.UTF8String)
                                : std::string(GetConsentFormErrorMessage(form_err));
                    }
                    strong_self.reset();
                    ReferenceCountedFutureImpl::CompleteCore(core, handle, code, msg.c_str());
                  }];
    });
  }
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::LoadAndShowConsentFormIfRequired(FormParent parent) {
  SafeFutureHandle<void> handle;
  {
    MutexLock lock(mutex_);
    if (LoadAndShowConsentFormIfRequiredLastResult().status() == kFutureStatusPending) {
      // This operation is already in progress.
      // Return a future with an error - this will not override the Fn entry.
      SafeFutureHandle<void> error_handle = CreateFuture();
      CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
      return MakeFuture<void>(futures(), error_handle);
    }
    handle = CreateFuture(kConsentInfoFnLoadAndShowConsentFormIfRequired);
  }

  std::weak_ptr<ConsentInfoInternal> weak_self = weak_from_this();

  util::DispatchAsyncSafeMainQueue(^{
    auto self = std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
    if (!self) {
      return;
    }
    [UMPConsentForm
        loadAndPresentIfRequiredFromViewController:parent
                                 completionHandler:^(NSError* _Nullable error) {
                                   auto strong_self =
                                       std::static_pointer_cast<ConsentInfoInternalIos>(
                                           weak_self.lock());
                                   if (!strong_self) return;
                                   auto core = strong_self->future_core();
                                   int code = 0;
                                   std::string msg;
                                   if (!error) {
                                     code = static_cast<int>(kConsentRequestSuccess);
                                     msg = GetConsentRequestErrorMessage(kConsentRequestSuccess);
                                   } else {
                                     ConsentFormError form_err =
                                         CppFormErrorFromIosFormError(error.code);
                                     code = static_cast<int>(form_err);
                                     msg = error.localizedDescription
                                               ? std::string(error.localizedDescription.UTF8String)
                                               : std::string(GetConsentFormErrorMessage(form_err));
                                   }
                                   strong_self.reset();
                                   ReferenceCountedFutureImpl::CompleteCore(core, handle, code,
                                                                            msg.c_str());
                                 }];
  });
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus ConsentInfoInternalIos::GetPrivacyOptionsRequirementStatus() {
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
      LogWarning("UMP: Unknown UMPPrivacyOptionsRequirementStatus returned by UMP iOS SDK: %d",
                 (int)ios_status);
      return kPrivacyOptionsRequirementStatusUnknown;
  }
}

Future<void> ConsentInfoInternalIos::ShowPrivacyOptionsForm(FormParent parent) {
  SafeFutureHandle<void> handle;
  {
    MutexLock lock(mutex_);
    if (ShowPrivacyOptionsFormLastResult().status() == kFutureStatusPending) {
      // This operation is already in progress.
      // Return a future with an error - this will not override the Fn entry.
      SafeFutureHandle<void> error_handle = CreateFuture();
      CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
      return MakeFuture<void>(futures(), error_handle);
    }
    handle = CreateFuture(kConsentInfoFnShowPrivacyOptionsForm);
  }

  std::weak_ptr<ConsentInfoInternal> weak_self = weak_from_this();

  util::DispatchAsyncSafeMainQueue(^{
    auto self = std::static_pointer_cast<ConsentInfoInternalIos>(weak_self.lock());
    if (!self) {
      return;
    }
    [UMPConsentForm
        presentPrivacyOptionsFormFromViewController:parent
                                  completionHandler:^(NSError* _Nullable error) {
                                    auto strong_self =
                                        std::static_pointer_cast<ConsentInfoInternalIos>(
                                            weak_self.lock());
                                    if (!strong_self) return;
                                    auto core = strong_self->future_core();
                                    int code = 0;
                                    std::string msg;
                                    if (!error) {
                                      code = static_cast<int>(kConsentRequestSuccess);
                                      msg = GetConsentRequestErrorMessage(kConsentRequestSuccess);
                                    } else {
                                      ConsentFormError form_err =
                                          CppFormErrorFromIosFormError(error.code);
                                      code = static_cast<int>(form_err);
                                      msg = error.localizedDescription
                                                ? std::string(error.localizedDescription.UTF8String)
                                                : std::string(GetConsentFormErrorMessage(form_err));
                                    }
                                    strong_self.reset();
                                    ReferenceCountedFutureImpl::CompleteCore(core, handle, code,
                                                                             msg.c_str());
                                  }];
  });
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalIos::CanRequestAds() {
  return (UMPConsentInformation.sharedInstance.canRequestAds == YES ? true : false);
}

void ConsentInfoInternalIos::Reset() { [UMPConsentInformation.sharedInstance reset]; }

}  // namespace internal
}  // namespace ump
}  // namespace firebase
