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

#include "admob/src/android/admob_android.h"

#include <jni.h>
#include <stddef.h>

#include <cstdarg>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "admob/admob_resources.h"
#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/ad_result_android.h"
#include "admob/src/android/adapter_response_info_android.h"
#include "admob/src/android/banner_view_internal_android.h"
#include "admob/src/android/interstitial_ad_internal_android.h"
#include "admob/src/android/response_info_android.h"
#include "admob/src/android/rewarded_ad_internal_android.h"
#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"
#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/google_play_services/availability.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

METHOD_LOOKUP_DEFINITION(mobile_ads,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/MobileAds",
                         MOBILEADS_METHODS);
METHOD_LOOKUP_DEFINITION(ad_size,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/AdSize",
                         ADSIZE_METHODS);
METHOD_LOOKUP_DEFINITION(request_config,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/RequestConfiguration",
                         REQUESTCONFIGURATION_METHODS);

METHOD_LOOKUP_DEFINITION(
    request_config_builder,
    PROGUARD_KEEP_CLASS
    "com/google/android/gms/ads/RequestConfiguration$Builder",
    REQUESTCONFIGURATIONBUILDER_METHODS);

METHOD_LOOKUP_DEFINITION(
    initialization_status,
    PROGUARD_KEEP_CLASS
    "com/google/android/gms/ads/initialization/InitializationStatus",
    INITIALIZATION_STATUS_METHODS);

METHOD_LOOKUP_DEFINITION(
    adapter_status,
    PROGUARD_KEEP_CLASS
    "com/google/android/gms/ads/initialization/AdapterStatus",
    ADAPTER_STATUS_METHODS);

METHOD_LOOKUP_DEFINITION(
    adapter_status_state,
    PROGUARD_KEEP_CLASS
    "com/google/android/gms/ads/initialization/AdapterStatus$State",
    METHOD_LOOKUP_NONE, ADAPTER_STATUS_STATE_FIELDS);

METHOD_LOOKUP_DEFINITION(
    admob_initialization_helper,
    "com/google/firebase/admob/internal/cpp/AdMobInitializationHelper",
    ADMOB_INITIALIZATION_HELPER_METHODS);

// Constants representing each AdMob function that returns a Future.
enum AdMobFn {
  kAdMobFnInitialize,
  kAdMobFnCount,
};

static JavaVM* g_java_vm = nullptr;
static const ::firebase::App* g_app = nullptr;
static jobject g_activity;

static bool g_initialized = false;

static ReferenceCountedFutureImpl* g_future_impl = nullptr;
// Mutex for creation/deletion of a g_future_impl.
static Mutex g_future_impl_mutex;  // NOLINT
static SafeFutureHandle<AdapterInitializationStatus> g_initialization_handle =
    SafeFutureHandle<AdapterInitializationStatus>::kInvalidHandle;  // NOLINT

struct MobileAdsCallData {
  // Thread-safe call data.
  MobileAdsCallData() : vm(g_java_vm), activity_global(nullptr) {}
  ~MobileAdsCallData() {
    JNIEnv* env = firebase::util::GetThreadsafeJNIEnv(vm);
    env->DeleteGlobalRef(activity_global);
  }
  JavaVM* vm;
  jobject activity_global;
};

// This function is run on the main thread and is called in the
// InitializeGoogleMobileAds() function.
void CallInitializeGoogleMobileAds(void* data) {
  MobileAdsCallData* call_data = reinterpret_cast<MobileAdsCallData*>(data);
  JNIEnv* env = firebase::util::GetThreadsafeJNIEnv(call_data->vm);
  bool jni_env_exists = (env != nullptr);
  FIREBASE_ASSERT(jni_env_exists);

  jobject activity = call_data->activity_global;
  env->CallStaticVoidMethod(admob_initialization_helper::GetClass(),
                            admob_initialization_helper::GetMethodId(
                                admob_initialization_helper::kInitializeAdMob),
                            activity);
  // Check if there is a JNI exception since the MobileAds.initialize method can
  // throw an IllegalArgumentException if the pub passes null for the activity.
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  delete call_data;
}

static AdapterStatus ConvertFromJavaAdapterStatus(jobject j_adapter_status) {
  JNIEnv* env = ::firebase::admob::GetJNI();

  std::string description = util::JniStringToString(
      env, env->CallObjectMethod(
               j_adapter_status,
               adapter_status::GetMethodId(adapter_status::kGetDescription)));
  util::CheckAndClearJniExceptions(env);

  int latency = env->CallIntMethod(
      j_adapter_status,
      adapter_status::GetMethodId(adapter_status::kGetLatency));
  util::CheckAndClearJniExceptions(env);

  jobject j_state_current = env->CallObjectMethod(
      j_adapter_status,
      adapter_status::GetMethodId(adapter_status::kGetInitializationState));
  util::CheckAndClearJniExceptions(env);

  jobject j_state_ready = env->GetStaticObjectField(
      adapter_status_state::GetClass(),
      adapter_status_state::GetFieldId(adapter_status_state::kReady));
  util::CheckAndClearJniExceptions(env);

  // is_initialized = (status.getInitializationStatus() ==
  // AdapterState.State.READY)
  bool is_initialized = env->CallBooleanMethod(
      j_state_current, util::enum_class::GetMethodId(util::enum_class::kEquals),
      j_state_ready);
  util::CheckAndClearJniExceptions(env);

  env->DeleteLocalRef(j_state_current);
  env->DeleteLocalRef(j_state_ready);
  env->DeleteLocalRef(j_adapter_status);
  return AdMobInternal::CreateAdapterStatus(description, is_initialized,
                                            latency);
}

