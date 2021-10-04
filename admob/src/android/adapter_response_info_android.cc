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

#include "admob/src/android/adapter_response_info_android.h"

#include <string>

#include "admob/src/android/ad_result_android.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(adapter_response_info,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdapterResponseInfo",
                         ADAPTERRESPONSEINFO_METHODS);

AdapterResponseInfo::AdapterResponseInfo(
    const AdapterResponseInfoInternal& internal) {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(internal.j_adapter_response_info);

  const jobject j_adapter_response_info =
      env->NewLocalRef(internal.j_adapter_response_info);

  AdResultInternal ad_result_internal;
  ad_result_internal.j_ad_error = env->CallObjectMethod(
      j_adapter_response_info,
      adapter_response_info::GetMethodId(adapter_response_info::kGetAdError));
  FIREBASE_ASSERT(ad_result_internal.j_ad_error);

  ad_result_ = AdResult(ad_result_internal);

  env->DeleteLocalRef(ad_result_internal.j_ad_error);

  const jobject j_adapter_class_name =
      env->CallObjectMethod(j_adapter_response_info,
                            adapter_response_info::GetMethodId(
                                adapter_response_info::kGetAdapterClassName));
  FIREBASE_ASSERT(j_adapter_class_name);
  adapter_class_name_ = util::JStringToString(env, j_adapter_class_name);
  env->DeleteLocalRef(j_adapter_class_name);

  const jlong j_latency = env->CallLongMethod(
      j_adapter_response_info, adapter_response_info::GetMethodId(
                                   adapter_response_info::kGetLatencyMillis));
  latency_ = (int64_t)j_latency;

  const jobject j_to_string = env->CallObjectMethod(
      j_adapter_response_info,
      adapter_response_info::GetMethodId(adapter_response_info::kToString));
  FIREBASE_ASSERT(j_to_string);
  to_string_ = util::JStringToString(env, j_to_string);
  env->DeleteLocalRef(j_to_string);
  env->DeleteLocalRef(j_adapter_response_info);
}

}  // namespace admob
}  // namespace firebase
