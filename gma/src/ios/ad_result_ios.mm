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

#include "gma/src/common/ad_result_internal.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/ios/ad_result_ios.h"
#include "gma/src/ios/response_info_ios.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace gma {

const char* const AdResult::kUndefinedDomain = "undefined";

AdResult::AdResult() {
  // Default constructor is available for Future creation.
  // Initialize it with some helpful debug values in the case
  // an AdResult makes it to the application in this default state.
  internal_ = new AdResultInternal();
  internal_->is_successful = false;
  internal_->ad_result_type = AdResultInternal::kAdResultInternalWrapperError;
  internal_->code = kAdErrorUninitialized;
  internal_->domain = "SDK";
  internal_->message = "This AdResult has not be initialized.";
  internal_->to_string = internal_->message;
  internal_->native_ad_error = nullptr;

  // While most data is passed into this object through the AdResultInternal
  // structure (above), the response_info_ is constructed when parsing
  // the j_ad_error itself.
  response_info_ = new ResponseInfo();
}

AdResult::AdResult(const AdResultInternal& ad_result_internal) {
  internal_ = new AdResultInternal();

  internal_->is_successful = ad_result_internal.is_successful;
  internal_->ad_result_type = ad_result_internal.ad_result_type;
  internal_->native_ad_error = nullptr;
  response_info_ = new ResponseInfo();

  // AdResults can be returned on success, or for errors encountered in the C++
  // SDK wrapper, or in the iOS GMA SDK.  The stucture is populated
  // differently across these three scenarios.
  if (internal_->is_successful) {
    internal_->code = kAdErrorNone;
    internal_->message = "";
    internal_->domain = "";
    internal_->to_string = "";
  } else if (internal_->ad_result_type ==
             AdResultInternal::kAdResultInternalWrapperError) {
    // Wrapper errors come with prepopulated code, domain, etc, fields.
    internal_->code = ad_result_internal.code;
    internal_->domain = ad_result_internal.domain;
    internal_->message = ad_result_internal.message;
    internal_->to_string = ad_result_internal.to_string;
  } else {
    FIREBASE_ASSERT(ad_result_internal.native_ad_error);

    // AdResults based on GMA iOS SDK errors will fetch code, domain,
    // message, and to_string values from the ObjC object.
    internal_->native_ad_error = ad_result_internal.native_ad_error;

    // Error Code.  Map the iOS GMA SDK error codes to our
    // platform-independent C++ SDK error codes.
    switch (internal_->ad_result_type) {
      case AdResultInternal::kAdResultInternalFullScreenContentError:
        // Full screen content errors have their own error codes.
        internal_->code =
            MapFullScreenContentErrorCodeToCPPErrorCode((GADPresentationErrorCode)internal_->native_ad_error.code);
        break;
      default:
        internal_->code =
            MapAdRequestErrorCodeToCPPErrorCode((GADErrorCode)internal_->native_ad_error.code);
    }

    internal_->domain = util::NSStringToString(internal_->native_ad_error.domain);
    internal_->message =
      util::NSStringToString(internal_->native_ad_error.localizedDescription);

    // Errors from LoadAd attempts have extra data pertaining to adapter
    // responses.
    if (internal_->ad_result_type == AdResultInternal::kAdResultInternalLoadAdError) {
      ResponseInfoInternal response_info_internal = ResponseInfoInternal( {
        ad_result_internal.native_ad_error.userInfo[GADErrorUserInfoKeyResponseInfo]
      });
      *response_info_ = ResponseInfo(response_info_internal);
    }

    NSString* ns_to_string = [[NSString alloc]initWithFormat:@"Received error with "
        "domain: %@, code: %ld, message: %@", internal_->native_ad_error.domain,
        (long)internal_->native_ad_error.code,
        internal_->native_ad_error.localizedDescription];
    internal_->to_string = util::NSStringToString(ns_to_string);
  }
}

AdResult::AdResult(const AdResult& ad_result) : AdResult() {
  // Reuse the assignment operator.
  this->response_info_ = new ResponseInfo();
  *this = ad_result;
}

AdResult::~AdResult() {
  FIREBASE_ASSERT(internal_);
  FIREBASE_ASSERT(response_info_);

  internal_->native_ad_error = nil;
  delete internal_;
  internal_ = nullptr;

  delete response_info_;
  response_info_ = nullptr;
}

AdResult& AdResult::operator=(const AdResult& ad_result) {
  FIREBASE_ASSERT(ad_result.internal_);
  FIREBASE_ASSERT(internal_);
  FIREBASE_ASSERT(response_info_);
  FIREBASE_ASSERT(ad_result.response_info_);

  AdResultInternal* preexisting_internal = internal_;
  {
    MutexLock(ad_result.internal_->mutex);
    MutexLock(internal_->mutex);
    internal_ = new AdResultInternal();

    internal_->native_ad_error = ad_result.internal_->native_ad_error;

    internal_->is_successful = ad_result.internal_->is_successful;
    internal_->ad_result_type = ad_result.internal_->ad_result_type;
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

bool AdResult::is_successful() const {
  FIREBASE_ASSERT(internal_);
  return internal_->is_successful;
}

std::unique_ptr<AdResult> AdResult::GetCause() const {
  FIREBASE_ASSERT(internal_);

  NSError* cause = internal_->native_ad_error.userInfo[NSUnderlyingErrorKey];
  if (cause == nil) {
    return std::unique_ptr<AdResult>(nullptr);
  } else {
    AdResultInternal ad_result_internal;
    ad_result_internal.native_ad_error = cause;
    return std::unique_ptr<AdResult>(new AdResult(ad_result_internal));
  }
}

/// Gets the error's code.
AdError AdResult::code() const {
  FIREBASE_ASSERT(internal_);
  return internal_->code;
}

/// Gets the domain of the error.
const std::string& AdResult::domain() const {
  FIREBASE_ASSERT(internal_);
  return internal_->domain;
}

/// Gets the message describing the error.
const std::string& AdResult::message() const {
  FIREBASE_ASSERT(internal_);
  return internal_->message;
}

const ResponseInfo& AdResult::response_info() const {
  FIREBASE_ASSERT(response_info_);
  return *response_info_;
}

/// Returns a log friendly string version of this object.
const std::string& AdResult::ToString() const {
  FIREBASE_ASSERT(internal_);
  return internal_->to_string;
}

}  // namespace gma
}  // namespace firebase
