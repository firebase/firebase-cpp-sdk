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

#include "admob/admob_resources.h"
#include "admob/src/android/ad_request_converter.h"
#include "admob/src/android/banner_view_internal_android.h"
#include "admob/src/android/interstitial_ad_internal_android.h"
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

static JavaVM* g_java_vm = nullptr;
static const ::firebase::App* g_app = nullptr;
static jobject g_activity;

bool g_initialized = false;

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
  env->CallStaticVoidMethod(mobile_ads::GetClass(),
                            mobile_ads::GetMethodId(mobile_ads::kInitialize),
                            activity);
  // Check if there is a JNI exception since the MobileAds.initialize method can
  // throw an IllegalArgumentException if the pub passes null for the activity.
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);

  delete call_data;
}

// Initializes the Google Mobile Ads SDK using the MobileAds.initialize()
// method. The AdMob app ID to retreived from the App's android manifest.
void InitializeGoogleMobileAds(JNIEnv* env) {
  MobileAdsCallData* call_data = new MobileAdsCallData();
  call_data->activity_global = env->NewGlobalRef(g_activity);
  util::RunOnMainThread(env, g_activity, CallInitializeGoogleMobileAds,
                        call_data);
}

InitResult Initialize(const ::firebase::App& app) {
  if (g_initialized) {
    LogWarning("AdMob is already initialized.");
    return kInitResultSuccess;
  }
  g_app = &app;
  return Initialize(g_app->GetJNIEnv(), g_app->activity());
}

