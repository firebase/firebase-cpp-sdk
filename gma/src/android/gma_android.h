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

#ifndef FIREBASE_GMA_SRC_ANDROID_GMA_ANDROID_H_
#define FIREBASE_GMA_SRC_ANDROID_GMA_ANDROID_H_

#include <jni.h>

#include "app/src/util_android.h"
#include "gma/src/common/gma_common.h"

namespace firebase {
namespace gma {

// Used to setup the cache of class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define AD_INSPECTOR_HELPER_METHODS(X)                                       \
  X(Constructor, "<init>", "(J)V")
// clang-format on

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
  X(SetNeighboringContentUrls, "setNeighboringContentUrls",                  \
      "(Ljava/util/List;)Lcom/google/android/gms/ads/AdRequest$Builder;"),   \
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
    util::kMethodTypeStatic),                                                \
  X(GetInlineAdaptiveBannerAdSize,                                           \
    "getInlineAdaptiveBannerAdSize",                                         \
    "(II)Lcom/google/android/gms/ads/AdSize;",                               \
    util::kMethodTypeStatic),                                                \
  X(GetCurrentOrientationInlineAdaptiveBannerAdSize,                         \
    "getCurrentOrientationInlineAdaptiveBannerAdSize",                       \
    "(Landroid/content/Context;I)Lcom/google/android/gms/ads/AdSize;",       \
    util::kMethodTypeStatic),                                                \
  X(GetLandscapeInlineAdaptiveBannerAdSize,                                  \
    "getLandscapeInlineAdaptiveBannerAdSize",                                \
    "(Landroid/content/Context;I)Lcom/google/android/gms/ads/AdSize;",       \
    util::kMethodTypeStatic),                                                \
  X(GetPortraitInlineAdaptiveBannerAdSize,                                   \
    "getPortraitInlineAdaptiveBannerAdSize",                                 \
    "(Landroid/content/Context;I)Lcom/google/android/gms/ads/AdSize;",       \
    util::kMethodTypeStatic)
// clang-format on

// clang-format off
#define MOBILEADS_METHODS(X)                                                 \
  X(Initialize, "initialize",                                                \
    "(Landroid/content/Context;)V", util::kMethodTypeStatic),                \
  X(OpenAdInspector, "openAdInspector",                                      \
    "(Landroid/content/Context;"                                             \
    "Lcom/google/android/gms/ads/OnAdInspectorClosedListener;)V",            \
     util::kMethodTypeStatic),                                               \
  X(SetRequestConfiguration, "setRequestConfiguration",                      \
    "(Lcom/google/android/gms/ads/RequestConfiguration;)V",                  \
    util::kMethodTypeStatic),                                                \
  X(GetRequestConfiguration, "getRequestConfiguration",                      \
    "()Lcom/google/android/gms/ads/RequestConfiguration;",                   \
    util::kMethodTypeStatic),                                                \
  X(GetInitializationStatus, "getInitializationStatus",                      \
    "()Lcom/google/android/gms/ads/initialization/InitializationStatus;",    \
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


// clang-format off
#define INITIALIZATION_STATUS_METHODS(X)                                     \
  X(GetAdapterStatusMap, "getAdapterStatusMap", "()Ljava/util/Map;")
// clang-format on

// clang-format off
#define ADAPTER_STATUS_METHODS(X)                                            \
  X(GetDescription, "getDescription", "()Ljava/lang/String;"),               \
  X(GetLatency, "getLatency", "()I"),                                        \
  X(GetInitializationState, "getInitializationState",                        \
    "()Lcom/google/android/gms/ads/initialization/AdapterStatus$State;")
// clang-format on

// clang-format off
#define ADAPTER_STATUS_STATE_FIELDS(X)                                       \
  X(NotReady, "NOT_READY",                                                   \
    "Lcom/google/android/gms/ads/initialization/AdapterStatus$State;",       \
    util::kFieldTypeStatic),                                                 \
  X(Ready, "READY",                                                          \
    "Lcom/google/android/gms/ads/initialization/AdapterStatus$State;",       \
    util::kFieldTypeStatic)
// clang-format on

// clang-format off
#define GMA_INITIALIZATION_HELPER_METHODS(X)                                 \
  X(InitializeGma, "initializeGma", "(Landroid/content/Context;)V",          \
    util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(ad_request_builder, ADREQUESTBUILDER_METHODS);
METHOD_LOOKUP_DECLARATION(mobile_ads, MOBILEADS_METHODS);
METHOD_LOOKUP_DECLARATION(request_config, REQUESTCONFIGURATION_METHODS);
METHOD_LOOKUP_DECLARATION(request_config_builder,
                          REQUESTCONFIGURATIONBUILDER_METHODS);
METHOD_LOOKUP_DECLARATION(ad_size, ADSIZE_METHODS);
METHOD_LOOKUP_DECLARATION(initialization_status, INITIALIZATION_STATUS_METHODS);
METHOD_LOOKUP_DECLARATION(adapter_status, ADAPTER_STATUS_METHODS);
METHOD_LOOKUP_DECLARATION(adapter_status_state, METHOD_LOOKUP_NONE,
                          ADAPTER_STATUS_STATE_FIELDS);

METHOD_LOOKUP_DECLARATION(ad_inspector_helper, AD_INSPECTOR_HELPER_METHODS);
METHOD_LOOKUP_DECLARATION(gma_initialization_helper,
                          GMA_INITIALIZATION_HELPER_METHODS);

// Needed when GMA is initialized without Firebase.
JNIEnv* GetJNI();

// Retrieves the activity used to initialize GMA.
jobject GetActivity();

// Register the native callbacks needed by the Futures.
bool RegisterNatives();

// Release classes registered by this module.
void ReleaseClasses(JNIEnv* env);

// Constructs a com.google.android.gms.ads.AdSize object from a C++ AdSize
// counterpart.
jobject CreateJavaAdSize(JNIEnv* env, jobject activity,
                         const AdSize& an_ad_size);

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_GMA_ANDROID_H_
