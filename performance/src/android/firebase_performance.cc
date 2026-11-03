// Copyright 2016 Google Inc. All Rights Reserved.

#include <assert.h>
#include <jni.h>

#include <cstdint>
#include <cstring>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "performance/src/android/performance_android_internal.h"
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

  if (!util::Initialize(env, app.activity())) {
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on FirebasePerformance.
  if (!internal::performance_jni::CacheMethodIds(env, app.activity())) {
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on HttpMetric.
  if (!internal::http_metric_jni::CacheMethodIds(env, app.activity())) {
    internal::performance_jni::ReleaseClass(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on Trace.
  if (!internal::trace_jni::CacheMethodIds(env, app.activity())) {
    internal::http_metric_jni::ReleaseClass(env);
    internal::performance_jni::ReleaseClass(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  // Get / create the Performance singleton.
  jobject performance_class_instance_local =
      env->CallStaticObjectMethod(internal::performance_jni::GetClass(),
                                  internal::performance_jni::GetMethodId(
                                      internal::performance_jni::kGetInstance));
  if (util::CheckAndClearJniExceptions(env) ||
      performance_class_instance_local == nullptr) {
    if (performance_class_instance_local != nullptr) {
      env->DeleteLocalRef(performance_class_instance_local);
    }
    internal::trace_jni::ReleaseClass(env);
    internal::http_metric_jni::ReleaseClass(env);
    internal::performance_jni::ReleaseClass(env);
    util::Terminate(env);
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

METHOD_LOOKUP_DEFINITION(performance_jni,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/perf/FirebasePerformance",
                         PERFORMANCE_METHODS)

METHOD_LOOKUP_DEFINITION(http_metric_jni,
                         "com/google/firebase/perf/metrics/HttpMetric",
                         HTTP_METRIC_METHODS)

METHOD_LOOKUP_DEFINITION(trace_jni, "com/google/firebase/perf/metrics/Trace",
                         TRACE_METHODS)

}  // namespace internal

// Clean up the API.
void Terminate() {
  MutexLock lock(g_app_mutex);
  if (!g_app) {
    LogWarning("%s API already shut down", internal::kPerformanceModuleName);
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  util::CancelCallbacks(env, internal::kPerformanceModuleName);
  internal::UnregisterTerminateOnDefaultAppDestroy(g_app);
  g_app = nullptr;
  if (g_performance_class_instance) {
    env->DeleteGlobalRef(g_performance_class_instance);
    g_performance_class_instance = nullptr;
  }
  internal::trace_jni::ReleaseClass(env);
  internal::http_metric_jni::ReleaseClass(env);
  internal::performance_jni::ReleaseClass(env);
  util::Terminate(env);
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
  util::CheckAndClearJniExceptions(env);
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
  util::CheckAndClearJniExceptions(env);
}

}  // namespace performance
}  // namespace firebase