static AdapterInitializationStatus PopulateAdapterInitializationStatus(
    jobject j_init_status) {
  if (!j_init_status)
    return AdMobInternal::CreateAdapterInitializationStatus({});

  JNIEnv* env = ::firebase::admob::GetJNI();
  std::map<std::string, AdapterStatus> adapter_status_map;
  // Map<String, AdapterStatus>
  jobject j_map = env->CallObjectMethod(
      j_init_status, initialization_status::GetMethodId(
                         initialization_status::kGetAdapterStatusMap));
  util::CheckAndClearJniExceptions(env);

  // Extract keys and values from the map.
  // key_set = map.keySet();
  jobject j_key_set =
      env->CallObjectMethod(j_map, util::map::GetMethodId(util::map::kKeySet));
  util::CheckAndClearJniExceptions(env);

  // iter = key_set.iterator();
  jobject j_iter = env->CallObjectMethod(
      j_key_set, util::set::GetMethodId(util::set::kIterator));
  util::CheckAndClearJniExceptions(env);

  // while (iter.hasNext()) {
  while (env->CallBooleanMethod(
      j_iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    // adapter_name = iter.next();
    jobject j_adapter_name = env->CallObjectMethod(
        j_iter, util::iterator::GetMethodId(util::iterator::kNext));
    util::CheckAndClearJniExceptions(env);

    // adapter_status = map.get(adapter_name);
    jobject j_adapter_status = env->CallObjectMethod(
        j_map, util::map::GetMethodId(util::map::kGet), j_adapter_name);
    util::CheckAndClearJniExceptions(env);

    std::string key =
        util::JniStringToString(env, j_adapter_name);  // deletes name
    AdapterStatus value =
        ConvertFromJavaAdapterStatus(j_adapter_status);  // deletes status

    adapter_status_map[key] = value;
  }

  env->DeleteLocalRef(j_iter);
  env->DeleteLocalRef(j_key_set);
  env->DeleteLocalRef(j_map);

  return AdMobInternal::CreateAdapterInitializationStatus(adapter_status_map);
}

// Initializes the Google Mobile Ads SDK using the MobileAds.initialize()
// method. The AdMob app ID to retreived from the App's android manifest.
Future<AdapterInitializationStatus> InitializeGoogleMobileAds(JNIEnv* env) {
  Future<AdapterInitializationStatus> future_to_return;
  {
    MutexLock lock(g_future_impl_mutex);
    FIREBASE_ASSERT(g_future_impl);
    FIREBASE_ASSERT(
        g_initialization_handle.get() ==
        SafeFutureHandle<AdapterInitializationStatus>::kInvalidHandle.get());
    g_initialization_handle =
        g_future_impl->SafeAlloc<AdapterInitializationStatus>(
            kAdMobFnInitialize);
    future_to_return = MakeFuture(g_future_impl, g_initialization_handle);
  }

  MobileAdsCallData* call_data = new MobileAdsCallData();
  call_data->activity_global = env->NewGlobalRef(g_activity);
  util::RunOnMainThread(env, g_activity, CallInitializeGoogleMobileAds,
                        call_data);

  return future_to_return;
}

Future<AdapterInitializationStatus> Initialize(const ::firebase::App& app,
                                               InitResult* init_result_out) {
  FIREBASE_ASSERT(!g_initialized);
  g_app = &app;
  return Initialize(g_app->GetJNIEnv(), g_app->activity(), init_result_out);
}

Future<AdapterInitializationStatus> Initialize(JNIEnv* env, jobject activity,
                                               InitResult* init_result_out) {
  FIREBASE_ASSERT(!g_initialized);

  if (g_java_vm == nullptr) {
    env->GetJavaVM(&g_java_vm);
  }

  // AdMob requires Google Play services if the class
  // "com.google.android.gms.ads.internal.ClientApi" does not exist.
  if (!util::FindClass(env, "com/google/android/gms/ads/internal/ClientApi") &&
      google_play_services::CheckAvailability(env, activity) !=
          google_play_services::kAvailabilityAvailable) {
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    // Need to return an invalid Future, because without AdMob initialized,
    // there is no ReferenceCountedFutureImpl to hold an actual Future instance.
    return Future<AdapterInitializationStatus>();
  }

  if (!util::Initialize(env, activity)) {
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    // Need to return an invalid Future, because without AdMob initialized,
    // there is no ReferenceCountedFutureImpl to hold an actual Future instance.
    return Future<AdapterInitializationStatus>();
  }

  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(env, activity,
                               firebase::internal::EmbeddedFile::ToVector(
                                   firebase_admob::admob_resources_filename,
                                   firebase_admob::admob_resources_data,
                                   firebase_admob::admob_resources_size));

  if (!(mobile_ads::CacheMethodIds(env, activity) &&
        ad_request_builder::CacheMethodIds(env, activity) &&
        adapter_response_info::CacheMethodIds(env, activity) &&
        ad_error::CacheMethodIds(env, activity) &&
        ad_size::CacheMethodIds(env, activity) &&
        ad_view::CacheMethodIds(env, activity) &&
        request_config::CacheMethodIds(env, activity) &&
        request_config_builder::CacheMethodIds(env, activity) &&
        response_info::CacheMethodIds(env, activity) &&
        adapter_status::CacheMethodIds(env, activity) &&
        adapter_status_state::CacheFieldIds(env, activity) &&
        initialization_status::CacheMethodIds(env, activity) &&
        admob_initialization_helper::CacheClassFromFiles(
            env, activity, &embedded_files) != nullptr &&
        admob_initialization_helper::CacheMethodIds(env, activity) &&
        banner_view_helper::CacheClassFromFiles(env, activity,
                                                &embedded_files) != nullptr &&
        banner_view_helper::CacheMethodIds(env, activity) &&
        banner_view_helper_ad_view_listener::CacheMethodIds(env, activity) &&
        interstitial_ad_helper::CacheClassFromFiles(
            env, activity, &embedded_files) != nullptr &&
        interstitial_ad_helper::CacheMethodIds(env, activity) &&
        rewarded_ad_helper::CacheClassFromFiles(env, activity,
                                                &embedded_files) != nullptr &&
        rewarded_ad_helper::CacheMethodIds(env, activity) &&
        load_ad_error::CacheMethodIds(env, activity) &&
        admob::RegisterNatives())) {
    ReleaseClasses(env);
    util::Terminate(env);
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Future<AdapterInitializationStatus>();
  }

  {
    MutexLock lock(g_future_impl_mutex);
    g_future_impl = new ReferenceCountedFutureImpl(kAdMobFnCount);
  }

  g_initialized = true;
  g_activity = env->NewGlobalRef(activity);

  Future<AdapterInitializationStatus> future = InitializeGoogleMobileAds(env);
  RegisterTerminateOnDefaultAppDestroy();

  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return future;
}

Future<AdapterInitializationStatus> InitializeLastResult() {
  MutexLock lock(g_future_impl_mutex);
  return g_future_impl
             ? static_cast<const Future<AdapterInitializationStatus>&>(
                   g_future_impl->LastResult(kAdMobFnInitialize))
             : Future<AdapterInitializationStatus>();
}

AdapterInitializationStatus GetInitializationStatus() {
  if (g_initialized) {
    JNIEnv* env = ::firebase::admob::GetJNI();
    jobject j_status = env->CallStaticObjectMethod(
        mobile_ads::GetClass(),
        mobile_ads::GetMethodId(mobile_ads::kGetInitializationStatus));
    util::CheckAndClearJniExceptions(env);
    AdapterInitializationStatus status =
        PopulateAdapterInitializationStatus(j_status);
    env->DeleteLocalRef(j_status);
    return status;
  } else {
    // Returns an empty map.
    return PopulateAdapterInitializationStatus(nullptr);
  }
}

void DisableSDKCrashReporting() {}

void DisableMediationInitialization() {}

void SetRequestConfiguration(
    const RequestConfiguration& request_configuration) {
  JNIEnv* env = ::firebase::admob::GetJNI();
  jobject builder = env->NewObject(request_config_builder::GetClass(),
                                   request_config_builder::GetMethodId(
                                       request_config_builder::kConstructor));
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  // Test DeviceIds
  if (request_configuration.test_device_ids.size() > 0) {
    jobject test_device_list =
        util::StdVectorToJavaList(env, request_configuration.test_device_ids);
    builder = util::ContinueBuilder(
        env, builder,
        env->CallObjectMethod(builder,
                              request_config_builder::GetMethodId(
                                  request_config_builder::kSetTestDeviceIds),
                              test_device_list));
    jni_exception = util::CheckAndClearJniExceptions(env);
    FIREBASE_ASSERT(!jni_exception);
    env->DeleteLocalRef(test_device_list);
  }

  jstring j_string_max_ad_rating = nullptr;
  switch (request_configuration.max_ad_content_rating) {
    case RequestConfiguration::kMaxAdContentRatingG:
      j_string_max_ad_rating = env->NewStringUTF("G");
      break;
    case RequestConfiguration::kMaxAdContentRatingPG:
      j_string_max_ad_rating = env->NewStringUTF("PG");
      break;
    case RequestConfiguration::kMaxAdContentRatingT:
      j_string_max_ad_rating = env->NewStringUTF("T");
      break;
    case RequestConfiguration::kMaxAdContentRatingMA:
      j_string_max_ad_rating = env->NewStringUTF("MA");
      break;
    case RequestConfiguration::kMaxAdContentRatingUnspecified:
    default:
      j_string_max_ad_rating = env->NewStringUTF("");
      break;
  }
  builder = util::ContinueBuilder(
      env, builder,
      env->CallObjectMethod(builder,
                            request_config_builder::GetMethodId(
                                request_config_builder::kSetMaxAdContentRating),
                            j_string_max_ad_rating));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  env->DeleteLocalRef(j_string_max_ad_rating);

  int child_directed_treatment_tag;
  switch (request_configuration.tag_for_child_directed_treatment) {
    case RequestConfiguration::kChildDirectedTreatmentFalse:
      child_directed_treatment_tag = 0;
      break;
    case RequestConfiguration::kChildDirectedTreatmentTrue:
      child_directed_treatment_tag = 1;
      break;
    default:
    case RequestConfiguration::kChildDirectedTreatmentUnspecified:
      child_directed_treatment_tag = -1;
      break;
  }
  builder = util::ContinueBuilder(
      env, builder,
      env->CallObjectMethod(
          builder,
          request_config_builder::GetMethodId(
              request_config_builder::kSetTagForChildDirectedTreatment),
          child_directed_treatment_tag));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  int under_age_of_consent_tag;
  switch (request_configuration.tag_for_under_age_of_consent) {
    case RequestConfiguration::kUnderAgeOfConsentFalse:
      under_age_of_consent_tag = 0;
      break;
    case RequestConfiguration::kUnderAgeOfConsentTrue:
      under_age_of_consent_tag = 1;
      break;
    default:
    case RequestConfiguration::kUnderAgeOfConsentUnspecified:
      under_age_of_consent_tag = -1;
      break;
  }
  builder = util::ContinueBuilder(
      env, builder,
      env->CallObjectMethod(
          builder,
          request_config_builder::GetMethodId(
              request_config_builder::kSetTagForUnderAgeOfConsent),
          under_age_of_consent_tag));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  // Build request configuration.
  jobject j_request_configuration = env->CallObjectMethod(
      builder,
      request_config_builder::GetMethodId(request_config_builder::kBuild));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  env->DeleteLocalRef(builder);

  // Set the request configuration.
  env->CallStaticVoidMethod(
      mobile_ads::GetClass(),
      mobile_ads::GetMethodId(mobile_ads::kSetRequestConfiguration),
      j_request_configuration);

  env->DeleteLocalRef(j_request_configuration);
}

RequestConfiguration GetRequestConfiguration() {
  JNIEnv* env = ::firebase::admob::GetJNI();
  RequestConfiguration request_configuration;
  jobject j_request_config = env->CallStaticObjectMethod(
      mobile_ads::GetClass(),
      mobile_ads::GetMethodId(mobile_ads::kGetRequestConfiguration));
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(j_request_config != nullptr);

  // Max Ad Content Rating.
  const jstring j_max_ad_content_rating =
      static_cast<jstring>(env->CallObjectMethod(
          j_request_config,
          request_config::GetMethodId(request_config::kGetMaxAdContentRating)));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(j_max_ad_content_rating != nullptr);
  const std::string max_ad_content_rating =
      env->GetStringUTFChars(j_max_ad_content_rating, nullptr);
  FIREBASE_ASSERT(!jni_exception);
  if (max_ad_content_rating == "G") {
    request_configuration.max_ad_content_rating =
        RequestConfiguration::kMaxAdContentRatingG;
  } else if (max_ad_content_rating == "PG") {
    request_configuration.max_ad_content_rating =
        RequestConfiguration::kMaxAdContentRatingPG;
  } else if (max_ad_content_rating == "MA") {
    request_configuration.max_ad_content_rating =
        RequestConfiguration::kMaxAdContentRatingMA;
  } else if (max_ad_content_rating == "T") {
    request_configuration.max_ad_content_rating =
        RequestConfiguration::kMaxAdContentRatingT;
  } else if (max_ad_content_rating == "") {
    request_configuration.max_ad_content_rating =
        RequestConfiguration::kMaxAdContentRatingUnspecified;
  } else {
    FIREBASE_ASSERT_MESSAGE(false,
                            "RequestConfiguration unknown MaxAdContentRating");
  }
  env->DeleteLocalRef(j_max_ad_content_rating);

  // Tag For Child Directed Treatment
  const jint j_child_directed_treatment_tag = env->CallIntMethod(
      j_request_config, request_config::GetMethodId(
                            request_config::kGetTagForChildDirectedTreatment));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  switch (j_child_directed_treatment_tag) {
    case -1:
      request_configuration.tag_for_child_directed_treatment =
          RequestConfiguration::kChildDirectedTreatmentUnspecified;
      break;
    case 0:
      request_configuration.tag_for_child_directed_treatment =
          RequestConfiguration::kChildDirectedTreatmentFalse;
      break;
    case 1:
      request_configuration.tag_for_child_directed_treatment =
          RequestConfiguration::kChildDirectedTreatmentTrue;
      break;
    default:
      FIREBASE_ASSERT_MESSAGE(
          false, "RequestConfiguration unknown TagForChildDirectedTreatment");
  }

  // Tag For Under Age Of Consent
  const jint j_under_age_of_consent_tag = env->CallIntMethod(
      j_request_config,
      request_config::GetMethodId(request_config::kGetTagForUnderAgeOfConsent));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  switch (j_under_age_of_consent_tag) {
    case -1:
      request_configuration.tag_for_under_age_of_consent =
          RequestConfiguration::kUnderAgeOfConsentUnspecified;
      break;
    case 0:
      request_configuration.tag_for_under_age_of_consent =
          RequestConfiguration::kUnderAgeOfConsentFalse;
      break;
    case 1:
      request_configuration.tag_for_under_age_of_consent =
          RequestConfiguration::kUnderAgeOfConsentTrue;
      break;
    default:
      FIREBASE_ASSERT_MESSAGE(
          false, "RequestConfiguration unknown TagForUnderAgeOfConsent");
  }

  // Test Device Ids
  const jobject j_test_device_id_list = env->CallObjectMethod(
      j_request_config,
      request_config::GetMethodId(request_config::kGetTestDeviceIds));
  jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(j_test_device_id_list != nullptr);
  util::JavaListToStdStringVector(env, &request_configuration.test_device_ids,
                                  j_test_device_id_list);
  env->DeleteLocalRef(j_test_device_id_list);

  return request_configuration;
}

// Release classes registered by this module.
void ReleaseClasses(JNIEnv* env) {
  mobile_ads::ReleaseClass(env);
  ad_request_builder::ReleaseClass(env);
  adapter_response_info::ReleaseClass(env);
  ad_error::ReleaseClass(env);
  ad_size::ReleaseClass(env);
  ad_view::ReleaseClass(env);
  request_config::ReleaseClass(env);
  request_config_builder::ReleaseClass(env);
  response_info::ReleaseClass(env);
  adapter_status::ReleaseClass(env);
  adapter_status_state::ReleaseClass(env);
  initialization_status::ReleaseClass(env);
  admob_initialization_helper::ReleaseClass(env);
  banner_view_helper::ReleaseClass(env);
  banner_view_helper_ad_view_listener::ReleaseClass(env);
  interstitial_ad_helper::ReleaseClass(env);
  rewarded_ad_helper::ReleaseClass(env);
  load_ad_error::ReleaseClass(env);
}

bool IsInitialized() { return g_initialized; }

void Terminate() {
  if (!g_initialized) {
    LogWarning("AdMob already shut down");
    return;
  }
  {
    MutexLock lock(g_future_impl_mutex);
    g_initialization_handle =
        SafeFutureHandle<AdapterInitializationStatus>::kInvalidHandle;
    delete g_future_impl;
    g_future_impl = nullptr;
  }
  UnregisterTerminateOnDefaultAppDestroy();
  DestroyCleanupNotifier();
  FIREBASE_ASSERT(g_activity);
  JNIEnv* env = GetJNI();
  g_initialized = false;
  g_app = nullptr;
  g_java_vm = nullptr;
  env->DeleteGlobalRef(g_activity);
  g_activity = nullptr;

  ReleaseClasses(env);
  util::Terminate(env);
}

const ::firebase::App* GetApp() {
  FIREBASE_ASSERT(g_app);
  return g_app;
}

JNIEnv* GetJNI() {
  if (g_app) {
    return g_app->GetJNIEnv();
  } else {
    FIREBASE_ASSERT(g_java_vm);
    return firebase::util::GetThreadsafeJNIEnv(g_java_vm);
  }
}

jobject GetActivity() { return (g_app) ? g_app->activity() : g_activity; }

static void CompleteAdFutureCallback(JNIEnv* env, jclass clazz, jlong data_ptr,
                                     jint error_code, jstring error_message) {
  if (data_ptr == 0) return;  // test call only

  const char* error_msg = env->GetStringUTFChars(error_message, nullptr);

  firebase::admob::FutureCallbackData<void>* callback_data =
      reinterpret_cast<firebase::admob::FutureCallbackData<void>*>(data_ptr);

  callback_data->future_data->future_impl.Complete(
      callback_data->future_handle, static_cast<int>(error_code), error_msg);

  env->ReleaseStringUTFChars(error_message, error_msg);

  // This method is responsible for disposing of the callback data struct.
  delete callback_data;
}

void CompleteLoadAdCallback(FutureCallbackData<AdResult>* callback_data,
                            jobject j_load_ad_error, AdMobError error_code,
                            const std::string& error_message) {
  FIREBASE_ASSERT(callback_data);

  std::string future_error_message;
  AdResultInternal ad_result_internal;

  ad_result_internal.native_ad_error = j_load_ad_error;
  ad_result_internal.ad_result_type =
      AdResultInternal::kAdResultInternalLoadAdError;
  ad_result_internal.is_successful = true;  // assume until proven otherwise.
  ad_result_internal.code = error_code;

  // Further result configuration is based on success/failure.
  if (j_load_ad_error != nullptr) {
    // The Android SDK returned an error.  Use the native_ad_error object
    // to populate a AdResult with the error specifics.
    ad_result_internal.is_successful = false;
  } else if (ad_result_internal.code != kAdMobErrorNone) {
    // C++ SDK Android AdMob Wrapper encountered an error.
    ad_result_internal.ad_result_type =
        AdResultInternal::kAdResultInternalWrapperError;
    ad_result_internal.is_successful = false;
    ad_result_internal.message = error_message;
    ad_result_internal.domain = "SDK";
    ad_result_internal.to_string =
        std::string("Internal error: ") + ad_result_internal.message;
    future_error_message = ad_result_internal.message;
  }

  // Invoke a friend of AdResult to have it invoke the AdResult
  // protected constructor with the AdResultInternal data.
  AdMobInternal::CompleteLoadAdFuture(callback_data, ad_result_internal.code,
                                      future_error_message, ad_result_internal);
}

void CompleteLoadAdAndroidErrorResult(JNIEnv* env, jlong data_ptr,
                                      jobject j_load_ad_error,
                                      AdMobError error_code,
                                      jstring j_error_message) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  FIREBASE_ASSERT(j_error_message);

  FutureCallbackData<AdResult>* callback_data =
      reinterpret_cast<firebase::admob::FutureCallbackData<AdResult>*>(
          data_ptr);

  std::string error_message = util::JStringToString(env, j_error_message);

  CompleteLoadAdCallback(callback_data, j_load_ad_error, error_code,
                         error_message);
}

