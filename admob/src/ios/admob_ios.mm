/*
 * Copyright 2016 Google LLC
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

#import "admob/src/ios/admob_ios.h"

#include "admob/src/include/firebase/admob.h"

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GADRequestConfiguration.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#import "admob/src/ios/FADRequest.h"

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/types.h"
#include "admob/src/ios/ad_result_ios.h"
#include "admob/src/ios/adapter_response_info_ios.h"
#include "admob/src/ios/response_info_ios.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace admob {

static const ::firebase::App* g_app = nullptr;

static bool g_initialized = false;

// Constants representing each AdMob function that returns a Future.
enum AdMobFn {
  kAdMobFnInitialize,
  kAdMobFnCount,
};

static ReferenceCountedFutureImpl* g_future_impl = nullptr;
// Mutex for creation/deletion of a g_future_impl.
static Mutex g_future_impl_mutex;  // NOLINT

// Take an iOS GADInitializationStatus object and populate our own
// AdapterInitializationStatus class.
static AdapterInitializationStatus PopulateAdapterInitializationStatus(
  GADInitializationStatus *init_status) {
  if (!init_status) return AdMobInternal::CreateAdapterInitializationStatus({});

  std::map<std::string, AdapterStatus> adapter_status_map;

  NSDictionary<NSString *, GADAdapterStatus*> *status_dict = init_status.adapterStatusesByClassName;
  for (NSString* key in status_dict) {
    GADAdapterStatus* status = [status_dict objectForKey:key];
    AdapterStatus adapter_status =
      AdMobInternal::CreateAdapterStatus(util::NSStringToString(status.description),
                                         status.state == GADAdapterInitializationStateReady,
                                         status.latency);
    adapter_status_map[util::NSStringToString(key)] = adapter_status;
  }
  return AdMobInternal::CreateAdapterInitializationStatus(adapter_status_map);
}
  
static Future<AdapterInitializationStatus> InitializeAdMob() {
  MutexLock lock(g_future_impl_mutex);
  FIREBASE_ASSERT(g_future_impl);

  SafeFutureHandle<AdapterInitializationStatus> handle =
    g_future_impl->SafeAlloc<AdapterInitializationStatus>(kAdMobFnInitialize);
  [GADMobileAds.sharedInstance startWithCompletionHandler:^(GADInitializationStatus *_Nonnull status){
      // GAD initialization has completed, with adapter initialization status.
      AdapterInitializationStatus adapter_status = PopulateAdapterInitializationStatus(status);
      {
        MutexLock lock(g_future_impl_mutex);
        // Check if g_future_impl still exists; if not, Terminate() was called, ignore the
        // result of this callback.
        if (g_future_impl) {
          g_future_impl->CompleteWithResult(handle, 0, "", adapter_status);
        }
      }
    }];
  return MakeFuture(g_future_impl, handle);
}
 
Future<AdapterInitializationStatus> Initialize(InitResult *init_result_out) {
  FIREBASE_ASSERT(!g_initialized);
  g_initialized = true;

  {
    MutexLock lock(g_future_impl_mutex);
    g_future_impl = new ReferenceCountedFutureImpl(kAdMobFnCount);
  }

  RegisterTerminateOnDefaultAppDestroy();
  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return InitializeAdMob();
}

Future<AdapterInitializationStatus> Initialize(const ::firebase::App& app, InitResult *init_result_out) { 
  FIREBASE_ASSERT(!g_initialized);
  g_initialized = true;
  {
    MutexLock lock(g_future_impl_mutex);
    g_future_impl = new ReferenceCountedFutureImpl(kAdMobFnCount);
  }
  g_app = &app;
  RegisterTerminateOnDefaultAppDestroy();
  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return InitializeAdMob();
}

bool IsInitialized() { return g_initialized; }

void SetRequestConfiguration(const RequestConfiguration& request_configuration)
{
  GADMobileAds.sharedInstance.requestConfiguration.testDeviceIdentifiers =
    util::StringVectorToNSMutableArray(request_configuration.test_device_ids);

  switch(request_configuration.max_ad_content_rating) {
    case RequestConfiguration::kMaxAdContentRatingG:
      GADMobileAds.sharedInstance.requestConfiguration.maxAdContentRating =
        GADMaxAdContentRatingGeneral;
      break;
    case RequestConfiguration::kMaxAdContentRatingPG:
      GADMobileAds.sharedInstance.requestConfiguration.maxAdContentRating =
        GADMaxAdContentRatingParentalGuidance;
      break;
    case RequestConfiguration::kMaxAdContentRatingT:
      GADMobileAds.sharedInstance.requestConfiguration.maxAdContentRating =
        GADMaxAdContentRatingTeen;
      break;
    case RequestConfiguration::kMaxAdContentRatingMA:
      GADMobileAds.sharedInstance.requestConfiguration.maxAdContentRating =
        GADMaxAdContentRatingMatureAudience;
      break;
    case RequestConfiguration::kMaxAdContentRatingUnspecified:
    default:
      // Do not set this value, which has significance in the iOS SDK.
      break;
  }
  
  switch(request_configuration.tag_for_child_directed_treatment) {
    case RequestConfiguration::kChildDirectedTreatmentFalse:
      [GADMobileAds.sharedInstance.requestConfiguration 
        tagForChildDirectedTreatment:NO];
      break;
    case RequestConfiguration::kChildDirectedTreatmentTrue:
      [GADMobileAds.sharedInstance.requestConfiguration
        tagForChildDirectedTreatment:YES];
      break;
    default:
    case RequestConfiguration::kChildDirectedTreatmentUnspecified:
      // Do not set this value, which has significance in the iOS SDK.
      break;
  }

  switch(request_configuration.tag_for_under_age_of_consent) {
    case RequestConfiguration::kUnderAgeOfConsentFalse:
      [GADMobileAds.sharedInstance.requestConfiguration
        tagForUnderAgeOfConsent:NO];
      break;
    case RequestConfiguration::kUnderAgeOfConsentTrue:
      [GADMobileAds.sharedInstance.requestConfiguration
        tagForUnderAgeOfConsent:YES];
      break;
    default:
    case RequestConfiguration::kUnderAgeOfConsentUnspecified:
      // Do not set this value, which has significance in the iOS SDK.
      break;
  }
}

RequestConfiguration GetRequestConfiguration() {
  RequestConfiguration request_configuration;

  GADMaxAdContentRating content_rating =
    GADMobileAds.sharedInstance.requestConfiguration.maxAdContentRating;
  if(content_rating == GADMaxAdContentRatingGeneral) {
    request_configuration.max_ad_content_rating =
      RequestConfiguration::kMaxAdContentRatingG;
  }
  else if(content_rating == GADMaxAdContentRatingParentalGuidance) {
    request_configuration.max_ad_content_rating =
      RequestConfiguration::kMaxAdContentRatingPG;
  }
  else if(content_rating == GADMaxAdContentRatingTeen) {
    request_configuration.max_ad_content_rating =
      RequestConfiguration::kMaxAdContentRatingT;
  }
  else if(content_rating == GADMaxAdContentRatingMatureAudience) {
    request_configuration.max_ad_content_rating =
      RequestConfiguration::kMaxAdContentRatingMA;
  } else {
    request_configuration.max_ad_content_rating =
      RequestConfiguration::kMaxAdContentRatingUnspecified;
  }

  request_configuration.tag_for_under_age_of_consent =
    RequestConfiguration::kUnderAgeOfConsentUnspecified;
  request_configuration.tag_for_child_directed_treatment =
    RequestConfiguration::kChildDirectedTreatmentUnspecified;

  util::NSArrayOfNSStringToVectorOfString(
    GADMobileAds.sharedInstance.requestConfiguration.testDeviceIdentifiers,
    &request_configuration.test_device_ids);

  return request_configuration;
}

Future<AdapterInitializationStatus> InitializeLastResult() {
  MutexLock lock(g_future_impl_mutex);
  return g_future_impl ? static_cast<const Future<AdapterInitializationStatus>&>(
                            g_future_impl->LastResult(kAdMobFnInitialize))
                      : Future<AdapterInitializationStatus>();
}

AdapterInitializationStatus GetInitializationStatus() {
  if (g_initialized) {
    GADInitializationStatus* status = GADMobileAds.sharedInstance.initializationStatus;
    return PopulateAdapterInitializationStatus(status);
  }
  else {
    // Returns an empty map.
    return PopulateAdapterInitializationStatus(nil);
  }
}

void Terminate() {
  FIREBASE_ASSERT(g_initialized);

  {
    MutexLock lock(g_future_impl_mutex);
    delete g_future_impl;
    g_future_impl = nullptr;
  }

  UnregisterTerminateOnDefaultAppDestroy();
  DestroyCleanupNotifier();
  g_initialized = false;
  g_app = nullptr;
}

const ::firebase::App* GetApp() { return g_app; }

// Constructs AdResult objects based on the encountered result, beit a
// successful result, and C++ SDK Wrapper error, or an error returned from
// the iOS Admob SDK.
void CompleteAdResult(FutureCallbackData<AdResult>* callback_data,
                      NSError *error, bool is_load_ad_result,
                      AdMobError error_code,
                      const char* error_message) {
  FIREBASE_ASSERT(callback_data);
  FIREBASE_ASSERT(error_message);

  std::string future_error_message;
  AdResultInternal ad_result_internal;

  ad_result_internal.ios_error = error;
  ad_result_internal.is_load_ad_error = is_load_ad_result;
  ad_result_internal.is_successful = true;  // assume until proven otherwise.
  ad_result_internal.is_wrapper_error = false;
  ad_result_internal.code = error_code;

  // Futher result configuration is based on success/failure.
  if (error != nullptr) {
    // The iOS SDK returned an error.  The NSError object
    // will be used by the AdError implementation to populate
    // it's fields.
    ad_result_internal.is_successful = false;
  } else if (ad_result_internal.code != kAdMobErrorNone) {
    // C++ SDK iOS AdMob Wrapper encountered an error.
    ad_result_internal.is_wrapper_error = true;
    ad_result_internal.is_successful = false;
    ad_result_internal.message = std::string(error_message);
    ad_result_internal.domain = "SDK";
    ad_result_internal.to_string = std::string("Internal error: ") +
      ad_result_internal.message;
    future_error_message = ad_result_internal.message;
  }

  AdMobInternal::CompleteLoadAdFuture(
      callback_data, ad_result_internal.code, future_error_message, ad_result_internal);
}

void CompleteLoadAdInternalResult(
    FutureCallbackData<AdResult>* callback_data,
    AdMobError error_code, const char* error_message) {
  FIREBASE_ASSERT(callback_data);
  FIREBASE_ASSERT(error_message);

  CompleteAdResult(callback_data, /*error=*/nullptr,
    /*is_load_ad_result=*/true, error_code, error_message);
}

