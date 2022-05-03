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

#include "gma/src/android/adapter_response_info_android.h"

#include <string>

#include "gma/src/android/ad_error_android.h"
#include "gma/src/android/gma_android.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"

namespace firebase {
namespace gma {

METHOD_LOOKUP_DEFINITION(adapter_response_info,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdapterResponseInfo",
                         ADAPTERRESPONSEINFO_METHODS);

AdapterResponseInfo::AdapterResponseInfo(
    const AdapterResponseInfoInternal& internal)
    : ad_result_() {
  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(internal.j_adapter_response_info);

  // Parse the fields of the GMA Android SDK's AdapterResponseInfo using the
  // reference in the internal structure.
  const jobject j_adapter_response_info =
      env->NewLocalRef(internal.j_adapter_response_info);

  // Construct an AdErrorInternal from the Android SDK AdError object in the
  // AdapterResponseInfo.
  const jobject j_native_ad_error = env->CallObjectMethod(
      j_adapter_response_info,
      adapter_response_info::GetMethodId(adapter_response_info::kGetAdError));

  if (j_native_ad_error != nullptr) {
    AdErrorInternal ad_error_internal;
    ad_error_internal.native_ad_error = j_native_ad_error;
    AdError ad_error = AdError(ad_error_internal);
    if (ad_error.code() != kAdErrorCodeNone) {
      ad_result_ = AdResult(ad_error);
    }
    env->DeleteLocalRef(ad_error_internal.native_ad_error);
  }

  // The class name of the adapter.
  const jobject j_adapter_class_name =
      env->CallObjectMethod(j_adapter_response_info,
                            adapter_response_info::GetMethodId(
                                adapter_response_info::kGetAdapterClassName));
  FIREBASE_ASSERT(j_adapter_class_name);
  adapter_class_name_ = util::JStringToString(env, j_adapter_class_name);
  env->DeleteLocalRef(j_adapter_class_name);

  // The latenency in milliseconds of time between request and response.
  const jlong j_latency = env->CallLongMethod(
      j_adapter_response_info, adapter_response_info::GetMethodId(
                                   adapter_response_info::kGetLatencyMillis));
  latency_ = (int64_t)j_latency;

  // A string represenation of the AdapterResponseInfo.
  const jobject j_to_string = env->CallObjectMethod(
      j_adapter_response_info,
      adapter_response_info::GetMethodId(adapter_response_info::kToString));
  FIREBASE_ASSERT(j_to_string);
  to_string_ = util::JStringToString(env, j_to_string);
  env->DeleteLocalRef(j_to_string);
  env->DeleteLocalRef(j_adapter_response_info);
}

}  // namespace gma
}  // namespace firebase
