/*
 * Copyright 2021 Google LLC
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

#ifndef FIREBASE_GMA_SRC_COMMON_GMA_COMMON_H_
#define FIREBASE_GMA_SRC_COMMON_GMA_COMMON_H_

#include <stdarg.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/reference_counted_future_impl.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"
#include "gma/src/include/firebase/gma/internal/query_info.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

// Error messages used for completing futures. These match the error codes in
// the AdErrorCode enumeration in the C++ API.
extern const char* kAdAlreadyInitializedErrorMessage;
extern const char* kAdCouldNotParseAdRequestErrorMessage;
extern const char* kAdLoadInProgressErrorMessage;
extern const char* kAdUninitializedErrorMessage;
extern const char* kImageUrlMalformedErrorMessage;
extern const char* kUnsupportedVariantTypeErrorMessage;
extern const char* kRecordImpressionFailureErrorMessage;

namespace internal {
class AdViewInternal;
class NativeAdInternal;
}  // namespace internal

// Determine whether GMA is initialized.
bool IsInitialized();

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Unregisters the cleanup task for this module if auto-initialization is
// disabled.
void UnregisterTerminateOnDefaultAppDestroy();

// Get the cleanup notifier for the GMA module, creating one if it doesn't
// exist.  This allows all objects that depend upon GMA's lifecycle to be
// cleaned up if the module is terminated.
CleanupNotifier* GetOrCreateCleanupNotifier();

// Destroy the cleanup notifier.
void DestroyCleanupNotifier();

// Returns the request agent string for this library.
const char* GetRequestAgentString();

// Hold backing data for returned Futures.
struct FutureData {
  explicit FutureData(int num_functions_that_return_futures)
      : future_impl(num_functions_that_return_futures) {}

  // Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl;
};

// Create a future and update the corresponding last result.
template <class T>
SafeFutureHandle<T> CreateFuture(int fn_idx, FutureData* future_data);

// Mark a future as complete.
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<void> handle, FutureData* future_data);

template <class T>
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<T> handle, FutureData* future_data,
                    const T& result);

// For calls that aren't asynchronous, create and complete the future at the
// same time.
Future<void> CreateAndCompleteFuture(int fn_idx, int error,
                                     const char* error_msg,
                                     FutureData* future_data);

// For calls that aren't asynchronous, create and complete a future with a
// result at the same time.
Future<AdResult> CreateAndCompleteFutureWithResult(int fn_idx, int error,
                                                   const char* error_msg,
                                                   FutureData* future_data,
                                                   const AdResult& result);

// For calls that aren't asynchronous, create and complete a future with a
// result at the same time.
Future<ImageResult> CreateAndCompleteFutureWithImageResult(
    int fn_idx, int error, const char* error_msg, FutureData* future_data,
    const ImageResult& result);

// For calls that aren't asynchronous, create and complete a future with a
// result at the same time.
Future<QueryInfoResult> CreateAndCompleteFutureWithQueryInfoResult(
    int fn_idx, int error, const char* error_msg, FutureData* future_data,
    const QueryInfoResult& result);

template <class T>
struct FutureCallbackData {
  FutureData* future_data;
  SafeFutureHandle<T> future_handle;
};

// Constructs a FutureCallbackData instance to handle operations that return
// void Futures.
FutureCallbackData<void>* CreateVoidFutureCallbackData(int fn_idx,
                                                       FutureData* future_data);

// Constructs a FutureCallbackData instance to handle results from LoadAd
// requests.
FutureCallbackData<AdResult>* CreateAdResultFutureCallbackData(
    int fn_idx, FutureData* future_data);

// Constructs a FutureCallbackData instance to handle results from LoadImage
// requests.
FutureCallbackData<ImageResult>* CreateImageResultFutureCallbackData(
    int fn_idx, FutureData* future_data);

// Constructs a FutureCallbackData instance to handle results from
// createQueryInfo requests.
FutureCallbackData<QueryInfoResult>* CreateQueryInfoResultFutureCallbackData(
    int fn_idx, FutureData* future_data);

// Template function implementations.
template <class T>
SafeFutureHandle<T> CreateFuture(int fn_idx, FutureData* future_data) {
  return future_data->future_impl.SafeAlloc<T>(fn_idx);
}
template <class T>
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<T> handle, FutureData* future_data,
                    const T& result) {
  future_data->future_impl.CompleteWithResult(handle, error, error_msg, result);
}

// A class that allows access to private/protected GMA structures for Java
// callbacks.  This is achieved via friend relationships with those classes.
class GmaInternal {
 public:
  // Completes an AdResult future with a successful result.
  static void CompleteLoadAdFutureSuccess(
      FutureCallbackData<AdResult>* callback_data,
      const ResponseInfoInternal& response_info);

  // Completes an AdResult future as an error given the AdErrorInternal object.
  static void CompleteLoadAdFutureFailure(
      FutureCallbackData<AdResult>* callback_data, int error_code,
      const std::string& error_message,
      const AdErrorInternal& ad_error_internal);

  // Completes an ImageResult future with a successful result.
  static void CompleteLoadImageFutureSuccess(
      FutureCallbackData<ImageResult>* callback_data,
      const std::vector<unsigned char>& img_data);

  // Completes an ImageResult future as an error.
  static void CompleteLoadImageFutureFailure(
      FutureCallbackData<ImageResult>* callback_data, int error_code,
      const std::string& error_message);

  // Completes an QueryInfoResult future with a successful result.
  static void CompleteCreateQueryInfoFutureSuccess(
      FutureCallbackData<QueryInfoResult>* callback_data,
      const std::string& query_info_data);

  // Completes an QueryInfoResult future as an error.
  static void CompleteCreateQueryInfoFutureFailure(
      FutureCallbackData<QueryInfoResult>* callback_data, int error_code,
      const std::string& error_message);

  // Constructs and returns an AdError object given an AdErrorInternal object.
  static AdError CreateAdError(const AdErrorInternal& ad_error_internal);

  // Construct and return an AdapterStatus with the given values.
  static AdapterStatus CreateAdapterStatus(const std::string& description,
                                           bool is_initialized, int latency) {
    AdapterStatus status;
    status.description_ = description;
    status.is_initialized_ = is_initialized;
    status.latency_ = latency;
    return status;
  }

  // Construct and return an AdapterInitializationStatus with the given
  // statuses.
  static AdapterInitializationStatus CreateAdapterInitializationStatus(
      const std::map<std::string, AdapterStatus>& status_map) {
    AdapterInitializationStatus init_status;
    init_status.adapter_status_map_ = status_map;
    return init_status;
  }

  // Update the AdViewInternal's AdSize width and height after it has been
  // loaded as AdViews with adaptive AdSizes may have varying dimensions.
  // This is done through the GmaInternal since it's a friend of AdViewInternal.
  static void UpdateAdViewInternalAdSizeDimensions(
      internal::AdViewInternal* ad_view_internal, int width, int height);

  // Add to the NativeAdInternal's image assets, after the native ad has loaded.
  // This is done through the GmaInternal since it's a friend of
  // NativeAdInternal.
  static void InsertNativeInternalImage(
      internal::NativeAdInternal* native_ad_internal,
      const NativeAdImageInternal& native_image_internal,
      const std::string& image_type, const bool& clear_existing_images);
};

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_GMA_COMMON_H_
