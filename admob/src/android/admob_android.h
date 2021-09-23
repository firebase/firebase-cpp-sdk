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

#ifndef FIREBASE_ADMOB_SRC_ANDROID_ADMOB_ANDROID_H_
#define FIREBASE_ADMOB_SRC_ANDROID_ADMOB_ANDROID_H_

#include <jni.h>

#include "app/src/util_android.h"

namespace firebase {
namespace admob {

class AdSize;

// Used to setup the cache of AdRequestBuilder class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define ADREQUESTBUILDER_METHODS(X)                                          \
  X(Constructor, "<init>", "()V"),                                           \
  X(Build, "build", "()Lcom/google/android/gms/ads/AdRequest;"),             \
  X(AddKeyword, "addKeyword",                                                \
      "(Ljava/lang/String;)Lcom/google/android/gms/ads/AdRequest$Builder;"), \
  X(AddNetworkExtrasBundle, "addNetworkExtrasBundle",                        \
      "(Ljava/lang/Class;Landroid/os/Bundle;)"                               \
      "Lcom/google/android/gms/ads/AdRequest$Builder;"),                     \
  X(SetContentUrl, "setContentUrl",                                          \
      "(Ljava/lang/String;)Lcom/google/android/gms/ads/AdRequest$Builder;"), \
  X(SetRequestAgent, "setRequestAgent",                                      \
      "(Ljava/lang/String;)Lcom/google/android/gms/ads/AdRequest$Builder;")
// clang-format on

// clang-format off
#define ADSIZE_METHODS(X)                                                    \
  X(Constructor, "<init>", "(II)V"),                                         \
  X(GetCurrentOrientationAnchoredAdaptiveBannerAdSize,                       \
    "getCurrentOrientationAnchoredAdaptiveBannerAdSize",                     \
    "(Landroid/content/Context;I)Lcom/google/android/gms/ads/AdSize;",       \
    util::kMethodTypeStatic),                                                \
  X(GetLandscapeAnchoredAdaptiveBannerAdSize,                                \
    "getLandscapeAnchoredAdaptiveBannerAdSize",                              \
    "(Landroid/content/Context;I)Lcom/google/android/gms/ads/AdSize;",       \
    util::kMethodTypeStatic),                                                \
  X(GetPortraitAnchoredAdaptiveBannerAdSize,                                 \
    "getPortraitAnchoredAdaptiveBannerAdSize",                               \
    "(Landroid/content/Context;I)Lcom/google/android/gms/ads/AdSize;",       \
    util::kMethodTypeStatic)

// clang-format on

// clang-format off
#define MOBILEADS_METHODS(X)                                                 \
  X(Initialize, "initialize",                                                \
    "(Landroid/content/Context;)V", util::kMethodTypeStatic),                \
  X(SetRequestConfiguration, "setRequestConfiguration",                      \
    "(Lcom/google/android/gms/ads/RequestConfiguration;)V",                  \
    util::kMethodTypeStatic),                                                \
  X(GetRequestConfiguration, "getRequestConfiguration",                      \
    "()Lcom/google/android/gms/ads/RequestConfiguration;",                  \
    util::kMethodTypeStatic)
// clang-format on

// clang-format off
#define REQUESTCONFIGURATIONBUILDER_METHODS(X)                               \
  X(Constructor, "<init>", "()V"),                                           \
  X(Build, "build",                                                          \
    "()Lcom/google/android/gms/ads/RequestConfiguration;"),                  \
  X(SetMaxAdContentRating, "setMaxAdContentRating",                          \
    "(Ljava/lang/String;)"                                                   \
    "Lcom/google/android/gms/ads/RequestConfiguration$Builder;"),            \
  X(SetTagForChildDirectedTreatment, "setTagForChildDirectedTreatment",      \
    "(I)Lcom/google/android/gms/ads/RequestConfiguration$Builder;"),         \
  X(SetTagForUnderAgeOfConsent, "setTagForUnderAgeOfConsent",                \
    "(I)Lcom/google/android/gms/ads/RequestConfiguration$Builder;"),         \
  X(SetTestDeviceIds, "setTestDeviceIds",                                    \
      "(Ljava/util/List;)"                                                   \
      "Lcom/google/android/gms/ads/RequestConfiguration$Builder;")
// clang-format on

// clang-format off
#define REQUESTCONFIGURATION_METHODS(X)                                      \
  X(GetMaxAdContentRating , "getMaxAdContentRating",                         \
  "()Ljava/lang/String;"),                                                   \
  X(GetTagForChildDirectedTreatment , "getTagForChildDirectedTreatment",     \
  "()I"),                                                                    \
  X(GetTagForUnderAgeOfConsent , "getTagForUnderAgeOfConsent", "()I"),       \
  X(GetTestDeviceIds , "getTestDeviceIds", "()Ljava/util/List;")


METHOD_LOOKUP_DECLARATION(ad_request_builder, ADREQUESTBUILDER_METHODS);
METHOD_LOOKUP_DECLARATION(mobile_ads, MOBILEADS_METHODS);
METHOD_LOOKUP_DECLARATION(request_config,
                          REQUESTCONFIGURATION_METHODS);
METHOD_LOOKUP_DECLARATION(request_config_builder,
                          REQUESTCONFIGURATIONBUILDER_METHODS);
METHOD_LOOKUP_DECLARATION(ad_size, ADSIZE_METHODS);

// Change codes used when receiving state change callbacks from the Java
// BannerViewHelperHelper object.
enum AdViewChangeCode {
  // The callback indicates the presentation state has changed.
  kChangePresentationState = 0,
  // The callback indicates the bounding box has changed.
  kChangeBoundingBox,
  kChangeCount
};

// Needed when AdMob is initialized without Firebase.
JNIEnv* GetJNI();

// Retrieves the activity used to initalize AdMob.
jobject GetActivity();

// Register the native callbacks needed by the Futures.
bool RegisterNatives();

// Release classes registered by this module.
void ReleaseClasses(JNIEnv* env);

// Constructs a com.google.android.gms.ads.AdSize object from a C++ AdSize
// counterpart.
jobject CreateJavaAdSize(JNIEnv* env, jobject activity,
                         const AdSize& an_ad_size);

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_ANDROID_ADMOB_ANDROID_H_
