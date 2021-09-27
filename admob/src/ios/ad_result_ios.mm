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

#include <string>

#import <GoogleMobileAds/GoogleMobileAds.h>>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace admob {

struct AdResultInternal {
  NSError* ios_error;
  Mutex mutex;
};

AdResult::AdResult(const AdResultInternal& ad_result_internal) {
  internal_ = new AdResultInternal();
  internal_->ios_error = ad_result_internal.ios_error;
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

AdResult::AdResult(const AdResult& ad_result) {
  FIREBASE_ASSERT(ad_result.internal_);

  internal_ = new AdResultInternal();
  internal_->ios_error = ad_result.internal_->ios_error;

  code_ = ad_result.code_;
  domain_ = ad_result.domain_;
  message_ = ad_result.message_;
  to_string_ = ad_result.to_string_;
}

bool AdResult::is_successful() const {
  FIREBASE_ASSERT(internal_);
  return (internal_->ios_error == nil);
}

std::unique_ptr<AdResult> AdResult::GetCause() {
  FIREBASE_ASSERT(internal_);

  MutexLock(internal_->mutex);
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
int AdResult::code() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->ios_error == nullptr) {
    return 0;
  } else if (code_ != 0) {
    return code_;
  }

  code_ = internal_->ios_error.code;
  return code_;
}

/// Gets the domain of the error.
const std::string& AdResult::domain() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->ios_error == nullptr) {
    return std::string();
  } else if (!domain_.empty()) {
    return domain_;
  }

  domain_ = util::NSStringToString(internal_->ios_error.domain);
  return domain_;
}

/// Gets the message describing the error.
const std::string& AdResult::message() {
    FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->ios_error == nullptr) {
    return std::string();
  } else if (!message_.empty()) {
    return message_;
  }

  message_ = util::NSStringToString(internal_->ios_error.localizedDescription);
  return message_;
}

/// Returns a log friendly string version of this object.
const std::string& AdResult::ToString() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->ios_error == nullptr) {
    return std::string();
  } else if(!to_string_.empty()) {
    return to_string_;
  }

  NSString *to_string = [[NSString alloc]initWithFormat:@"Received error with "
        "domain: %@, code: %ld, message: %@", internal_->ios_error.domain,
        internal_->ios_error.code, internal_->ios_error.localizedDescription];
  to_string_ = util::NSStringToString(to_string);
  return to_string_;
}

}  // namespace admob
}  // namespace firebase