void CompleteLoadAdInternalResult(FutureCallbackData<AdResult>* callback_data,
                                  AdMobError error_code,
                                  const char* error_message) {
  FIREBASE_ASSERT(callback_data);
  FIREBASE_ASSERT(error_message);

  CompleteLoadAdCallback(callback_data, /*j_load_ad_error=*/nullptr, error_code,
                         error_message);
}

AdValue::PrecisionType ConvertAndroidPrecisionTypeToCPPPrecisionType(
    const jint j_precision_type) {
  // Values taken from:
  // https://firebase.google.com/docs/reference/android/com/google/android/gms/ads/AdValue.PrecisionType
  switch (j_precision_type) {
    default:
      LogWarning("Could not convert AdValue precisionType: %l",
                 j_precision_type);
    case 0:
      return AdValue::kdValuePrecisionUnknown;
    case 1:  // ESTIMATED
      return AdValue::kAdValuePrecisionEstimated;
    case 2:  // PUBLISHER_PROVIDED
      return AdValue::kAdValuePrecisionPublisherProvided;
    case 3:  // PRECISE
      return AdValue::kAdValuePrecisionPrecise;
  }
}

static void JNICALL AdMobInitializationHelper_initializationCompleteCallback(
    JNIEnv* env, jclass clazz, jobject j_initialization_status) {
  AdapterInitializationStatus adapter_status =
      PopulateAdapterInitializationStatus(j_initialization_status);
  {
    MutexLock lock(g_future_impl_mutex);
    // Check if g_future_impl still exists; if not, Terminate() was called,
    // ignore the result of this callback.
    if (g_future_impl &&
        g_initialization_handle.get() !=
            SafeFutureHandle<AdapterInitializationStatus>::kInvalidHandle
                .get()) {
      g_future_impl->CompleteWithResult(g_initialization_handle, 0, "",
                                        adapter_status);
      g_initialization_handle =
          SafeFutureHandle<AdapterInitializationStatus>::kInvalidHandle;
    }
  }
}

