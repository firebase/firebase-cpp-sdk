// Copyright 2016 Google Inc. All Rights Reserved.

#include "app/src/jni/exception.h"
#include <assert.h>
#include <jni.h>
#include "app/src/app_android.h"

#include <cstdint>
#include <cstring>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "performance/src/android/performance_android_internal.h"
#include "app/src/app_android.h"
#include "app/src/jni/string.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

// Global reference to the Android Performance class instance.
// This is initialized in performance::Initialize() and never released
// during the lifetime of the application.
static jobject g_performance_class_instance = nullptr;

// Used to retrieve the JNI environment in order to call methods on the
// Android Performance class.
static const ::firebase::App* g_app = nullptr;

static Mutex g_app_mutex;

// Initialize the Performance API.
InitResult Initialize(const ::firebase::App& app) {
  MutexLock lock(g_app_mutex);
  if (g_app) {
    LogWarning("%s API already initialized", internal::kPerformanceModuleName);
    return kInitResultSuccess;
  }
  LogInfo("Firebase Performance API Initializing");
  FIREBASE_ASSERT(!g_performance_class_instance);
  JNIEnv* env = app.GetJNIEnv();

  if (!true) {
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on FirebasePerformance.
  if (!internal::performance_jni::CacheMethodIds(env, app.activity())) {
    
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on HttpMetric.
  if (!internal::http_metric_jni::CacheMethodIds(env, app.activity())) {
    internal::performance_jni::ReleaseClass(env);
    
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on Trace.
  if (!internal::trace_jni::CacheMethodIds(env, app.activity())) {
    internal::http_metric_jni::ReleaseClass(env);
    internal::performance_jni::ReleaseClass(env);
    
    return kInitResultFailedMissingDependency;
  }

  // Get / create the Performance singleton.
  jobject performance_class_instance_local =
      env->CallStaticObjectMethod(internal::performance_jni::GetClass(),
                                  internal::performance_jni::GetMethodId(
                                      internal::performance_jni::kGetInstance));
  if (jni::CheckAndClearException(env) ||
      performance_class_instance_local == nullptr) {
    if (performance_class_instance_local != nullptr) {
      env->DeleteLocalRef(performance_class_instance_local);
    }
    internal::trace_jni::ReleaseClass(env);
    internal::http_metric_jni::ReleaseClass(env);
    internal::performance_jni::ReleaseClass(env);
    
    return kInitResultFailedMissingDependency;
  }
  g_app = &app;
  // Keep a reference to the Performance singleton.
  g_performance_class_instance =
      env->NewGlobalRef(performance_class_instance_local);
  FIREBASE_ASSERT(g_performance_class_instance);

  env->DeleteLocalRef(performance_class_instance_local);
  internal::RegisterTerminateOnDefaultAppDestroy(&app);
  LogInfo("%s API Initialized", internal::kPerformanceModuleName);
  return kInitResultSuccess;
}

namespace internal {

// Determine whether the performance module is initialized.
bool IsInitialized() { 
  MutexLock lock(g_app_mutex);
  return g_app != nullptr; 
}

jobject GetFirebasePerformanceClassInstance() {
  MutexLock lock(g_app_mutex);
  return g_performance_class_instance;
}

const ::firebase::App* GetFirebaseApp() { 
  MutexLock lock(g_app_mutex);
  return g_app; 
}

DEFINE_FIREBASE_VERSION_STRING(FirebasePerformance);

namespace performance_jni {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<jclass> local_clazz(env, env->FindClass("com/google/firebase/perf/FirebasePerformance"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kSetEnabled] = env->GetMethodID(g_class.get(), "setPerformanceCollectionEnabled", "(Z)V");
    if (!g_method_ids[kSetEnabled]) return false;
    g_method_ids[kGetEnabled] = env->GetMethodID(g_class.get(), "isPerformanceCollectionEnabled", "()Z");
    if (!g_method_ids[kGetEnabled]) return false;
    g_method_ids[kGetInstance] = env->GetStaticMethodID(g_class.get(), "getInstance", "()Lcom/google/firebase/perf/FirebasePerformance;");
    if (!g_method_ids[kGetInstance]) return false;
    g_method_ids[kNewTrace] = env->GetMethodID(g_class.get(), "newTrace", "(Ljava/lang/String;)Lcom/google/firebase/perf/metrics/Trace;");
    if (!g_method_ids[kNewTrace]) return false;
    g_method_ids[kNewHttpMetric] = env->GetMethodID(g_class.get(), "newHttpMetric", "(Ljava/lang/String;Ljava/lang/String;)Lcom/google/firebase/perf/metrics/HttpMetric;");
    if (!g_method_ids[kNewHttpMetric]) return false;
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


namespace http_metric_jni {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<jclass> local_clazz(env, env->FindClass("com/google/firebase/perf/metrics/HttpMetric"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kStartHttpMetric] = env->GetMethodID(g_class.get(), "start", "()V");
    if (!g_method_ids[kStartHttpMetric]) return false;
    g_method_ids[kStopHttpMetric] = env->GetMethodID(g_class.get(), "stop", "()V");
    if (!g_method_ids[kStopHttpMetric]) return false;
    g_method_ids[kSetAttribute] = env->GetMethodID(g_class.get(), "putAttribute", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!g_method_ids[kSetAttribute]) return false;
    g_method_ids[kGetAttribute] = env->GetMethodID(g_class.get(), "getAttribute", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!g_method_ids[kGetAttribute]) return false;
    g_method_ids[kRemoveAttribute] = env->GetMethodID(g_class.get(), "removeAttribute", "(Ljava/lang/String;)V");
    if (!g_method_ids[kRemoveAttribute]) return false;
    g_method_ids[kSetHttpResponseCode] = env->GetMethodID(g_class.get(), "setHttpResponseCode", "(I)V");
    if (!g_method_ids[kSetHttpResponseCode]) return false;
    g_method_ids[kSetRequestPayloadSize] = env->GetMethodID(g_class.get(), "setRequestPayloadSize", "(J)V");
    if (!g_method_ids[kSetRequestPayloadSize]) return false;
    g_method_ids[kSetResponseContentType] = env->GetMethodID(g_class.get(), "setResponseContentType", "(Ljava/lang/String;)V");
    if (!g_method_ids[kSetResponseContentType]) return false;
    g_method_ids[kSetResponsePayloadSize] = env->GetMethodID(g_class.get(), "setResponsePayloadSize", "(J)V");
    if (!g_method_ids[kSetResponsePayloadSize]) return false;
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


namespace trace_jni {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<jclass> local_clazz(env, env->FindClass("com/google/firebase/perf/metrics/Trace"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kStartTrace] = env->GetMethodID(g_class.get(), "start", "()V");
    if (!g_method_ids[kStartTrace]) return false;
    g_method_ids[kStopTrace] = env->GetMethodID(g_class.get(), "stop", "()V");
    if (!g_method_ids[kStopTrace]) return false;
    g_method_ids[kSetAttribute] = env->GetMethodID(g_class.get(), "putAttribute", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!g_method_ids[kSetAttribute]) return false;
    g_method_ids[kGetAttribute] = env->GetMethodID(g_class.get(), "getAttribute", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!g_method_ids[kGetAttribute]) return false;
    g_method_ids[kRemoveAttribute] = env->GetMethodID(g_class.get(), "removeAttribute", "(Ljava/lang/String;)V");
    if (!g_method_ids[kRemoveAttribute]) return false;
    g_method_ids[kIncrementMetric] = env->GetMethodID(g_class.get(), "incrementMetric", "(Ljava/lang/String;J)V");
    if (!g_method_ids[kIncrementMetric]) return false;
    g_method_ids[kGetLongMetric] = env->GetMethodID(g_class.get(), "getLongMetric", "(Ljava/lang/String;)J");
    if (!g_method_ids[kGetLongMetric]) return false;
    g_method_ids[kPutMetric] = env->GetMethodID(g_class.get(), "putMetric", "(Ljava/lang/String;J)V");
    if (!g_method_ids[kPutMetric]) return false;
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


}  // namespace internal

// Clean up the API.
void Terminate() {
  MutexLock lock(g_app_mutex);
  if (!g_app) {
    LogWarning("%s API already shut down", internal::kPerformanceModuleName);
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  firebase::util::CancelCallbacks(env, internal::kPerformanceModuleName);
  internal::UnregisterTerminateOnDefaultAppDestroy(g_app);
  g_app = nullptr;
  if (g_performance_class_instance) {
    env->DeleteGlobalRef(g_performance_class_instance);
    g_performance_class_instance = nullptr;
  }
  internal::trace_jni::ReleaseClass(env);
  internal::http_metric_jni::ReleaseClass(env);
  internal::performance_jni::ReleaseClass(env);
  
}

// Determines if performance collection is enabled.
bool GetPerformanceCollectionEnabled() {
  MutexLock lock(g_app_mutex);
  FIREBASE_ASSERT_RETURN(false, g_app != nullptr);
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean result =
      env->CallBooleanMethod(g_performance_class_instance,
                             internal::performance_jni::GetMethodId(
                                 internal::performance_jni::kGetEnabled));
  jni::CheckAndClearException(env);
  return static_cast<bool>(result);
}

// Sets performance collection enabled or disabled.
void SetPerformanceCollectionEnabled(bool enabled) {
  MutexLock lock(g_app_mutex);
  FIREBASE_ASSERT_RETURN_VOID(g_app != nullptr);
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean collection_enabled = enabled;
  env->CallVoidMethod(g_performance_class_instance,
                      internal::performance_jni::GetMethodId(
                          internal::performance_jni::kSetEnabled),
                      collection_enabled);
  jni::CheckAndClearException(env);
}

}  // namespace performance
}  // namespace firebase
