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

#include "admob/src/android/ad_result_android.h"

#include <jni.h>

#include <memory>
#include <string>

#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(ad_error,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdError",
                         ADERROR_METHODS);

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
  internal_->j_ad_error = nullptr;
}

AdResult::AdResult(const AdResultInternal& ad_result_internal) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  internal_ = new AdResultInternal();
  internal_->is_successful = ad_result_internal.is_successful;
  internal_->is_wrapper_error = ad_result_internal.is_wrapper_error;
  internal_->j_ad_error = nullptr;

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
    FIREBASE_ASSERT(ad_result_internal.j_ad_error);

    // AdResults based on Admob Android SDK errors will fetch code, domain,
    // message, and to_string values from the Java object.
    internal_->j_ad_error = env->NewGlobalRef(ad_result_internal.j_ad_error);

    JNIEnv* env = ::firebase::admob::GetJNI();
    FIREBASE_ASSERT(env);
    internal_->code =
        MapAndroidAdRequestErrorCodeToCPPErrorCode(env->CallIntMethod(
            internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetCode)));

    jobject j_domain = env->CallObjectMethod(
        internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetDomain));
    FIREBASE_ASSERT(j_domain);
    internal_->domain = util::JStringToString(env, j_domain);
    env->DeleteLocalRef(j_domain);

    jobject j_message = env->CallObjectMethod(
        internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetMessage));
    FIREBASE_ASSERT(j_message);
    internal_->message = util::JStringToString(env, j_message);
    env->DeleteLocalRef(j_message);

    jobject j_to_string = env->CallObjectMethod(
        internal_->j_ad_error, ad_error::GetMethodId(ad_error::kToString));
    FIREBASE_ASSERT(j_to_string);
    internal_->to_string = util::JStringToString(env, j_to_string);
    env->DeleteLocalRef(j_to_string);
  }
}

AdResult::AdResult(const AdResult& ad_result) : AdResult() {
  // Reuse the assignment operator.
  *this = ad_result;
}

AdResult& AdResult::operator=(const AdResult& ad_result) {
  if (&ad_result == this) {
    return *this;
  }

  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(internal_);
  FIREBASE_ASSERT(ad_result.internal_);

  AdResultInternal* preexisting_internal = internal_;
  {
    MutexLock(ad_result.internal_->mutex);
    MutexLock(internal_->mutex);
    internal_ = new AdResultInternal();

    internal_->is_successful = ad_result.internal_->is_successful;
    internal_->is_wrapper_error = ad_result.internal_->is_wrapper_error;
    internal_->code = ad_result.internal_->code;
    internal_->domain = ad_result.internal_->domain;
    internal_->message = ad_result.internal_->message;
    if (ad_result.internal_->j_ad_error != nullptr) {
      internal_->j_ad_error =
          env->NewGlobalRef(ad_result.internal_->j_ad_error);
    }

    if (preexisting_internal->j_ad_error) {
      env->DeleteGlobalRef(preexisting_internal->j_ad_error);
      preexisting_internal->j_ad_error = nullptr;
    }
  }

  // Deleting the internal deletes the mutex within it, so we wait for complete
  // deletion until after the mutex leaves scope.
  delete preexisting_internal;
  return *this;
}

AdResult::~AdResult() {
  FIREBASE_ASSERT(internal_);
  if (internal_->j_ad_error != nullptr) {
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env);
    env->DeleteGlobalRef(internal_->j_ad_error);
    internal_->j_ad_error = nullptr;
  }
  delete internal_;
  internal_ = nullptr;
}

bool AdResult::is_successful() const {
  FIREBASE_ASSERT(internal_);
  return internal_->is_successful;
}

std::unique_ptr<AdResult> AdResult::GetCause() const {
  FIREBASE_ASSERT(internal_);

  if (internal_->is_wrapper_error) {
    return std::unique_ptr<AdResult>(nullptr);
  } else {
    FIREBASE_ASSERT(internal_->j_ad_error);
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env);

    jobject j_ad_error = env->CallObjectMethod(
        internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetCause));

    AdResultInternal ad_result_internal;
    ad_result_internal.is_wrapper_error = false;
    ad_result_internal.j_ad_error = j_ad_error;
    std::unique_ptr<AdResult> ad_result =
        std::unique_ptr<AdResult>(new AdResult(ad_result_internal));
    env->DeleteLocalRef(j_ad_error);
    return ad_result;
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

void AdResult::set_to_string(std::string to_string) {
  FIREBASE_ASSERT(internal_);
  internal_->to_string = to_string;
}
}  // namespace admob
}  // namespace firebase