void CompleteAdResultIOS(FutureCallbackData<AdResult>* callback_data,
                        NSError *gad_error) {
  FIREBASE_ASSERT(callback_data);

  AdMobError error_code = MapADErrorCodeToCPPErrorCode((GADErrorCode)gad_error.code);
  CompleteAdResult(callback_data, gad_error, /*is_load_ad_result=*/false,
    error_code, util::NSStringToString(gad_error.localizedDescription).c_str());
}

AdValue ConvertGADAdValueToCppAdValue(GADAdValue* gad_value) {
  FIREBASE_ASSERT(gad_value);

  AdValue::PrecisionType precision_type;
  switch(gad_value.precision) {
    default:
    case GADAdValuePrecisionUnknown:
      precision_type = AdValue::kdValuePrecisionUnknown;
      break;
    case GADAdValuePrecisionEstimated:
      precision_type = AdValue::kAdValuePrecisionEstimated;
      break;
    case GADAdValuePrecisionPublisherProvided:
      precision_type = AdValue::kAdValuePrecisionPublisherProvided;
      break;
    case GADAdValuePrecisionPrecise:
      precision_type = AdValue::kAdValuePrecisionPrecise;
      break;
  }

  return AdValue(util::NSStringToString(gad_value.currencyCode).c_str(),
    precision_type, (int64_t)gad_value.value.longLongValue);

}

}  // namespace admob
}  // namespace firebase
