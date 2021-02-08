// Copyright 2016 Google Inc. All Rights Reserved.

#include <assert.h>
#include <jni.h>

#include <cstdint>
#include <cstring>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
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

// Initialize the Performance API.
InitResult Initialize(const ::firebase::App& app) {
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
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers on Trace.
  if (!internal::trace_jni::CacheMethodIds(env, app.activity())) {
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  g_app = &app;
  // Get / create the Performance singleton.
  jobject performance_class_instance_local =
      env->CallStaticObjectMethod(internal::performance_jni::GetClass(),
                                  internal::performance_jni::GetMethodId(
                                      internal::performance_jni::kGetInstance));
  util::CheckAndClearJniExceptions(env);
  // Keep a reference to the Performance singleton.
  g_performance_class_instance =
      env->NewGlobalRef(performance_class_instance_local);
  FIREBASE_ASSERT(g_performance_class_instance);

  env->DeleteLocalRef(performance_class_instance_local);
  internal::RegisterTerminateOnDefaultAppDestroy();
  LogInfo("%s API Initialized", internal::kPerformanceModuleName);
  return kInitResultSuccess;
}

namespace internal {

// Determine whether the performance module is initialized.
bool IsInitialized() { return g_app != nullptr; }

jobject GetFirebasePerformanceClassInstance() {
  return g_performance_class_instance;
}

const ::firebase::App* GetFirebaseApp() { return g_app; }

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
  if (!g_app) {
    LogWarning("%s API already shut down", internal::kPerformanceModuleName);
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  util::CancelCallbacks(env, internal::kPerformanceModuleName);
  internal::UnregisterTerminateOnDefaultAppDestroy();
  g_app = nullptr;
  env->DeleteGlobalRef(g_performance_class_instance);
  g_performance_class_instance = nullptr;
  internal::performance_jni::ReleaseClass(env);
  util::Terminate(env);
}

// Determines if performance collection is enabled.
bool GetPerformanceCollectionEnabled() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
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
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
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
