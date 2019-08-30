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

#include "app/src/google_play_services/availability_android.h"

#include <jni.h>

#include <map>

#include "app/google_api_resources.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"

namespace google_play_services {

namespace {

using firebase::Future;
using firebase::FutureHandle;
using firebase::JNINativeMethod;

// clang-format off
#define GOOGLE_API_AVAILABILITY_METHODS(X)                                     \
  X(GetInstance,                                                               \
    "getInstance",                                                             \
    "()Lcom/google/android/gms/common/GoogleApiAvailability;",                 \
    ::firebase::util::kMethodTypeStatic, ::firebase::util::kMethodOptional),   \
  X(IsGooglePlayServicesAvailable, "isGooglePlayServicesAvailable",            \
    "(Landroid/content/Context;)I", ::firebase::util::kMethodTypeInstance,     \
    ::firebase::util::kMethodOptional)
// clang-format on
METHOD_LOOKUP_DECLARATION(googleapiavailability,
                          GOOGLE_API_AVAILABILITY_METHODS)
METHOD_LOOKUP_DEFINITION(googleapiavailability,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/common/GoogleApiAvailability",
                         GOOGLE_API_AVAILABILITY_METHODS)

// clang-format off
#define GOOGLE_API_AVAILABILITY_HELPER_METHODS(X)                              \
  X(StopCallbacks, "stopCallbacks", "()V",                                     \
    ::firebase::util::kMethodTypeStatic),                                      \
  X(MakeGooglePlayServicesAvailable, "makeGooglePlayServicesAvailable",        \
    "(Landroid/app/Activity;)Z", ::firebase::util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(googleapiavailabilityhelper,
                          GOOGLE_API_AVAILABILITY_HELPER_METHODS)
METHOD_LOOKUP_DEFINITION(
    googleapiavailabilityhelper,
    "com/google/firebase/app/internal/cpp/GoogleApiAvailabilityHelper",
    GOOGLE_API_AVAILABILITY_HELPER_METHODS)

// Enum from the Java side. These numbers won't change according to comments
// in the GmsCore code. Only the entries we care about are reproduced here.
enum ConnectionResult {
  kConnectionResultSuccess = 0,
  kConnectionResultServiceMissing = 1,
  kConnectionResultServiceVersionUpdateRequired = 2,
  kConnectionResultServiceDisabled = 3,
  kConnectionResultServiceInvalid = 9,
  kConnectionResultServiceUpdating = 18,
  kConnectionResultServiceMissingPermission = 19,
};

static const struct {
  ConnectionResult java;
  Availability cpp;
} ConnectionResultToAvailability[] = {
    {kConnectionResultSuccess, kAvailabilityAvailable},
    {kConnectionResultServiceMissing, kAvailabilityUnavailableMissing},
    {kConnectionResultServiceVersionUpdateRequired,
     kAvailabilityUnavailableUpdateRequired},
    {kConnectionResultServiceDisabled, kAvailabilityUnavailableDisabled},
    {kConnectionResultServiceInvalid, kAvailabilityUnavailableInvalid},
    {kConnectionResultServiceUpdating, kAvailabilityUnavailableUpdating},
    {kConnectionResultServiceMissingPermission,
     kAvailabilityUnavailablePermissions},
};

enum AvailabilityFn { kAvailabilityFnMakeAvail, kAvailabilityFnCount };
struct AvailabilityData {
  AvailabilityData()
      : future_impl(kAvailabilityFnCount),
        classes_loaded(false),
        fetched_availability(false),
        cached_availability(kAvailabilityUnavailableOther) {}
  // Mapping of Java ConnectionResult values to C++ enum.

  // Future support for MakeGooglePlayServicesAvailable.
  firebase::ReferenceCountedFutureImpl future_impl;
  firebase::SafeFutureHandle<void> future_handle_make;
  bool classes_loaded;
  // Whether we've already checked for Google Play services availability.
  bool fetched_availability;
  // Cached availability state from the last time we checked.
  Availability cached_availability;
};

AvailabilityData* g_data = nullptr;

JNIEXPORT void JNICALL GoogleApiAvailabilityHelper_onCompleteNative(
    JNIEnv* env, jobject clazz, jint status_code, jstring status_message) {
  // If the API is not loaded, this will be nullptr.
  if (g_data != nullptr) {
    if (status_code == 0) {
      g_data->fetched_availability = true;
      g_data->cached_availability = kAvailabilityAvailable;
    }
    g_data->future_impl.Complete(
        g_data->future_handle_make, status_code,
        firebase::util::JniStringToString(env, status_message).c_str());
  }
}

static const JNINativeMethod kHelperMethods[] = {
    {"onCompleteNative", "(ILjava/lang/String;)V",
     reinterpret_cast<void*>(GoogleApiAvailabilityHelper_onCompleteNative)}};

void ReleaseClasses(JNIEnv* env) {
  googleapiavailability::ReleaseClass(env);
  googleapiavailabilityhelper::ReleaseClass(env);
}

// Number of references to this module via Initialize() vs. Terminate().
static unsigned int g_initialized_count = 0;

}  // namespace

bool Initialize(JNIEnv* env, jobject activity) {
  g_initialized_count++;

  if (g_data != nullptr) {
    // Already initialized.
    return true;
  }

  // Instantiate where we store stuff.
  g_data = new AvailabilityData();

  // We depend on firebase::util, so initialize it.
  if (firebase::util::Initialize(env, activity)) {
    // Check if the GoogleApiAvailability class exists at all.
    jclass availability_class = firebase::util::FindClass(
        env, "com/google/android/gms/common/GoogleApiAvailability");
    if (availability_class != nullptr) {
      env->DeleteLocalRef(availability_class);
      // Cache embedded files and load embedded classes.
      const std::vector<firebase::internal::EmbeddedFile> embedded_files =
          firebase::util::CacheEmbeddedFiles(
              env, activity,
              firebase::internal::EmbeddedFile::ToVector(
                  google_api::google_api_resources_filename,
                  google_api::google_api_resources_data,
                  google_api::google_api_resources_size));

      // Cache JNI methods.
      if (googleapiavailability::CacheMethodIds(env, activity) &&
          googleapiavailabilityhelper::CacheClassFromFiles(env, activity,
                                                           &embedded_files) &&
          googleapiavailabilityhelper::CacheMethodIds(env, activity) &&
          googleapiavailabilityhelper::RegisterNatives(
              env, kHelperMethods, FIREBASE_ARRAYSIZE(kHelperMethods))) {
        // Everything initialized properly.
        g_data->classes_loaded = true;
        return true;
      }
      ReleaseClasses(env);
    }
    firebase::util::Terminate(env);
  }

  firebase::LogError(
      "Unable to check Google Play services availablity as the "
      "com.google.android.gms.common.GoogleApiAvailability class is not "
      "present in this application.");
  delete g_data;
  g_data = nullptr;
  g_initialized_count--;
  return false;
}

void Terminate(JNIEnv* env) {
  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  if (g_data && g_initialized_count == 0) {
    if (g_data->classes_loaded) {
      env->CallStaticVoidMethod(
          googleapiavailabilityhelper::GetClass(),
          googleapiavailabilityhelper::GetMethodId(
              googleapiavailabilityhelper::kStopCallbacks));
      firebase::util::CheckAndClearJniExceptions(env);
      ReleaseClasses(env);
      firebase::util::Terminate(env);
    }
    delete g_data;
    g_data = nullptr;
  }
}

Availability CheckAvailability(JNIEnv* env, jobject activity) {
  if (!g_data && !Initialize(env, activity)) {
    // Only initializes if necessary, e.g. before App.
    return kAvailabilityUnavailableOther;
  }

  if (g_data->fetched_availability) return g_data->cached_availability;

  jobject api = env->CallStaticObjectMethod(
      googleapiavailability::GetClass(),
      googleapiavailability::GetMethodId(googleapiavailability::kGetInstance));
  if (!firebase::util::CheckAndClearJniExceptions(env) && api != nullptr) {
    jint result = env->CallIntMethod(
        api,
        googleapiavailability::GetMethodId(
            googleapiavailability::kIsGooglePlayServicesAvailable),
        activity);
    firebase::util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(api);
    for (size_t i = 0; i < FIREBASE_ARRAYSIZE(ConnectionResultToAvailability);
         i++) {
      if (result == ConnectionResultToAvailability[i].java) {
        g_data->cached_availability = ConnectionResultToAvailability[i].cpp;
        g_data->fetched_availability = true;
        return g_data->cached_availability;
      }
    }
  }
  return kAvailabilityUnavailableOther;
}

struct CallData {
  // Thread-safe call data.
  JavaVM* vm;
  jobject activity_global;
};

void CallMakeAvailable(void* data) {
  CallData* call_data = reinterpret_cast<CallData*>(data);
  JNIEnv* env = firebase::util::GetThreadsafeJNIEnv(call_data->vm);
  if (env != nullptr) {
    jboolean result = env->CallStaticBooleanMethod(
        googleapiavailabilityhelper::GetClass(),
        googleapiavailabilityhelper::GetMethodId(
            googleapiavailabilityhelper::kMakeGooglePlayServicesAvailable),
        call_data->activity_global);
    firebase::util::CheckAndClearJniExceptions(env);
    env->DeleteGlobalRef(call_data->activity_global);
    if (result == JNI_FALSE) {
      g_data->future_impl.Complete(
          g_data->future_handle_make, -1,
          "Call to makeGooglePlayServicesAvailable failed.");
    }
  }
  delete call_data;
}

Future<void> MakeAvailable(JNIEnv* env, jobject activity) {
  bool is_initialized = g_data || Initialize(env, activity);
  bool ran_function = false;

  if (g_data && !g_data->future_impl.ValidFuture(g_data->future_handle_make)) {
    g_data->future_handle_make =
        g_data->future_impl.SafeAlloc<void>(kAvailabilityFnMakeAvail);

    if (g_data->fetched_availability &&
        g_data->cached_availability == kAvailabilityAvailable) {
      g_data->future_impl.Complete(g_data->future_handle_make, 0, "");
      ran_function = true;
    } else if (is_initialized && googleapiavailability::GetClass() != nullptr) {
      jobject api =
          env->CallStaticObjectMethod(googleapiavailability::GetClass(),
                                      googleapiavailability::GetMethodId(
                                          googleapiavailability::kGetInstance));
      if (!firebase::util::CheckAndClearJniExceptions(env) && api != nullptr) {
        CallData* call_data = new CallData();  // deleted in CallMakeAvailable
        env->GetJavaVM(&call_data->vm);
        call_data->activity_global = env->NewGlobalRef(activity);
        firebase::util::RunOnMainThread(env, call_data->activity_global,
                                        CallMakeAvailable, call_data);
        ran_function = true;
        env->DeleteLocalRef(api);
      }
    }
    if (!ran_function) {
      g_data->future_impl.Complete(g_data->future_handle_make, -2,
                                   "GoogleApiAvailability was unavailable.");
    }
  }
  return MakeAvailableLastResult();
}

Future<void> MakeAvailableLastResult() {
  if (g_data != nullptr) {
    return static_cast<const Future<void>&>(
        g_data->future_impl.LastResult(kAvailabilityFnMakeAvail));
  } else {
    return Future<void>();
  }
}

}  // namespace google_play_services
