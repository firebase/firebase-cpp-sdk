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

#import <GoogleMobileAds/GADRequestConfiguration.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#import "admob/src/ios/FADRequest.h"

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/types.h"
#include "admob/src/ios/ad_result_ios.h"
#include "admob/src/ios/adapter_response_info_ios.h"
#include "admob/src/ios/load_ad_result_ios.h"
#include "admob/src/ios/response_info_ios.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace admob {

static const ::firebase::App* g_app = nullptr;

static bool g_initialized = false;

InitResult Initialize() {
  g_initialized = true;
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

InitResult Initialize(const ::firebase::App& app) { 
   if (g_initialized) {
    LogWarning("AdMob is already initialized.");
    return kInitResultSuccess;
  }
  g_initialized = true;
  g_app = &app;
  RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
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

void Terminate() {
  UnregisterTerminateOnDefaultAppDestroy();
  DestroyCleanupNotifier();
  g_initialized = false;
  g_app = nullptr;
}

const ::firebase::App* GetApp() { return g_app; }

// Constructs AdResult objects based on the encountered result, beit a
// successful result, and C++ SDK Wrapper error, or an error returned from
// the iOS Admob SDK.
void CompleteLoadAdResult(FutureCallbackData<LoadAdResult>* callback_data,
                          NSError *error, AdMobError error_code,
                          const char* error_message) {
  FIREBASE_ASSERT(callback_data);
  FIREBASE_ASSERT(error_message);

  std::string future_error_message;
  LoadAdResultInternal load_ad_result_internal;
  AdResultInternal* adr = &(load_ad_result_internal.ad_result_internal);

  adr->ios_error = error;
  adr->is_successful = true;  // assume until proven otherwise.
  adr->is_wrapper_error = false;
  adr->code = error_code;

  // Futher result configuration is based on success/failure.
  if (error != nullptr) {
    // The iOS SDK returned an error.  The NSError object
    // will be used by the LoadAdError implementation to populate
    // it's fields.
    adr->is_successful = false;
  } else if (adr->code != kAdMobErrorNone) {
    // C++ SDK iOS AdMob Wrapper encountered an error.
    adr->is_wrapper_error = true;
    adr->is_successful = false;
    adr->message = std::string(error_message);
    adr->domain = "SDK";
    adr->to_string = std::string("Internal error: ") + adr->message;
    future_error_message = adr->message;
  }

  AdMobInternal::CompleteLoadAdFuture(
      callback_data, adr->code, future_error_message, load_ad_result_internal);
}

void CompleteLoadAdInternalResult(
    FutureCallbackData<LoadAdResult>* callback_data,
    AdMobError error_code, const char* error_message) {
  FIREBASE_ASSERT(callback_data);
  FIREBASE_ASSERT(error_message);

  CompleteLoadAdResult(callback_data, /*error=*/nullptr, error_code, error_message);
}

void CompleteLoadAdIOSResult(FutureCallbackData<LoadAdResult>* callback_data,
                             NSError *gad_error) {
  FIREBASE_ASSERT(callback_data);

  AdMobError error_code = MapADErrorCodeToCPPErrorCode((GADErrorCode)gad_error.code);
  CompleteLoadAdResult(callback_data, gad_error, error_code,
                       /*error_message=*/"");
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