namespace {

// Common JNI methods
//
void JNI_completeAdFutureCallback(JNIEnv* env, jclass clazz, jlong data_ptr,
                                  jint error_code, jstring error_message) {
  CompleteAdFutureCallback(env, clazz, data_ptr, error_code, error_message);
}

void JNI_completeLoadedAd(JNIEnv* env, jclass clazz, jlong data_ptr) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  FutureCallbackData<AdResult>* callback_data =
      reinterpret_cast<FutureCallbackData<AdResult>*>(data_ptr);
  CompleteLoadAdInternalResult(callback_data, kAdMobErrorNone,
                               /*error_message=*/"");
}

void JNI_completeLoadAdError(JNIEnv* env, jclass clazz, jlong data_ptr,
                             jobject j_load_ad_error, jint j_error_code,
                             jstring j_error_message) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  FIREBASE_ASSERT(j_error_message);
  const AdMobError error_code =
      MapAndroidAdRequestErrorCodeToCPPErrorCode(j_error_code);
  CompleteLoadAdAndroidErrorResult(env, data_ptr, j_load_ad_error, error_code,
                                   j_error_message);
}

// Internal Errors use AdMobError codes.
void JNI_completeLoadAdInternalError(JNIEnv* env, jclass clazz, jlong data_ptr,
                                     jint j_error_code,
                                     jstring j_error_message) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  FIREBASE_ASSERT(j_error_message);
  const AdMobError error_code = static_cast<AdMobError>(j_error_code);
  CompleteLoadAdAndroidErrorResult(env, data_ptr, /*j_load_ad_error=*/nullptr,
                                   error_code, j_error_message);
}

