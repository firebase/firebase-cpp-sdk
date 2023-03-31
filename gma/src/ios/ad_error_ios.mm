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

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#import "gma/src/ios/FADRequest.h"

#include <string>

#include "app/src/assert.h"
#include "app/src/util_ios.h"
#include "gma/src/common/ad_error_internal.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/ios/ad_error_ios.h"
#include "gma/src/ios/response_info_ios.h"


namespace firebase {
namespace gma {

const char* const AdError::kUndefinedDomain = "undefined";

AdError::AdError() {
  // Default constructor is available for Future creation.
  // Initialize it with some helpful debug values in the case
  // an AdError makes it to the application in this default state.
  internal_ = new AdErrorInternal();
  internal_->is_successful = false;
  internal_->ad_error_type = AdErrorInternal::kAdErrorInternalWrapperError;
  internal_->code = kAdErrorCodeUninitialized;
  internal_->domain = "SDK";
  internal_->message = "This AdError has not be initialized.";
  internal_->to_string = internal_->message;
  internal_->native_ad_error = nullptr;

  // While most data is passed into this object through the AdErrorInternal
  // structure (above), the response_info_ is constructed when parsing
  // the j_ad_error itself.
  response_info_ = new ResponseInfo();
}

AdError::AdError(const AdErrorInternal& ad_error_internal) {
  internal_ = new AdErrorInternal();

  internal_->is_successful = ad_error_internal.is_successful;
  internal_->ad_error_type = ad_error_internal.ad_error_type;
  internal_->native_ad_error = nullptr;
  response_info_ = new ResponseInfo();

  // AdErrors can be returned on success, or for errors encountered in the C++
  // SDK wrapper, or in the iOS GMA SDK.  The structure is populated
  // differently across these three scenarios.
  if (internal_->is_successful) {
    internal_->code = kAdErrorCodeNone;
    internal_->message = "";
    internal_->domain = "";
    internal_->to_string = "";
  } else if (internal_->ad_error_type == AdErrorInternal::kAdErrorInternalWrapperError) {
    // Wrapper errors come with prepopulated code, domain, etc, fields.
    internal_->code = ad_error_internal.code;
    internal_->domain = ad_error_internal.domain;
    internal_->message = ad_error_internal.message;
    internal_->to_string = ad_error_internal.to_string;
  } else {
    FIREBASE_ASSERT(ad_error_internal.native_ad_error);

    // AdErrors based on GMA iOS SDK errors will fetch code, domain,
    // message, and to_string values from the ObjC object.
    internal_->native_ad_error = ad_error_internal.native_ad_error;

    // Error Code.  Map the iOS GMA SDK error codes to our
    // platform-independent C++ SDK error codes.
    switch (internal_->ad_error_type) {
      case AdErrorInternal::kAdErrorInternalFullScreenContentError:
        // Full screen content errors have their own error codes.
        internal_->code = MapFullScreenContentErrorCodeToCPPErrorCode(
            (GADPresentationErrorCode)internal_->native_ad_error.code);
        break;
      case AdErrorInternal::kAdErrorInternalOpenAdInspectorError:
        // OpenAdInspector errors are all internal errors on iOS.
        internal_->code = kAdErrorCodeInternalError;
        break;
      default:
        internal_->code =
            MapAdRequestErrorCodeToCPPErrorCode((GADErrorCode)internal_->native_ad_error.code);
    }

    internal_->domain = util::NSStringToString(internal_->native_ad_error.domain);
    internal_->message = util::NSStringToString(internal_->native_ad_error.localizedDescription);

    // Errors from LoadAd attempts have extra data pertaining to adapter
    // responses.
    if (internal_->ad_error_type == AdErrorInternal::kAdErrorInternalLoadAdError) {
      ResponseInfoInternal response_info_internal = ResponseInfoInternal(
          {ad_error_internal.native_ad_error.userInfo[GADErrorUserInfoKeyResponseInfo]});
      *response_info_ = ResponseInfo(response_info_internal);
    }

    NSString* ns_to_string = [[NSString alloc]
        initWithFormat:@"Received error with "
                        "domain: %@, code: %ld, message: %@",
                       internal_->native_ad_error.domain, (long)internal_->native_ad_error.code,
                       internal_->native_ad_error.localizedDescription];
    internal_->to_string = util::NSStringToString(ns_to_string);
  }
}

AdError::AdError(const AdError& ad_result) : AdError() {
  // Reuse the assignment operator.
  this->response_info_ = new ResponseInfo();
  *this = ad_result;
}

AdError::~AdError() {
  FIREBASE_ASSERT(internal_);
  FIREBASE_ASSERT(response_info_);

  internal_->native_ad_error = nil;
  delete internal_;
  internal_ = nullptr;

  delete response_info_;
  response_info_ = nullptr;
}

AdError& AdError::operator=(const AdError& ad_result) {
  FIREBASE_ASSERT(ad_result.internal_);
  FIREBASE_ASSERT(internal_);
  FIREBASE_ASSERT(response_info_);
  FIREBASE_ASSERT(ad_result.response_info_);

  AdErrorInternal* preexisting_internal = internal_;
  {
    MutexLock(ad_result.internal_->mutex);
    MutexLock(internal_->mutex);
    internal_ = new AdErrorInternal();

    internal_->native_ad_error = ad_result.internal_->native_ad_error;

    internal_->is_successful = ad_result.internal_->is_successful;
    internal_->ad_error_type = ad_result.internal_->ad_error_type;
    internal_->code = ad_result.internal_->code;
    internal_->domain = ad_result.internal_->domain;
    internal_->message = ad_result.internal_->message;
    internal_->to_string = ad_result.internal_->to_string;

    *response_info_ = *ad_result.response_info_;
  }

  // Deleting the internal deletes the mutex within it, so we have to delete
  // the internal after the mutex lock leaves scope.
  preexisting_internal->native_ad_error = nullptr;
  delete preexisting_internal;

  return *this;
}

std::unique_ptr<AdError> AdError::GetCause() const {
  FIREBASE_ASSERT(internal_);

  NSError* cause = internal_->native_ad_error.userInfo[NSUnderlyingErrorKey];
  if (cause == nil) {
    return std::unique_ptr<AdError>(nullptr);
  } else {
    AdErrorInternal ad_error_internal;
    ad_error_internal.native_ad_error = cause;
    return std::unique_ptr<AdError>(new AdError(ad_error_internal));
  }
}

/// Gets the error's code.
AdErrorCode AdError::code() const {
  FIREBASE_ASSERT(internal_);
  return internal_->code;
}

/// Gets the domain of the error.
const std::string& AdError::domain() const {
  FIREBASE_ASSERT(internal_);
  return internal_->domain;
}

/// Gets the message describing the error.
const std::string& AdError::message() const {
  FIREBASE_ASSERT(internal_);
  return internal_->message;
}

const ResponseInfo& AdError::response_info() const {
  FIREBASE_ASSERT(response_info_);
  return *response_info_;
}

/// Returns a log friendly string version of this object.
const std::string& AdError::ToString() const {
  FIREBASE_ASSERT(internal_);
  return internal_->to_string;
}

}  // namespace gma
}  // namespace firebase
