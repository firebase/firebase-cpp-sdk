/*
 * Copyright 2016 Google LLC
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

#include "admob/src/android/ad_request_converter.h"

#include <assert.h>
#include <jni.h>

#include <cstdarg>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "admob/admob_resources.h"
#include "admob/src/android/admob_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/log.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

static const char* kAdMobAdapterClassName =
    "com/google/ads/mediation/admob/AdMobAdapter";

METHOD_LOOKUP_DEFINITION(ad_request_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdRequest$Builder",
                         ADREQUESTBUILDER_METHODS);

METHOD_LOOKUP_DEFINITION(
    request_config_builder,
    PROGUARD_KEEP_CLASS
    "com/google/android/gms/ads/RequestConfiguration$Builder",
    REQUESTCONFIGURATIONBUILDER_METHODS);

AdRequestConverter::AdRequestConverter(const AdRequest& request) {
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject builder = env->NewObject(
      ad_request_builder::GetClass(),
      ad_request_builder::GetMethodId(ad_request_builder::kConstructor));

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

  // Network Extras.
  const std::map<std::string, std::map<std::string, std::string>>& extras =
      request.extras();
  for (auto adapter_iter = extras.begin(); adapter_iter != extras.end();
       ++adapter_iter) {
    std::string adapter_name = adapter_iter->first;

    jclass adapter_class = util::FindClass(env, adapter_name.c_str());
    // If the adapter wasn't found correctly, the app has a serious problem.
    FIREBASE_ASSERT_MESSAGE(
        adapter_class,
        "Failed to locate adapter class for extras. Check that \"%s\""
        " is present in your APK.",
        adapter_name.c_str());
    continue;

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

  // Set the request agent string so requests originating from this library can
  // be tracked and reported on as a group.
  jstring agent_str =
      env->NewStringUTF(firebase::admob::GetRequestAgentString());
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

  java_request_ = env->NewGlobalRef(java_request_ref);
  env->DeleteLocalRef(java_request_ref);
}

AdRequestConverter::~AdRequestConverter() {
  JNIEnv* env = ::firebase::admob::GetJNI();
  env->DeleteGlobalRef(java_request_);
}

jobject AdRequestConverter::GetJavaRequestObject() { return java_request_; }

}  // namespace admob
}  // namespace firebase