void JNI_notifyAdClickedFullScreenContentEvent(JNIEnv* env, jclass clazz,
                                               jlong data_ptr) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  internal::FullScreenAdEventListener* listener =
      reinterpret_cast<internal::FullScreenAdEventListener*>(data_ptr);
  listener->NotifyListenerOfAdClickedFullScreenContent();
}

void JNI_notifyAdDismissedFullScreenContentEvent(JNIEnv* env, jclass clazz,
                                                 jlong data_ptr) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  internal::FullScreenAdEventListener* listener =
      reinterpret_cast<internal::FullScreenAdEventListener*>(data_ptr);
  listener->NotifyListenerOfAdDismissedFullScreenContent();
}

void JNI_notifyAdFailedToShowFullScreenContentEvent(JNIEnv* env, jclass clazz,
                                                    jlong data_ptr,
                                                    jobject j_ad_error) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  FIREBASE_ASSERT(j_ad_error);
  internal::FullScreenAdEventListener* listener =
      reinterpret_cast<internal::FullScreenAdEventListener*>(data_ptr);
  AdResultInternal ad_result_internal;
  ad_result_internal.ad_result_type =
      AdResultInternal::kAdResultInternalFullScreenContentError;
  ad_result_internal.native_ad_error = j_ad_error;

  // Invoke AdMobInternal, a friend of AdResult, to have it access its
  // protected constructor with the AdError data.
  const AdResult& ad_result = AdMobInternal::CreateAdResult(ad_result_internal);
  listener->NotifyListenerOfAdFailedToShowFullScreenContent(ad_result);
}

