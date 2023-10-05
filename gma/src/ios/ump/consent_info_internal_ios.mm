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

#include "gma/src/ios/ump/consent_info_internal_ios.h"

#include "app/src/assert.h"
#include "app/src/thread.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

ConsentInfoInternalIos* ConsentInfoInternalIos::s_instance = nullptr;
firebase::Mutex ConsentInfoInternalIos::s_instance_mutex;


// This explicitly implements the constructor for the outer class,
// ConsentInfoInternal.
ConsentInfoInternal* ConsentInfoInternal::CreateInstance() {
  return new ConsentInfoInternalIos();
}

ConsentInfoInternalIos::ConsentInfoInternalIos()
  : loaded_form_(nil) {
  MutexLock lock(s_instance_mutex);
  FIREBASE_ASSERT(s_instance == nullptr);
  s_instance = this;
}

ConsentInfoInternalIos::~ConsentInfoInternalIos() {
  MutexLock lock(s_instance_mutex);
  s_instance = nullptr;
}

static ConsentRequestError CppRequestErrorFromIosRequestError(NSInteger code) {
  switch(code) {
  case UMPRequestErrorCodeInternal:
    return kConsentRequestErrorInternal;
  case UMPRequestErrorCodeInvalidAppID:
    return kConsentRequestErrorInvalidAppId;
  case UMPRequestErrorCodeMisconfiguration:
    return kConsentRequestErrorMisconfiguration;
  case UMPRequestErrorCodeNetwork:
    return kConsentRequestErrorNetwork;
  default:
    LogWarning("GMA: Unknown UMPRequestErrorCode returned by UMP iOS SDK: %d",
	       (int)code);
    return kConsentRequestErrorUnknown;
  }
}

static ConsentFormError CppFormErrorFromIosFormError(NSInteger code) {
  switch(code) {
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
    LogWarning("GMA: Unknown UMPFormErrorCode returned by UMP iOS SDK: %d",
	       (int)code);
    return kConsentFormErrorUnknown;
  }
}

