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

METHOD_LOOKUP_DEFINITION(mobile_ads,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/MobileAds",
                         MOBILEADS_METHODS);


METHOD_LOOKUP_DEFINITION(
    request_config_builder,
    PROGUARD_KEEP_CLASS
    "com/google/android/gms/ads/RequestConfiguration$Builder",
    REQUESTCONFIGURATIONBUILDER_METHODS);

AdRequestConverter::AdRequestConverter(AdRequest request) {
  ConvertRequestConfiguration(request);
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject builder = env->NewObject(
      ad_request_builder::GetClass(),
      ad_request_builder::GetMethodId(ad_request_builder::kConstructor));

  // Keywords.
  for (int i = 0; i < request.keyword_count; i++) {
    jstring keyword_str = env->NewStringUTF(request.keywords[i]);
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(
            builder,
            ad_request_builder::GetMethodId(ad_request_builder::kAddKeyword),
            keyword_str));
    env->DeleteLocalRef(keyword_str);
  }

  // Network Extras.
  if (request.extras_count > 0) {
    jobject extras_bundle =
        env->NewObject(util::bundle::GetClass(),
                       util::bundle::GetMethodId(util::bundle::kConstructor));

    for (int i = 0; i < request.extras_count; i++) {
      jstring extra_key_str = env->NewStringUTF(request.extras[i].key);
      jstring extra_value_str = env->NewStringUTF(request.extras[i].value);
      env->CallVoidMethod(extras_bundle,
                          util::bundle::GetMethodId(util::bundle::kPutString),
                          extra_key_str, extra_value_str);
      env->DeleteLocalRef(extra_value_str);
      env->DeleteLocalRef(extra_key_str);
    }

    jclass admob_adapter_class =
        util::FindClass(env, admob::GetActivity(), kAdMobAdapterClassName);

    // If the adapter wasn't found correctly, the app has a serious problem.
    FIREBASE_ASSERT_MESSAGE(
        admob_adapter_class,
        "Failed to locate the AdMobAdapter class for extras. Check that "
        "com.google.ads.mediation.admob.AdMobAdapter is present in your APK.");
    if (!admob_adapter_class) {
      env->DeleteLocalRef(extras_bundle);
      return;
    }

    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(builder,
                              ad_request_builder::GetMethodId(
                                  ad_request_builder::kAddNetworkExtrasBundle),
                              admob_adapter_class, extras_bundle));

    env->DeleteLocalRef(extras_bundle);
    env->DeleteLocalRef(admob_adapter_class);
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

void AdRequestConverter::ConvertRequestConfiguration(AdRequest request) const {
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject builder = env->NewObject(request_config_builder::GetClass(),
                                   request_config_builder::GetMethodId(
                                       request_config_builder::kConstructor));

  // Child-drected treatment.
  if (request.tagged_for_child_directed_treatment !=
      kChildDirectedTreatmentStateUnknown) {
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(
            builder,
            request_config_builder::GetMethodId(
                request_config_builder::kSetTagForChildDirectedTreatment),
            (request.tagged_for_child_directed_treatment ==
             kChildDirectedTreatmentStateTagged)));
  }

  // Test DeviceIds
  if (request.test_device_id_count > 0) {
    std::vector<std::string> test_devices_vector;
    for (int i = 0; i < request.test_device_id_count; i++) {
      test_devices_vector.push_back(request.test_device_ids[i]);
    }
    jobject test_device_list =
        util::StdVectorToJavaList(env, test_devices_vector);
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(builder,
                              request_config_builder::GetMethodId(
                                  request_config_builder::kSetTestDeviceIds),
                              test_device_list));
    env->DeleteLocalRef(test_device_list);

    // Build request configuration.
    jobject request_configuration = env->CallObjectMethod(
        builder,
        request_config_builder::GetMethodId(request_config_builder::kBuild));
    env->DeleteLocalRef(builder);
    env->CallStaticVoidMethod(
        mobile_ads::GetClass(),
        mobile_ads::GetMethodId(mobile_ads::kSetRequestConfiguration),
        request_configuration);
  }
}

}  // namespace admob
}  // namespace firebase