void JNI_notifyAdImpressionEvent(JNIEnv* env, jclass clazz, jlong data_ptr) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  internal::FullScreenAdEventListener* listener =
      reinterpret_cast<internal::FullScreenAdEventListener*>(data_ptr);
  listener->NotifyListenerOfAdImpression();
}

void JNI_notifyAdShowedFullScreenContentEvent(JNIEnv* env, jclass clazz,
                                              jlong data_ptr) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  internal::FullScreenAdEventListener* listener =
      reinterpret_cast<internal::FullScreenAdEventListener*>(data_ptr);
  listener->NotifyListenerOfAdShowedFullScreenContent();
}

void JNI_notifyAdPaidEvent(JNIEnv* env, jclass clazz, jlong data_ptr,
                           jstring j_currency_code, jint j_precision_type,
                           jlong j_value_micros) {
  FIREBASE_ASSERT(data_ptr);
  internal::FullScreenAdEventListener* listener =
      reinterpret_cast<internal::FullScreenAdEventListener*>(data_ptr);

  const char* currency_code = env->GetStringUTFChars(j_currency_code, nullptr);
  const AdValue::PrecisionType precision_type =
      ConvertAndroidPrecisionTypeToCPPPrecisionType(j_precision_type);
  AdValue ad_value(currency_code, precision_type, (int64_t)j_value_micros);
  listener->NotifyListenerOfPaidEvent(ad_value);
}

