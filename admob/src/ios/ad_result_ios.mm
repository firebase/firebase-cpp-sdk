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

#import "admob/src/ios/FADRequest.h"

#include <string>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/ios/ad_result_ios.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace admob {

const char* AdResult::kUndefinedDomain = "undefined";

AdResult::AdResult() {
  // Default constructor is available for Future creation.
  // Initialize it with some helpful debug values in case we encounter a
  // scenario where an AdResult makes it to the application in such a state.
  internal_ = new AdResultInternal();
  internal_->is_successful = false;
  internal_->is_wrapper_error = true;
  internal_->code = kAdMobErrorUninitialized;
  internal_->domain = "SDK";
  internal_->message = "This AdResult has not be initialized.";
  internal_->to_string = internal_->message;
  internal_->ios_error = nullptr;
}

AdResult::AdResult(const AdResultInternal& ad_result_internal) {
  internal_ = new AdResultInternal();

  internal_->is_successful = ad_result_internal.is_successful;
  internal_->is_wrapper_error = ad_result_internal.is_wrapper_error;
  internal_->ios_error = nullptr;

  if (internal_->is_successful) {
    internal_->code = kAdMobErrorNone;
    internal_->is_wrapper_error = false;
  } else if (internal_->is_wrapper_error) {
    // Wrapper errors come with prepopulated code, domain, etc, fields.
    internal_->code = ad_result_internal.code;
    internal_->domain = ad_result_internal.domain;
    internal_->message = ad_result_internal.message;
    internal_->to_string = ad_result_internal.to_string;
  } else {
    FIREBASE_ASSERT(ad_result_internal.ios_error);

    // AdResults based on Admob iOS SDK errors will fetch code, domain,
    // message, and to_string values from the ObjC object.
    internal_->ios_error = ad_result_internal.ios_error;
    internal_->code = MapGADErrorCodeToCPPErrorCode(internal_->ios_error.code);
    internal_->domain = util::NSStringToString(internal_->ios_error.domain);
    internal_->message =
      util::NSStringToString(internal_->ios_error.localizedDescription);

    NSString* ns_to_string = [[NSString alloc]initWithFormat:@"Received error with "
        "domain: %@, code: %ld, message: %@", internal_->ios_error.domain,
        internal_->ios_error.code, internal_->ios_error.localizedDescription];
    internal_->to_string = util::NSStringToString(ns_to_string);
  }
}

AdResult::AdResult(const AdResult& ad_result) : AdResult() {
  // Reuse the assignment operator.
  *this = ad_result;
}

AdResult::~AdResult() {
  FIREBASE_ASSERT(internal_);
  {
    MutexLock(internal_->mutex);
    internal_->ios_error = nil;
  }
  delete internal_;
  internal_ = nullptr;
}

AdResult& AdResult::operator=(const AdResult& ad_result) {
  FIREBASE_ASSERT(ad_result.internal_);
  FIREBASE_ASSERT(internal_);

  AdResultInternal* preexisting_internal = internal_;
  {
    MutexLock(ad_result.internal_->mutex);
    MutexLock(internal_->mutex);
    internal_ = new AdResultInternal();

    internal_->ios_error = ad_result.internal_->ios_error;

    internal_->is_successful = ad_result.internal_->is_successful;
    internal_->code = ad_result.internal_->code;
    internal_->domain = ad_result.internal_->domain;
    internal_->message = ad_result.internal_->message;
    internal_->to_string = ad_result.internal_->to_string;
  }

  // Deleting the internal deletes the mutex within it, so we have to delete
  // the internal after the MutexLock leaves scope.
  preexisting_internal->ios_error = nullptr;
  delete preexisting_internal;

  return *this;
}

bool AdResult::is_successful() const {
  FIREBASE_ASSERT(internal_);
  return internal_->is_successful;
}

std::unique_ptr<AdResult> AdResult::GetCause() const {
  FIREBASE_ASSERT(internal_);

  NSError* cause = internal_->ios_error.userInfo[NSUnderlyingErrorKey];
  if (cause == nil) {
    return std::unique_ptr<AdResult>(nullptr);
  } else {
    AdResultInternal ad_result_internal;
    ad_result_internal.ios_error = cause;
    return std::unique_ptr<AdResult>(new AdResult(ad_result_internal));
  }
}

/// Gets the error's code.
AdMobError AdResult::code() const {
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

/// Returns a log friendly string version of this object.
const std::string& AdResult::ToString() const {
  FIREBASE_ASSERT(internal_);
  return internal_->to_string;
}

}  // namespace admob
}  // namespace firebase
