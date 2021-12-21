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

#include "gma/src/android/ad_request_converter.h"

#include <assert.h>
#include <jni.h>

#include <cstdarg>
#include <cstddef>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/src/log.h"
#include "app/src/util_android.h"
#include "gma/gma_resources.h"
#include "gma/src/android/gma_android.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

METHOD_LOOKUP_DEFINITION(ad_request_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdRequest$Builder",
                         ADREQUESTBUILDER_METHODS);

jobject GetJavaAdRequestFromCPPAdRequest(const AdRequest& request,
                                         gma::AdError* error) {
  FIREBASE_ASSERT(error);
  *error = kAdErrorNone;

  JNIEnv* env = ::firebase::gma::GetJNI();
  jobject builder = env->NewObject(
      ad_request_builder::GetClass(),
      ad_request_builder::GetMethodId(ad_request_builder::kConstructor));

  // Network Extras.
  // The map associates class names of mediation adapters with a
  // key value pairs, the extras, to send to those medation adapters.
  // e.g. Mediation_Map < class name, Extras_Map < key, value > >
  const std::map<std::string, std::map<std::string, std::string>>& extras =
      request.extras();
  for (auto adapter_iter = extras.begin(); adapter_iter != extras.end();
       ++adapter_iter) {
    std::string adapter_name = adapter_iter->first;

    jclass adapter_class = util::FindClass(env, adapter_name.c_str());
    if (adapter_class == nullptr) {
      LogError(
          "Failed to resolve extras class. Check that \"%s\""
          " is present in your APK.",
          adapter_name.c_str());
      *error = kAdErrorAdNetworkClassLoadError;
      env->DeleteLocalRef(builder);
      return nullptr;
    }

    jobject extras_bundle =
        env->NewObject(util::bundle::GetClass(),
                       util::bundle::GetMethodId(util::bundle::kConstructor));

    for (auto extra_iter = adapter_iter->second.begin();
         extra_iter != adapter_iter->second.end(); ++extra_iter) {
      jstring extra_key_str = env->NewStringUTF(extra_iter->first.c_str());
      jstring extra_value_str = env->NewStringUTF(extra_iter->second.c_str());
      env->CallVoidMethod(extras_bundle,
                          util::bundle::GetMethodId(util::bundle::kPutString),
                          extra_key_str, extra_value_str);
      env->DeleteLocalRef(extra_value_str);
      env->DeleteLocalRef(extra_key_str);
    }

    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(builder,
                              ad_request_builder::GetMethodId(
                                  ad_request_builder::kAddNetworkExtrasBundle),
                              adapter_class, extras_bundle));

    env->DeleteLocalRef(extras_bundle);
    env->DeleteLocalRef(adapter_class);
  }

  // Keywords.
  const std::unordered_set<std::string>& keywords = request.keywords();
  for (auto keyword = keywords.begin(); keyword != keywords.end(); ++keyword) {
    jstring keyword_str = env->NewStringUTF((*keyword).c_str());
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(
            builder,
            ad_request_builder::GetMethodId(ad_request_builder::kAddKeyword),
            keyword_str));
    env->DeleteLocalRef(keyword_str);
  }

  // Content URL
  if (!request.content_url().empty()) {
    jstring content_url = env->NewStringUTF(request.content_url().c_str());
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(
            builder,
            ad_request_builder::GetMethodId(ad_request_builder::kSetContentUrl),
            content_url));
    env->DeleteLocalRef(content_url);
  }

  // Neighboring Content Urls.
  if (!request.neighboring_content_urls().empty()) {
    jobject url_list = util::StdUnorderedSetToJavaList(
        env, request.neighboring_content_urls());
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(
            builder,
            ad_request_builder::GetMethodId(
                ad_request_builder::kSetNeighboringContentUrls),
            url_list));
    env->DeleteLocalRef(url_list);
  }

  // Set the request agent string so requests originating from this library can
  // be tracked and reported on as a group.
  jstring agent_str = env->NewStringUTF(firebase::gma::GetRequestAgentString());
  builder = util::ContinueBuilder(
      env, builder,
      env->CallObjectMethod(
          builder,
          ad_request_builder::GetMethodId(ad_request_builder::kSetRequestAgent),
          agent_str));
  env->DeleteLocalRef(agent_str);

  // Build request and load ad.
  jobject java_request_ref = env->CallObjectMethod(
      builder, ad_request_builder::GetMethodId(ad_request_builder::kBuild));
  env->DeleteLocalRef(builder);

  return java_request_ref;
}

AdError MapAndroidAdRequestErrorCodeToCPPErrorCode(jint j_error_code) {
  // Android error code sourced from
  // https://developers.google.com/android/reference/com/google/android/gms/ads/AdRequest
  switch (j_error_code) {
    case 0:  // ERROR_CODE_INTERNAL_ERROR
      return kAdErrorInternalError;
    case 1:  // ERROR_CODE_INVALID_REQUEST
      return kAdErrorInvalidRequest;
    case 2:  // ERROR_CODE_NETWORK_ERROR
      return kAdErrorNetworkError;
    case 3:  // ERROR_CODE_NO_FILL
      return kAdErrorNoFill;
    case 8:  // ERROR_CODE_APP_ID_MISSING
      return kAdErrorApplicationIdentifierMissing;
    case 9:  // ERROR_CODE_MEDIATION_NO_FILL
      return kAdErrorMediationNoFill;
    case 10:  // ERROR_CODE_REQUEST_ID_MISMATCH
      return kAdErrorInvalidRequest;
    case 11:  // ERROR_CODE_INVALID_AD_STRING
      return kAdErrorInvalidAdString;
    default:
      return kAdErrorUnknown;
  }
}

AdError MapAndroidFullScreenContentErrorCodeToCPPErrorCode(jint j_error_code) {
  // Android error code sourced from
  // https://developers.google.com/android/reference/com/google/android/gms/ads/FullScreenContentCallback
  switch (j_error_code) {
    case 0:  // ERROR_CODE_INTERNAL_ERROR
      return kAdErrorInternalError;
    case 1:  // ERROR_CODE_AD_REUSED
      return kAdErrorAdAlreadyUsed;
    case 2:  // ERROR_CODE_NOT_READY
      return kAdErrorAdNotReady;
    case 3:  // ERROR_CODE_APP_NOT_FOREGROUND
      return kAdErrorAppNotInForeground;
    case 4:  // ERROR_CODE_MEDIATION_SHOW_ERROR
      return kAdErrorMediationShowError;
    default:
      return kAdErrorUnknown;
  }
}

}  // namespace gma
}  // namespace firebase