// JNI functions specific to BannerViews
//
void JNI_BannerViewHelper_notifyBoundingBoxChanged(JNIEnv* env, jclass clazz,
                                                   jlong data_ptr) {
  firebase::admob::internal::BannerViewInternal* internal =
      reinterpret_cast<firebase::admob::internal::BannerViewInternal*>(
          data_ptr);
  internal->NotifyListenerOfBoundingBoxChange(internal->bounding_box());
}

void JNI_BannerViewHelper_notifyAdClicked(JNIEnv* env, jclass clazz,
                                          jlong data_ptr) {
  firebase::admob::internal::BannerViewInternal* internal =
      reinterpret_cast<firebase::admob::internal::BannerViewInternal*>(
          data_ptr);
  internal->NotifyListenerAdClicked();
}

void JNI_BannerViewHelper_notifyAdClosed(JNIEnv* env, jclass clazz,
                                         jlong data_ptr) {
  firebase::admob::internal::BannerViewInternal* internal =
      reinterpret_cast<firebase::admob::internal::BannerViewInternal*>(
          data_ptr);
  internal->NotifyListenerAdClosed();
}

void JNI_BannerViewHelper_notifyAdImpression(JNIEnv* env, jclass clazz,
                                             jlong data_ptr) {
  firebase::admob::internal::BannerViewInternal* internal =
      reinterpret_cast<firebase::admob::internal::BannerViewInternal*>(
          data_ptr);
  internal->NotifyListenerAdImpression();
}

void JNI_BannerViewHelper_notifyAdOpened(JNIEnv* env, jclass clazz,
                                         jlong data_ptr) {
  firebase::admob::internal::BannerViewInternal* internal =
      reinterpret_cast<firebase::admob::internal::BannerViewInternal*>(
          data_ptr);
  internal->NotifyListenerAdOpened();
}

void JNI_BannerViewHelper_notifyAdPaidEvent(JNIEnv* env, jclass clazz,
                                            jlong data_ptr,
                                            jstring j_currency_code,
                                            jint j_precision_type,
                                            jlong j_value_micros) {
  FIREBASE_ASSERT(data_ptr);
  internal::BannerViewInternal* internal =
      reinterpret_cast<internal::BannerViewInternal*>(data_ptr);

  const char* currency_code = env->GetStringUTFChars(j_currency_code, nullptr);
  const AdValue::PrecisionType precision_type =
      ConvertAndroidPrecisionTypeToCPPPrecisionType(j_precision_type);
  AdValue ad_value(currency_code, precision_type, (int64_t)j_value_micros);
  internal->NotifyListenerOfPaidEvent(ad_value);
}

// JNI functions specific to RewardedAds
//
void JNI_RewardedAd_UserEarnedReward(JNIEnv* env, jclass clazz, jlong data_ptr,
                                     jstring reward_type, jint amount) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(data_ptr);
  internal::RewardedAdInternal* internal =
      reinterpret_cast<internal::RewardedAdInternal*>(data_ptr);
  internal->NotifyListenerOfUserEarnedReward(
      util::JStringToString(env, reward_type), (int64_t)amount);
}

}  // namespace

