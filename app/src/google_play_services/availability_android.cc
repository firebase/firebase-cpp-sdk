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

#include "app/src/app_android.h"
#include "app/src/jni/exception.h"
#include "app/src/google_play_services/availability_android.h"

#include <jni.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/google_api_resources.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"

namespace google_play_services {

namespace {

using firebase::Future;
using firebase::FutureHandle;
using ::JNINativeMethod;
using firebase::Mutex;
using firebase::MutexLock;

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
namespace googleapiavailability {
  enum Method {
    kGetInstance,
    kIsGooglePlayServicesAvailable,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace googleapiavailability {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<jclass> local_clazz(env, env->FindClass("com/google/android/gms/common/GoogleApiAvailability"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetInstance] = env->GetStaticMethodID(g_class.get(), "getInstance", "()Lcom/google/android/gms/common/GoogleApiAvailability;");
    ::firebase::jni::CheckAndClearException(env);
    g_method_ids[kIsGooglePlayServicesAvailable] = env->GetMethodID(g_class.get(), "isGooglePlayServicesAvailable", "(Landroid/content/Context;)I");
    ::firebase::jni::CheckAndClearException(env);
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define GOOGLE_API_AVAILABILITY_HELPER_METHODS(X)                              \
  X(StopCallbacks, "stopCallbacks", "()V",                                     \
    ::firebase::util::kMethodTypeStatic),                                      \
  X(MakeGooglePlayServicesAvailable, "makeGooglePlayServicesAvailable",        \
    "(Landroid/app/Activity;)Z", ::firebase::util::kMethodTypeStatic)
// clang-format on
namespace googleapiavailabilityhelper {
  enum Method {
    kStopCallbacks,
    kMakeGooglePlayServicesAvailable,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace googleapiavailabilityhelper {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<jclass> local_clazz(env, env->FindClass("com/google/firebase/app/internal/cpp/GoogleApiAvailabilityHelper"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kStopCallbacks] = env->GetStaticMethodID(g_class.get(), "stopCallbacks", "()V");
    if (!g_method_ids[kStopCallbacks]) return false;
    g_method_ids[kMakeGooglePlayServicesAvailable] = env->GetStaticMethodID(g_class.get(), "makeGooglePlayServicesAvailable", "(Landroid/app/Activity;)Z");
    if (!g_method_ids[kMakeGooglePlayServicesAvailable]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


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

// Intentionally leaked so JNI callbacks and threads still running at process
// exit never touch destroyed statics.
static Mutex& GetAvailabilityMutex() {
  static Mutex* mutex = new Mutex();
  return *mutex;
}

static std::shared_ptr<AvailabilityData>& GetAvailabilityDataRef() {
  static std::shared_ptr<AvailabilityData>* data =
      new std::shared_ptr<AvailabilityData>();
  return *data;
}

static JavaVM* g_vm = nullptr;
// Number of references to this module via Initialize() vs. Terminate().
static unsigned int g_initialized_count = 0;
static bool g_implicitly_initialized = false;

std::shared_ptr<AvailabilityData> GetAvailabilityDataLocked() {
  return GetAvailabilityDataRef();
}

JNIEXPORT void JNICALL GoogleApiAvailabilityHelper_onCompleteNative(
    JNIEnv* env, jobject clazz, jint status_code, jstring status_message) {
  (void)clazz;
  std::string message =
      ::firebase::jni::JStringToString(env, status_message);
  std::shared_ptr<AvailabilityData> data;
  firebase::SafeFutureHandle<void> handle;
  {
    MutexLock lock(GetAvailabilityMutex());
    data = GetAvailabilityDataLocked();
    if (data != nullptr) {
      if (status_code == 0) {
        data->fetched_availability = true;
        data->cached_availability = kAvailabilityAvailable;
      }
      handle = data->future_handle_make;
    }
  }
  if (data != nullptr) {
    data->future_impl.Complete(handle, status_code, message.c_str());
  }
}

static const JNINativeMethod kHelperMethods[] = {
    {"onCompleteNative", "(ILjava/lang/String;)V",
     reinterpret_cast<void*>(GoogleApiAvailabilityHelper_onCompleteNative)}};

void ReleaseClasses(JNIEnv* env) {
  googleapiavailability::ReleaseClass(env);
  googleapiavailabilityhelper::ReleaseClass(env);
}

bool InitializeLocked(JNIEnv* env, jobject activity) {
  if (!env || !activity) return false;
  env->GetJavaVM(&g_vm);
  if (g_implicitly_initialized && GetAvailabilityDataLocked() != nullptr) {
    // Adopt the implicit initialization reference from a prior
    // CheckAvailability/MakeAvailable call.
    g_implicitly_initialized = false;
    g_initialized_count++;
    return true;
  }
  g_initialized_count++;

  if (GetAvailabilityDataLocked() != nullptr) {
    // Already initialized.
    return true;
  }

  // Instantiate where we store stuff.
  GetAvailabilityDataRef() = std::make_shared<AvailabilityData>();

  // We depend on firebase::util, so initialize it.
  if (true) {
    // Check if the GoogleApiAvailability class exists at all.
    jclass availability_class = env->FindClass("com/google/android/gms/common/GoogleApiAvailability");
    if (availability_class != nullptr) {
      env->DeleteLocalRef(availability_class);
      // Initialize JNI methods.
      if (googleapiavailability::CacheMethodIds(env, activity) &&
          googleapiavailabilityhelper::CacheMethodIds(env, activity)) {
        env->RegisterNatives(googleapiavailabilityhelper::GetClass(), kHelperMethods, std::size(kHelperMethods));
        // Everything initialized properly.
        GetAvailabilityDataRef()->classes_loaded = true;
        return true;
      }
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
      }
      ReleaseClasses(env);
    }
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
  }
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
  firebase::LogError(
      "Unable to check Google Play services availability as the "
      "com.google.android.gms.common.GoogleApiAvailability class is not "
      "present in this application.");
  GetAvailabilityDataRef().reset();
  g_initialized_count--;
  return false;
}

}  // namespace

bool Initialize(JNIEnv* env, jobject activity) {
  MutexLock lock(GetAvailabilityMutex());
  return InitializeLocked(env, activity);
}

void Terminate(JNIEnv* env) {
  std::shared_ptr<AvailabilityData> data_to_destroy;
  jclass helper_class_local = nullptr;
  jmethodID stop_callbacks_method = nullptr;
  bool should_cleanup_classes = false;
  {
    MutexLock lock(GetAvailabilityMutex());
    if (g_initialized_count == 0 && !g_implicitly_initialized) {
      // Nothing to terminate.
      firebase::LogWarning("Extraneous call to google_play_services::Terminate");
      return;
    }
    if (env == nullptr && g_vm != nullptr) {
      env = firebase::util::GetThreadsafeJNIEnv(g_vm);
    }
    if (env == nullptr) {
      env = firebase::util::GetJNIEnvFromApp();
    }
    std::shared_ptr<AvailabilityData> current_data = GetAvailabilityDataLocked();
    bool will_destroy = (g_initialized_count == 1) || (g_initialized_count == 0 && g_implicitly_initialized);
    if (will_destroy && current_data && current_data->classes_loaded && env == nullptr) {
      firebase::LogWarning(
          "google_play_services::Terminate called with null JNIEnv "
          "and no JVM available; deferring class cleanup.");
      return;
    }
    if (g_initialized_count > 0) {
      g_initialized_count--;
    }
    g_implicitly_initialized = false;
    if (current_data && g_initialized_count == 0) {
      data_to_destroy = std::move(GetAvailabilityDataRef());
      
      
      if (data_to_destroy->classes_loaded && env != nullptr) {
        should_cleanup_classes = true;
        jclass helper_class = googleapiavailabilityhelper::GetClass();
        if (helper_class != nullptr) {
          helper_class_local =
              static_cast<jclass>(env->NewLocalRef(helper_class));
        }
        stop_callbacks_method = googleapiavailabilityhelper::GetMethodId(
            googleapiavailabilityhelper::kStopCallbacks);
      }
    }
  }
  if (should_cleanup_classes && env != nullptr) {
    if (helper_class_local != nullptr && stop_callbacks_method != nullptr) {
      env->CallStaticVoidMethod(helper_class_local, stop_callbacks_method);
      ::firebase::jni::CheckAndClearException(env);
    }
    if (helper_class_local != nullptr) {
      env->DeleteLocalRef(helper_class_local);
    }
    {
      MutexLock lock(GetAvailabilityMutex());
      if (g_initialized_count == 0) {
        ReleaseClasses(env);
      }
    }
  }
  data_to_destroy.reset();
}

Availability CheckAvailability(JNIEnv* env, jobject activity) {
  if (!env || !activity) return kAvailabilityUnavailableOther;
  std::shared_ptr<AvailabilityData> data;
  jclass availability_class_local = nullptr;
  jmethodID get_instance_method = nullptr;
  jmethodID is_available_method = nullptr;
  {
    MutexLock lock(GetAvailabilityMutex());
    if (GetAvailabilityDataLocked() == nullptr) {
      if (!InitializeLocked(env, activity)) {
        // Only initializes if necessary, e.g. before App.
        return kAvailabilityUnavailableOther;
      }
      g_implicitly_initialized = true;
      g_initialized_count--;
    }

    data = GetAvailabilityDataLocked();
    if (data->fetched_availability) return data->cached_availability;

    // Take a local reference under the lock: a concurrent Terminate() may
    // release the cached global class reference.
    if (data->classes_loaded && googleapiavailability::GetClass() != nullptr) {
      availability_class_local = static_cast<jclass>(
          env->NewLocalRef(googleapiavailability::GetClass()));
      get_instance_method = googleapiavailability::GetMethodId(
          googleapiavailability::kGetInstance);
      is_available_method = googleapiavailability::GetMethodId(
          googleapiavailability::kIsGooglePlayServicesAvailable);
    }
  }

  if (availability_class_local != nullptr && get_instance_method != nullptr &&
      is_available_method != nullptr) {
    jobject api = env->CallStaticObjectMethod(
        availability_class_local, get_instance_method);
    if (!::firebase::jni::CheckAndClearException(env) && api != nullptr) {
      jint result = env->CallIntMethod(api, is_available_method, activity);
      bool exception = ::firebase::jni::CheckAndClearException(env);
      env->DeleteLocalRef(api);
      if (!exception) {
        for (size_t i = 0; i < std::size(ConnectionResultToAvailability);
             i++) {
          if (result == ConnectionResultToAvailability[i].java) {
            Availability availability = ConnectionResultToAvailability[i].cpp;
            if (availability == kAvailabilityAvailable) {
              MutexLock lock(GetAvailabilityMutex());
              data->cached_availability = availability;
              data->fetched_availability = true;
            }
            env->DeleteLocalRef(availability_class_local);
            return availability;
          }
        }
      }
    }
    env->DeleteLocalRef(availability_class_local);
  }
  return kAvailabilityUnavailableOther;
}

struct CallData {
  // Thread-safe call data.
  std::shared_ptr<AvailabilityData> availability_data;
  firebase::SafeFutureHandle<void> future_handle;
  JavaVM* vm;
  jobject activity_global;
};

void CallMakeAvailable(void* data) {
  CallData* call_data = reinterpret_cast<CallData*>(data);
  JNIEnv* env = firebase::util::GetThreadsafeJNIEnv(call_data->vm);
  if (env != nullptr) {
    jclass helper_class_local = nullptr;
    jmethodID make_available_method = nullptr;
    {
      MutexLock lock(GetAvailabilityMutex());
      std::shared_ptr<AvailabilityData> current_data =
          GetAvailabilityDataLocked();
      if (current_data != nullptr &&
          current_data == call_data->availability_data &&
          current_data->classes_loaded &&
          googleapiavailabilityhelper::GetClass() != nullptr) {
        helper_class_local = static_cast<jclass>(
            env->NewLocalRef(googleapiavailabilityhelper::GetClass()));
        make_available_method = googleapiavailabilityhelper::GetMethodId(
            googleapiavailabilityhelper::kMakeGooglePlayServicesAvailable);
      }
    }
    bool failed = true;
    if (helper_class_local != nullptr && make_available_method != nullptr) {
      jboolean result = env->CallStaticBooleanMethod(
          helper_class_local, make_available_method,
          call_data->activity_global);
      bool exception_thrown = ::firebase::jni::CheckAndClearException(env);
      failed = exception_thrown || (result == JNI_FALSE);
    }
    if (helper_class_local != nullptr) {
      env->DeleteLocalRef(helper_class_local);
    }
    if (failed && call_data->availability_data != nullptr) {
      call_data->availability_data->future_impl.Complete(
          call_data->future_handle, -1,
          "Call to makeGooglePlayServicesAvailable failed.");
    }
    if (call_data->activity_global != nullptr) {
      env->DeleteGlobalRef(call_data->activity_global);
    }
  } else {
    if (call_data->availability_data != nullptr) {
      call_data->availability_data->future_impl.Complete(
          call_data->future_handle, -1,
          "Call to makeGooglePlayServicesAvailable failed.");
    }
    if (call_data->activity_global != nullptr) {
      JNIEnv* cleanup_env = firebase::util::GetJNIEnvFromApp();
      if (cleanup_env != nullptr) {
        cleanup_env->DeleteGlobalRef(call_data->activity_global);
      }
    }
  }
  delete call_data;
}

Future<void> MakeAvailable(JNIEnv* env, jobject activity) {
  if (!env || !activity) return Future<void>();
  std::shared_ptr<AvailabilityData> data;
  firebase::SafeFutureHandle<void> handle_to_complete;
  bool should_complete_immediately = false;
  int complete_error = 0;
  const char* complete_message = "";
  {
    MutexLock lock(GetAvailabilityMutex());
    bool is_initialized = (GetAvailabilityDataLocked() != nullptr);
    if (!is_initialized) {
      is_initialized = InitializeLocked(env, activity);
      if (is_initialized) {
        g_implicitly_initialized = true;
        g_initialized_count--;
      }
    }
    data = GetAvailabilityDataLocked();
    if (!data) {
      return Future<void>();
    }
    Future<void> last_result = static_cast<const Future<void>&>(
        data->future_impl.LastResult(kAvailabilityFnMakeAvail));
    if (last_result.status() == firebase::kFutureStatusPending) {
      return last_result;
    }

    data->future_handle_make =
        data->future_impl.SafeAlloc<void>(kAvailabilityFnMakeAvail);
    handle_to_complete = data->future_handle_make;

    if (data->fetched_availability &&
        data->cached_availability == kAvailabilityAvailable) {
      should_complete_immediately = true;
      complete_error = 0;
      complete_message = "";
    } else {
      bool scheduled = false;
      if (is_initialized && googleapiavailability::GetClass() != nullptr) {
        jobject api = env->CallStaticObjectMethod(
            googleapiavailability::GetClass(),
            googleapiavailability::GetMethodId(
                googleapiavailability::kGetInstance));
        if (!::firebase::jni::CheckAndClearException(env) &&
            api != nullptr) {
          CallData* call_data = new CallData();  // deleted in CallMakeAvailable
          call_data->availability_data = data;
          call_data->future_handle = handle_to_complete;
          env->GetJavaVM(&call_data->vm);
          call_data->activity_global = env->NewGlobalRef(activity);
          firebase::util::RunOnMainThread(env, call_data->activity_global,
                                          CallMakeAvailable, call_data);
          scheduled = true;
          env->DeleteLocalRef(api);
        }
      }
      if (!scheduled) {
        should_complete_immediately = true;
        complete_error = -2;
        complete_message = "GoogleApiAvailability was unavailable.";
      }
    }
  }
  if (should_complete_immediately && data != nullptr) {
    data->future_impl.Complete(handle_to_complete, complete_error,
                               complete_message);
  }
  return MakeAvailableLastResult();
}

Future<void> MakeAvailableLastResult() {
  std::shared_ptr<AvailabilityData> data;
  {
    MutexLock lock(GetAvailabilityMutex());
    data = GetAvailabilityDataLocked();
  }
  if (data != nullptr) {
    return static_cast<const Future<void>&>(
        data->future_impl.LastResult(kAvailabilityFnMakeAvail));
  } else {
    return Future<void>();
  }
}

}  // namespace google_play_services
