/*
 * Copyright 2024 Google LLC
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

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include "gma/src/ios/query_info_internal_ios.h"

#import "gma/src/ios/FADRequest.h"
#import "gma/src/ios/gma_ios.h"

#include "app/src/util_ios.h"
#include "gma/src/ios/response_info_ios.h"

namespace firebase {
namespace gma {
namespace internal {

QueryInfoInternalIOS::QueryInfoInternalIOS(QueryInfo *base)
    : QueryInfoInternal(base),
      initialized_(false),
      query_info_callback_data_(nil),
      parent_view_(nil) {}

QueryInfoInternalIOS::~QueryInfoInternalIOS() {
  firebase::MutexLock lock(mutex_);
  if (query_info_callback_data_ != nil) {
    delete query_info_callback_data_;
    query_info_callback_data_ = nil;
  }
}

Future<void> QueryInfoInternalIOS::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kQueryInfoFnInitialize);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  if (initialized_) {
    CompleteFuture(kAdErrorCodeAlreadyInitialized, kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
  } else {
    initialized_ = true;
    parent_view_ = (UIView *)parent;
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
  }
  return future;
}

Future<QueryInfoResult> QueryInfoInternalIOS::CreateQueryInfo(AdFormat format,
                                                              const AdRequest &request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<QueryInfoResult> *callback_data =
      CreateQueryInfoResultFutureCallbackData(kQueryInfoFnCreateQueryInfo, &future_data_);
  Future<QueryInfoResult> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  if (query_info_callback_data_ != nil) {
    CompleteQueryInfoInternalError(callback_data, kAdErrorCodeLoadInProgress,
                                   kAdLoadInProgressErrorMessage);
    return future;
  }

  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the QueryInfo.
  query_info_callback_data_ = callback_data;

  // Guard against parameter object destruction before the async operation
  // executes (below).
  AdRequest local_ad_request = request;

  dispatch_async(dispatch_get_main_queue(), ^{
    // Create a GADRequest from a gma::AdRequest.
    AdErrorCode error_code = kAdErrorCodeNone;
    std::string error_message;
    GADRequest *ad_request =
        GADRequestFromCppAdRequest(local_ad_request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if (error_code == kAdErrorCodeNone) {
        error_code = kAdErrorCodeInternalError;
        error_message = kAdCouldNotParseAdRequestErrorMessage;
      }
      CompleteQueryInfoInternalError(query_info_callback_data_, error_code, error_message.c_str());
      query_info_callback_data_ = nil;
    } else {
      // Make the create query info request.
      [GADQueryInfo
          createQueryInfoWithRequest:ad_request
                            adFormat:MapCPPAdFormatToGADAdformat(format)
                   completionHandler:^(GADQueryInfo *query_info, NSError *error)  // NO LINT
                                     {
                                       if (error) {
                                         CreateQueryInfoFailedWithError(error);
                                       } else {
                                         CreateQueryInfoSucceeded(query_info);
                                       }
                                     }];
    }
  });

  return future;
}

Future<QueryInfoResult> QueryInfoInternalIOS::CreateQueryInfoWithAdUnit(AdFormat format,
                                                                        const AdRequest &request,
                                                                        const char *ad_unit_id) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<QueryInfoResult> *callback_data =
      CreateQueryInfoResultFutureCallbackData(kQueryInfoFnCreateQueryInfoWithAdUnit, &future_data_);
  Future<QueryInfoResult> future =
      MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  if (query_info_callback_data_ != nil) {
    CompleteQueryInfoInternalError(callback_data, kAdErrorCodeLoadInProgress,
                                   kAdLoadInProgressErrorMessage);
    return future;
  }

  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the QueryInfo.
  query_info_callback_data_ = callback_data;

  // Guard against parameter object destruction before the async operation
  // executes (below).
  AdRequest local_ad_request = request;
  NSString *local_ad_unit_id = @(ad_unit_id);

  dispatch_async(dispatch_get_main_queue(), ^{
    // Create a GADRequest from a gma::AdRequest.
    AdErrorCode error_code = kAdErrorCodeNone;
    std::string error_message;
    GADRequest *ad_request =
        GADRequestFromCppAdRequest(local_ad_request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if (error_code == kAdErrorCodeNone) {
        error_code = kAdErrorCodeInternalError;
        error_message = kAdCouldNotParseAdRequestErrorMessage;
      }
      CompleteQueryInfoInternalError(query_info_callback_data_, error_code, error_message.c_str());
      query_info_callback_data_ = nil;
    } else {
      // Make the create query info request.
      [GADQueryInfo
          createQueryInfoWithRequest:ad_request
                            adFormat:MapCPPAdFormatToGADAdformat(format)
                             adUnitID:local_ad_unit_id
                   completionHandler:^(GADQueryInfo *query_info, NSError *error)  // NO LINT
                                     {
                                       if (error) {
                                         CreateQueryInfoFailedWithError(error);
                                       } else {
                                         CreateQueryInfoSucceeded(query_info);
                                       }
                                     }];
    }
  });

  return future;
}

void QueryInfoInternalIOS::CreateQueryInfoSucceeded(GADQueryInfo *query_info) {
  firebase::MutexLock lock(mutex_);

  if (query_info_callback_data_ != nil) {
    CompleteQueryInfoInternalSuccess(query_info_callback_data_, query_info.query);
    query_info_callback_data_ = nil;
  }
}

void QueryInfoInternalIOS::CreateQueryInfoFailedWithError(NSError *gad_error) {
  firebase::MutexLock lock(mutex_);
  FIREBASE_ASSERT(gad_error);
  if (query_info_callback_data_ != nil) {
    AdErrorCode error_code = MapAdRequestErrorCodeToCPPErrorCode((GADErrorCode)gad_error.code);
    CompleteQueryInfoInternalError(query_info_callback_data_, error_code,
                                   util::NSStringToString(gad_error.localizedDescription).c_str());
    query_info_callback_data_ = nil;
  }
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
