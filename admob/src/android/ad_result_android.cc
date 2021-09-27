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

#ifndef FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_ANDROID_CC_
#define FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_ANDROID_CC_

#include "admob/src/android/ad_result_android.h"

#include <jni.h>

#include <memory>
#include <string>

#include "admob/src/android/admob_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/mutex.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(ad_error,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdError",
                         ADERROR_METHODS);

struct AdResultInternal {
  jobject j_ad_error;
  Mutex mutex;
};

AdResult::kUndefinedDomain = "undefined";

AdResult::AdResult(const AdResultInternal& ad_result_internal) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  internal_ = new AdResultInternal();
  internal_->j_ad_error = env->NewGlobalRef(ad_result_internal.j_ad_error);

  code_ = 0;
}

AdResult::~AdResult() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->j_ad_error != nullptr) {
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env);
    env->DeleteGlobalRef(internal_->j_ad_error);
    delete internal_;
    internal_ = nullptr;
  }
}

AdResult::AdResult(const AdResult& ad_result) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(ad_result.internal_);

  jobject j_ad_error = env->NewGlobalRef(ad_result.internal_->j_ad_error);

  internal_ = new AdResultInternal();
  internal_->j_ad_error = j_ad_error;

  code_ = ad_result.code_;
  domain_ = ad_result.domain_;
  message_ = ad_result.message_;
  to_string_ = ad_result.to_string_;
}

bool AdResult::is_successful() const {
  FIREBASE_ASSERT(internal_);
  return (internal_->j_ad_error == nullptr);
}

std::unique_ptr<AdResult> AdResult::GetCause() {
  FIREBASE_ASSERT(internal_);

  MutexLock(internal_->mutex);

  if (internal_->j_ad_error == nullptr) {
    return std::unique_ptr<AdResult>(nullptr);
  } else {
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env);
    jobject j_ad_error = env->CallObjectMethod(
        internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetCause));

    AdResultInternal ad_result_internal;
    ad_result_internal.j_ad_error = j_ad_error;
    std::unique_ptr<AdResult> ad_result =
        std::unique_ptr<AdResult>(new AdResult(ad_result_internal));
    env->DeleteLocalRef(j_ad_error);
    return ad_result;
  }
}

/// Gets the error's code.
int AdResult::code() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->j_ad_error == nullptr) {
    return 0;
  } else if (code_ != 0) {
    return code_;
  }

  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);
  code_ = (int)env->CallIntMethod(internal_->j_ad_error,
                                  ad_error::GetMethodId(ad_error::kGetCode));
  return code_;
}

/// Gets the domain of the error.
const std::string& AdResult::domain() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->j_ad_error == nullptr) {
    return std::string();
  } else if (!domain_.empty()) {
    return domain_;
  }

  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);
  jobject j_domain = env->CallObjectMethod(
      internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetDomain));
  domain_ = util::JStringToString(env, j_domain);
  env->DeleteLocalRef(j_domain);
  return domain_;
}

/// Gets the message describing the error.
const std::string& AdResult::message() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->j_ad_error == nullptr) {
    return std::string();
  } else if (!message_.empty()) {
    return message_;
  }

  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);
  jobject j_message = env->CallObjectMethod(
      internal_->j_ad_error, ad_error::GetMethodId(ad_error::kGetMessage));
  message_ = util::JStringToString(env, j_message);
  env->DeleteLocalRef(j_message);
  return message_;
}

/// Returns a log friendly string version of this object.
const std::string& AdResult::ToString() {
  FIREBASE_ASSERT(internal_);
  MutexLock(internal_->mutex);

  if (internal_->j_ad_error == nullptr) {
    return std::string();
  } else if (!to_string_.empty()) {
    return to_string_;
  }

  JNIEnv* env = ::firebase::admob::GetJNI();
  FIREBASE_ASSERT(env);
  jobject j_to_string = env->CallObjectMethod(
      internal_->j_ad_error, ad_error::GetMethodId(ad_error::kToString));
  to_string_ = util::JStringToString(env, j_to_string);
  env->DeleteLocalRef(j_to_string);
  return to_string_;
}

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_INTERNAL_ANDROID_H_