InitResult Initialize(JNIEnv* env, jobject activity) {
  if (g_java_vm == nullptr) {
    env->GetJavaVM(&g_java_vm);
  }

  // AdMob requires Google Play services if the class
  // "com.google.android.gms.ads.internal.ClientApi" does not exist.
  if (!util::FindClass(env, "com/google/android/gms/ads/internal/ClientApi") &&
      google_play_services::CheckAvailability(env, activity) !=
          google_play_services::kAvailabilityAvailable) {
    return kInitResultFailedMissingDependency;
  }

  if (g_initialized) {
    LogWarning("AdMob is already initialized.");
    return kInitResultSuccess;
  }

  if (!util::Initialize(env, activity)) {
    return kInitResultFailedMissingDependency;
  }

  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(env, activity,
                               firebase::internal::EmbeddedFile::ToVector(
                                   firebase_admob::admob_resources_filename,
                                   firebase_admob::admob_resources_data,
                                   firebase_admob::admob_resources_size));

  if (!(mobile_ads::CacheMethodIds(env, activity) &&
        ad_request_builder::CacheMethodIds(env, activity) &&
        ad_size::CacheMethodIds(env, activity) &&
        ad_view::CacheMethodIds(env, activity) &&
        request_config_builder::CacheMethodIds(env, activity) &&
        banner_view_helper::CacheClassFromFiles(env, activity,
                                                &embedded_files) != nullptr &&
        banner_view_helper::CacheMethodIds(env, activity) &&
        banner_view_helper_ad_view_listener::CacheMethodIds(env, activity) &&
        interstitial_ad_helper::CacheClassFromFiles(
            env, activity, &embedded_files) != nullptr &&
        interstitial_ad_helper::CacheMethodIds(env, activity) &&
        admob::RegisterNatives())) {
    ReleaseClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  g_initialized = true;
  g_activity = env->NewGlobalRef(activity);

  InitializeGoogleMobileAds(env);
  RegisterTerminateOnDefaultAppDestroy();

  return kInitResultSuccess;
}

// Release classes registered by this module.
void ReleaseClasses(JNIEnv* env) {
  mobile_ads::ReleaseClass(env);
  ad_request_builder::ReleaseClass(env);
  ad_size::ReleaseClass(env);
  ad_view::ReleaseClass(env);
  request_config_builder::ReleaseClass(env);
  banner_view_helper::ReleaseClass(env);
  banner_view_helper_ad_view_listener::ReleaseClass(env);
  interstitial_ad_helper::ReleaseClass(env);
}

bool IsInitialized() { return g_initialized; }

void Terminate() {
  if (!g_initialized) {
    LogWarning("AdMob already shut down");
    return;
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

  firebase::admob::FutureCallbackData* callback_data =
      reinterpret_cast<firebase::admob::FutureCallbackData*>(data_ptr);

  CompleteFuture(static_cast<int>(error_code), error_msg,
                 callback_data->future_handle, callback_data->future_data);

  env->ReleaseStringUTFChars(error_message, error_msg);

  // This method is responsible for disposing of the callback data struct.
  delete callback_data;
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_admob_internal_cpp_BannerViewHelper_completeBannerViewFutureCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jint error_code,
    jstring error_message) {
  CompleteAdFutureCallback(env, clazz, data_ptr, error_code, error_message);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_admob_internal_cpp_BannerViewHelper_notifyStateChanged(
    JNIEnv* env, jclass clazz, jlong data_ptr, jint change_code) {
  if (data_ptr == 0) return;  // test call only

  firebase::admob::internal::BannerViewInternal* internal =
      reinterpret_cast<firebase::admob::internal::BannerViewInternal*>(
          data_ptr);

  switch (static_cast<firebase::admob::AdViewChangeCode>(change_code)) {
    case kChangePresentationState:
      internal->NotifyListenerOfPresentationStateChange(
          internal->GetPresentationState());
      break;
    case kChangeBoundingBox:
      internal->NotifyListenerOfBoundingBoxChange(internal->GetBoundingBox());
      break;
    default:
      break;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_admob_internal_cpp_InterstitialAdHelper_completeInterstitialAdFutureCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jint error_code,
    jstring error_message) {
  CompleteAdFutureCallback(env, clazz, data_ptr, error_code, error_message);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_admob_internal_cpp_InterstitialAdHelper_notifyPresentationStateChanged(
    JNIEnv* env, jclass clazz, jlong data_ptr, jint state) {
  if (data_ptr == 0) return;  // test call only

  firebase::admob::internal::InterstitialAdInternal* internal =
      reinterpret_cast<firebase::admob::internal::InterstitialAdInternal*>(
          data_ptr);

  internal->NotifyListenerOfPresentationStateChange(
      static_cast<firebase::admob::InterstitialAd::PresentationState>(state));
}

bool RegisterNatives() {
  static const JNINativeMethod kBannerMethods[] = {
      {"completeBannerViewFutureCallback", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(
           &Java_com_google_firebase_admob_internal_cpp_BannerViewHelper_completeBannerViewFutureCallback)},  // NOLINT
      {"notifyStateChanged", "(JI)V",
       reinterpret_cast<void*>(
           &Java_com_google_firebase_admob_internal_cpp_BannerViewHelper_notifyStateChanged)},  // NOLINT
  };
  static const JNINativeMethod kInterstitialMethods[] = {
      {"completeInterstitialAdFutureCallback", "(JILjava/lang/String;)V",
       reinterpret_cast<void*>(
           &Java_com_google_firebase_admob_internal_cpp_InterstitialAdHelper_completeInterstitialAdFutureCallback)},  // NOLINT
      {"notifyPresentationStateChanged", "(JI)V",
       reinterpret_cast<void*>(
           &Java_com_google_firebase_admob_internal_cpp_InterstitialAdHelper_notifyPresentationStateChanged)},  // NOLINT
  };
  JNIEnv* env = GetJNI();
  return banner_view_helper::RegisterNatives(
             env, kBannerMethods, FIREBASE_ARRAYSIZE(kBannerMethods)) &&
         interstitial_ad_helper::RegisterNatives(
             env, kInterstitialMethods,
             FIREBASE_ARRAYSIZE(kInterstitialMethods));
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
  }
  bool jni_exception = util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(!jni_exception);
  FIREBASE_ASSERT(j_ad_size == nullptr);
  return j_ad_size;
}

}  // namespace admob
}  // namespace firebase
