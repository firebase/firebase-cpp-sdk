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

#include "admob/src/android/load_ad_result_android.h"

#include <jni.h>

#include <memory>
#include <string>

#include "admob/src/android/admob_android.h"
#include "admob/src/android/response_info_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"

#include <android/log.h>

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(load_ad_error,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/LoadAdError",
                         LOADADERROR_METHODS);

LoadAdResult::LoadAdResult()
    : AdResult(), response_info_() {
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult() default constructor, this: %p", this);
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult() default constructor end, j_ad_error: %p", internal_->j_ad_error);
}

LoadAdResult::LoadAdResult(const LoadAdResultInternal& load_ad_result_internal)
    : AdResult(load_ad_result_internal.ad_result_internal) {
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult(LoadAdResultInternal) , this: %p", this);
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);

  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult(LoadAdResultInternal)load_ad_result_internal is_successful: %d", load_ad_result_internal.ad_result_internal.is_successful);
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult(LoadAdResultInternal) load_ad_result_internal is_wrapper_error: %d", load_ad_result_internal.ad_result_internal.is_wrapper_error);
  if (!load_ad_result_internal.ad_result_internal.is_successful &&
      !load_ad_result_internal.ad_result_internal.is_wrapper_error) {
    __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult(LoadAdResultInternal) load_ad_result_internal j_ad_error: %p", load_ad_result_internal.ad_result_internal.j_ad_error);
    FIREBASE_ASSERT(load_ad_result_internal.ad_result_internal.j_ad_error);

    jobject j_load_ad_error =
        load_ad_result_internal.ad_result_internal.j_ad_error;

    jobject j_response_info = env->CallObjectMethod(
        j_load_ad_error,
        load_ad_error::GetMethodId(load_ad_error::kGetResponseInfo));

    if (j_response_info != nullptr) {
      ResponseInfoInternal response_info_internal;
      response_info_internal.j_response_info = j_response_info;
      response_info_ = ResponseInfo(response_info_internal);
      env->DeleteLocalRef(j_response_info);
    }

    jobject j_to_string = env->CallObjectMethod(
        j_load_ad_error, load_ad_error::GetMethodId(load_ad_error::kToString));
    set_to_string(util::JStringToString(env, j_to_string));
    env->DeleteLocalRef(j_to_string);
  }
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult(LoadAdResultInternal) final j_ad_error: %p", internal_->j_ad_error);
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult(LoadAdResultInternal) end");
}

LoadAdResult::LoadAdResult(const LoadAdResult& load_ad_result) : AdResult(load_ad_result) {
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult() copy constructor, this: %p", this);
  response_info_ = load_ad_result.response_info_;
  __android_log_print(ANDROID_LOG_ERROR, "DEDB", "LoadAdResult::LoadAdResult() copy constructor, j_ad_error: %p", internal_->j_ad_error);
}

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_ANDROID_CC_