bool RegisterNatives() {
  static const JNINativeMethod kBannerMethods[] = {
      {"completeBannerViewFutureCallback", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeAdFutureCallback)},
      {"completeBannerViewLoadedAd", "(J)V",
       reinterpret_cast<void*>(&JNI_completeLoadedAd)},
      {"completeBannerViewLoadAdError",
       "(JLcom/google/android/gms/ads/LoadAdError;ILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeLoadAdError)},
      {"completeBannerViewLoadAdInternalError", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeLoadAdInternalError)},
      {"notifyBoundingBoxChanged", "(J)V",
       reinterpret_cast<void*>(&JNI_BannerViewHelper_notifyBoundingBoxChanged)},
      {"notifyAdClicked", "(J)V",
       reinterpret_cast<void*>(&JNI_BannerViewHelper_notifyAdClicked)},
      {"notifyAdClosed", "(J)V",
       reinterpret_cast<void*>(&JNI_BannerViewHelper_notifyAdClosed)},
      {"notifyAdImpression", "(J)V",
       reinterpret_cast<void*>(&JNI_BannerViewHelper_notifyAdImpression)},
      {"notifyAdOpened", "(J)V",
       reinterpret_cast<void*>(&JNI_BannerViewHelper_notifyAdOpened)},
      {"notifyPaidEvent", "(JLjava/lang/String;IJ)V",
       reinterpret_cast<void*>(&JNI_BannerViewHelper_notifyAdPaidEvent)},
  };
  static const JNINativeMethod kInterstitialMethods[] = {
      {"completeInterstitialAdFutureCallback", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeAdFutureCallback)},
      {"completeInterstitialLoadedAd", "(J)V",
       reinterpret_cast<void*>(&JNI_completeLoadedAd)},
      {"completeInterstitialLoadAdError",
       "(JLcom/google/android/gms/ads/LoadAdError;ILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeLoadAdError)},
      {"completeInterstitialLoadAdInternalError", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeLoadAdInternalError)},
      {"notifyAdClickedFullScreenContentEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdClickedFullScreenContentEvent)},
      {"notifyAdDismissedFullScreenContentEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdDismissedFullScreenContentEvent)},
      {"notifyAdFailedToShowFullScreenContentEvent",
       "(JLcom/google/android/gms/ads/AdError;)V",
       reinterpret_cast<void*>(
           &JNI_notifyAdFailedToShowFullScreenContentEvent)},
      {"notifyAdImpressionEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdImpressionEvent)},
      {"notifyAdShowedFullScreenContentEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdShowedFullScreenContentEvent)},
      {"notifyPaidEvent", "(JLjava/lang/String;IJ)V",
       reinterpret_cast<void*>(&JNI_notifyAdPaidEvent)},
  };

  static const JNINativeMethod kRewardedAdMethods[] = {
      {"completeRewardedAdFutureCallback", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeAdFutureCallback)},
      {"completeRewardedLoadedAd", "(J)V",
       reinterpret_cast<void*>(&JNI_completeLoadedAd)},
      {"completeRewardedLoadAdError",
       "(JLcom/google/android/gms/ads/LoadAdError;ILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeLoadAdError)},
      {"completeRewardedLoadAdInternalError", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_completeLoadAdInternalError)},
      {"notifyUserEarnedRewardEvent", "(JLjava/lang/String;I)V",
       reinterpret_cast<void*>(&JNI_RewardedAd_UserEarnedReward)},
      {"notifyAdClickedFullScreenContentEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdClickedFullScreenContentEvent)},
      {"notifyAdDismissedFullScreenContentEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdDismissedFullScreenContentEvent)},
      {"notifyAdFailedToShowFullScreenContentEvent",
       "(JLcom/google/android/gms/ads/AdError;)V",
       reinterpret_cast<void*>(
           &JNI_notifyAdFailedToShowFullScreenContentEvent)},
      {"notifyAdImpressionEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdImpressionEvent)},
      {"notifyAdShowedFullScreenContentEvent", "(J)V",
       reinterpret_cast<void*>(&JNI_notifyAdShowedFullScreenContentEvent)},
      {"notifyPaidEvent", "(JLjava/lang/String;IJ)V",
       reinterpret_cast<void*>(&JNI_notifyAdPaidEvent)},
  };
  static const JNINativeMethod kAdMobInitializationMethods[] = {
      {"initializationCompleteCallback",
       "(Lcom/google/android/gms/ads/initialization/InitializationStatus;)V",
       reinterpret_cast<void*>(
           &AdMobInitializationHelper_initializationCompleteCallback)}  // NOLINT
  };

  JNIEnv* env = GetJNI();
  return banner_view_helper::RegisterNatives(
             env, kBannerMethods, FIREBASE_ARRAYSIZE(kBannerMethods)) &&
         interstitial_ad_helper::RegisterNatives(
             env, kInterstitialMethods,
             FIREBASE_ARRAYSIZE(kInterstitialMethods)) &&
         rewarded_ad_helper::RegisterNatives(
             env, kRewardedAdMethods, FIREBASE_ARRAYSIZE(kRewardedAdMethods)) &&
         admob_initialization_helper::RegisterNatives(
             env, kAdMobInitializationMethods,
             FIREBASE_ARRAYSIZE(kAdMobInitializationMethods));
}

jobject CreateJavaAdSize(JNIEnv* env, jobject j_activity,
                         const AdSize& adsize) {
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(j_activity);

  jobject j_ad_size = nullptr;
  switch (adsize.type()) {
    case AdSize::kTypeAnchoredAdaptive:
      switch (adsize.orientation()) {
        case AdSize::kOrientationLandscape:
          j_ad_size = env->CallStaticObjectMethod(
              ad_size::GetClass(),
              ad_size::GetMethodId(
                  ad_size::kGetCurrentOrientationAnchoredAdaptiveBannerAdSize),
              j_activity, adsize.width());
          break;
        case AdSize::kOrientationPortrait:
          j_ad_size = env->CallStaticObjectMethod(
              ad_size::GetClass(),
              ad_size::GetMethodId(
                  ad_size::kGetLandscapeAnchoredAdaptiveBannerAdSize),
              j_activity, adsize.width());
          break;
        case AdSize::kOrientationCurrent:
          j_ad_size = env->CallStaticObjectMethod(
              ad_size::GetClass(),
              ad_size::GetMethodId(
                  ad_size::kGetCurrentOrientationAnchoredAdaptiveBannerAdSize),
              j_activity, adsize.width());

        default:
          FIREBASE_ASSERT_MESSAGE(true, "Uknown AdSize Orientation");
      }
    case AdSize::kTypeStandard:
      j_ad_size = env->NewObject(ad_size::GetClass(),
                                 ad_size::GetMethodId(ad_size::kConstructor),
                                 adsize.width(), adsize.height());
      break;
    default:
      FIREBASE_ASSERT_MESSAGE(true, "Uknown AdSize Type");
  }
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(j_ad_size);
  return j_ad_size;
}

}  // namespace admob
}  // namespace firebase