Future<void> ConsentInfoInternalIos::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  if (RequestConsentInfoUpdateLastResult().status() == kFutureStatusPending) {
    // This operation is already in progress.
    // Return a future with an error - this will not override the Fn entry.
    LogInfo("UMP C++: RequestConsentInfoUpdate returning operation in progress error");
    SafeFutureHandle<void> error_handle = CreateFuture(kConsentInfoFnOperationInProgress);
    CompleteFuture(error_handle, kConsentRequestErrorOperationInProgress);
    return MakeFuture<void>(futures(), error_handle);
  }

  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnRequestConsentInfoUpdate);

  UMPRequestParameters *ios_parameters = [[UMPRequestParameters alloc] init];
  ios_parameters.tagForUnderAgeOfConsent = params.tag_for_under_age_of_consent ? YES : NO;
  UMPDebugSettings *ios_debug_settings = [[UMPDebugSettings alloc] init];
  bool has_debug_settings = false;

  switch(params.debug_settings.debug_geography) {
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

  LogInfo("UMP C++: RequestConsentInfoUpdate dispatching");
  util::DispatchAsyncSafeMainQueue(^{
      LogInfo("UMP C++: RequestConsentInfoUpdate calling iOS method in main thread");
      [UMPConsentInformation.sharedInstance
	  requestConsentInfoUpdateWithParameters:ios_parameters
	  completionHandler:^(NSError *_Nullable error){
	  LogInfo("UMP C++: RequestConsentInfoUpdate completionHandler start");
	  if (!error) {
            MutexLock lock(s_instance_mutex);
            if (s_instance) {
              CompleteFuture(handle, kConsentRequestSuccess);
            }
	  } else {
            MutexLock lock(s_instance_mutex);
            if (s_instance) {
              CompleteFuture(handle, CppRequestErrorFromIosRequestError(error.code), error.localizedDescription.UTF8String);
            }
	  }
	  LogInfo("UMP C++: RequestConsentInfoUpdate completionHandler end");
	}];
    });

  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalIos::GetConsentStatus() {
  UMPConsentStatus ios_status =
    UMPConsentInformation.sharedInstance.consentStatus;
  switch(ios_status) {
  case UMPConsentStatusNotRequired:
    return kConsentStatusNotRequired;
  case UMPConsentStatusRequired:
    return kConsentStatusRequired;
  case UMPConsentStatusObtained:
    return kConsentStatusObtained;
  case UMPConsentStatusUnknown:
    return kConsentStatusUnknown;
  default:
    LogWarning("GMA: Unknown UMPConsentStatus returned by UMP iOS SDK: %d",
	       (int)ios_status);
    return kConsentStatusUnknown;
  }
}


ConsentFormStatus ConsentInfoInternalIos::GetConsentFormStatus() {
  UMPFormStatus ios_status =
    UMPConsentInformation.sharedInstance.formStatus;
  switch(ios_status) {
  case UMPFormStatusAvailable:
    return kConsentFormStatusAvailable;
  case UMPFormStatusUnavailable:
    return kConsentFormStatusUnavailable;
  case UMPFormStatusUnknown:
    return kConsentFormStatusUnknown;
  default:
    LogWarning("GMA: Unknown UMPFormConsentStatus returned by UMP iOS SDK: %d",
	       (int)ios_status);
    return kConsentFormStatusUnknown;
  }
}

Future<void> ConsentInfoInternalIos::LoadConsentForm() {
  if (LoadConsentFormLastResult().status() == kFutureStatusPending) {
    // This operation is already in progress.
    // Return a future with an error - this will not override the Fn entry.
    SafeFutureHandle<void> error_handle = CreateFuture(kConsentInfoFnOperationInProgress);
    CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
    return MakeFuture<void>(futures(), error_handle);
  }

  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnLoadConsentForm);
  loaded_form_ = nil;

  LogInfo("UMP C++: LoadConsentForm dispatching");
  util::DispatchAsyncSafeMainQueue(^{
      LogInfo("UMP C++: LoadConsentForm calling iOS method in main thread");
	[UMPConsentForm
	  loadWithCompletionHandler:^(UMPConsentForm *_Nullable form, NSError *_Nullable error){
	    LogInfo("UMP C++: LoadConsentForm completionHandler start");
	    if (form) {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		SetLoadedForm(form);
		CompleteFuture(handle, kConsentFormSuccess, "Success");
	      }
	    } else if (error) {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, CppFormErrorFromIosFormError(error.code), error.localizedDescription.UTF8String);
	      }
	    } else {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, kConsentFormErrorUnknown, "An unknown error occurred.");
	      }
	    }
	    LogInfo("UMP C++: LoadConsentForm completionHandler end");
	  }];
      });
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::ShowConsentForm(FormParent parent) {
  if (ShowConsentFormLastResult().status() == kFutureStatusPending) {
    // This operation is already in progress.
    // Return a future with an error - this will not override the Fn entry.
    SafeFutureHandle<void> error_handle = CreateFuture(kConsentInfoFnOperationInProgress);
    CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
    return MakeFuture<void>(futures(), error_handle);
  }

  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnShowConsentForm);

  if (!loaded_form_) {
    CompleteFuture(handle, kConsentFormErrorInvalidOperation,
		   "You must call LoadConsentForm() prior to calling ShowConsentForm().");
  } else {
    util::DispatchAsyncSafeMainQueue(^{
	[loaded_form_ presentFromViewController:parent
			     completionHandler:^(NSError *_Nullable error){
	    LogInfo("UMP C++: ShowConsentForm completionHandler start");
	    if (!error) {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, kConsentRequestSuccess);
	      }
	    } else {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, CppFormErrorFromIosFormError(error.code), error.localizedDescription.UTF8String);
	      }
	    }
	    LogInfo("UMP C++: ShowConsentForm completionHandler end");
	  }];
      });
  }
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalIos::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  if (LoadAndShowConsentFormIfRequiredLastResult().status() ==
      kFutureStatusPending) {
    // This operation is already in progress.
    // Return a future with an error - this will not override the Fn entry.
    SafeFutureHandle<void> error_handle = CreateFuture(kConsentInfoFnOperationInProgress);
    CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
    return MakeFuture<void>(futures(), error_handle);
  }

  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnLoadAndShowConsentFormIfRequired);

  util::DispatchAsyncSafeMainQueue(^{
      [UMPConsentForm loadAndPresentIfRequiredFromViewController:parent
					       completionHandler:^(NSError *_Nullable error){
	  LogInfo("UMP C++: LoadAndShowConsentFormIfRequired completionHandler start");
	  if (!error) {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, kConsentRequestSuccess);
	      }
	  } else {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, CppFormErrorFromIosFormError(error.code), error.localizedDescription.UTF8String);
	      }
	  }
	  LogInfo("UMP C++: LoadAndShowConsentFormIfRequired completionHandler end");
	}];
    });
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalIos::GetPrivacyOptionsRequirementStatus() {
  UMPPrivacyOptionsRequirementStatus ios_status =
    UMPConsentInformation.sharedInstance.privacyOptionsRequirementStatus;
  switch(ios_status) {
  case UMPPrivacyOptionsRequirementStatusRequired:
    return kPrivacyOptionsRequirementStatusRequired;
  case UMPPrivacyOptionsRequirementStatusNotRequired:
    return kPrivacyOptionsRequirementStatusNotRequired;
  case UMPPrivacyOptionsRequirementStatusUnknown:
    return kPrivacyOptionsRequirementStatusUnknown;
  default:
    LogWarning("GMA: Unknown UMPPrivacyOptionsRequirementStatus returned by UMP iOS SDK: %d",
	       (int)ios_status);
    return kPrivacyOptionsRequirementStatusUnknown;
  }
}

Future<void> ConsentInfoInternalIos::ShowPrivacyOptionsForm(FormParent parent) {
  if (ShowPrivacyOptionsFormLastResult().status() == kFutureStatusPending) {
    // This operation is already in progress.
    // Return a future with an error - this will not override the Fn entry.
    SafeFutureHandle<void> error_handle = CreateFuture(kConsentInfoFnOperationInProgress);
    CompleteFuture(error_handle, kConsentFormErrorOperationInProgress);
    return MakeFuture<void>(futures(), error_handle);
  }

  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnShowPrivacyOptionsForm);
  util::DispatchAsyncSafeMainQueue(^{
      [UMPConsentForm presentPrivacyOptionsFormFromViewController:parent
						completionHandler:^(NSError *_Nullable error){
	  LogInfo("UMP C++: ShowPrivacyOptionsForm completionHandler start");
	  if (!error) {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, kConsentRequestSuccess);
	      }
	  } else {
	      MutexLock lock(s_instance_mutex);
	      if (s_instance) {
		CompleteFuture(handle, CppFormErrorFromIosFormError(error.code), error.localizedDescription.UTF8String);
	      }
	  }
	  LogInfo("UMP C++: ShowPrivacyOptionsForm completionHandler end");
	}];
    });
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalIos::CanRequestAds() {
  return (UMPConsentInformation.sharedInstance.canRequestAds == YES ? true : false);
}

void ConsentInfoInternalIos::Reset() {
  [UMPConsentInformation.sharedInstance reset];
}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
