// Copyright 2016 Google Inc. All Rights Reserved.

#include <jni.h>

#include "app/src/app_android.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/log.h"
#include "app/src/util_android.h"
#include "performance/src/android/performance_android_internal.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

static jobject g_performance_class_instance = nullptr;
static const ::firebase::App* g_app = nullptr;
static Mutex g_app_mutex;

namespace {
void ReleaseAllJniClasses(JNIEnv* env) {
  internal::trace_jni::ReleaseClass(env);
  internal::http_metric_jni::ReleaseClass(env);
  internal::performance_jni::ReleaseClass(env);
}
}  // namespace

InitResult Initialize(const ::firebase::App& app) {
  MutexLock lock(g_app_mutex);
  if (g_app) {
    LogWarning("%s API already initialized", internal::kPerformanceModuleName);
    return kInitResultSuccess;
  }
  FIREBASE_ASSERT(!g_performance_class_instance);
  JNIEnv* env = app.GetJNIEnv();
  if (!util::Initialize(env, app.activity())) {
    return kInitResultFailedMissingDependency;
  }
  if (!internal::performance_jni::CacheMethodIds(env, app.activity()) ||
      !internal::http_metric_jni::CacheMethodIds(env, app.activity()) ||
      !internal::trace_jni::CacheMethodIds(env, app.activity())) {
    ReleaseAllJniClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }
  jobject local_inst = env->CallStaticObjectMethod(
      internal::performance_jni::GetClass(),
      internal::performance_jni::GetMethodId(
          internal::performance_jni::kGetInstance));
  if (jni::CheckAndClearException(env) || !local_inst) {
    if (local_inst) env->DeleteLocalRef(local_inst);
    ReleaseAllJniClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }
  g_app = &app;
  g_performance_class_instance = env->NewGlobalRef(local_inst);
  env->DeleteLocalRef(local_inst);
  internal::RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

namespace internal {
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
}  // namespace internal

void Terminate() {
  MutexLock lock(g_app_mutex);
  if (!g_app) {
    LogWarning("%s API already shut down", internal::kPerformanceModuleName);
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  g_app = nullptr;
  util::CancelCallbacks(env, internal::kPerformanceModuleName);
  internal::UnregisterTerminateOnDefaultAppDestroy();
  if (g_performance_class_instance) {
    env->DeleteGlobalRef(g_performance_class_instance);
    g_performance_class_instance = nullptr;
  }
  ReleaseAllJniClasses(env);
  util::Terminate(env);
}

bool GetPerformanceCollectionEnabled() {
  MutexLock lock(g_app_mutex);
  FIREBASE_ASSERT_RETURN(false, g_app != nullptr);
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean res = env->CallBooleanMethod(
      g_performance_class_instance,
      internal::performance_jni::GetMethodId(
          internal::performance_jni::kGetEnabled));
  jni::CheckAndClearException(env);
  return static_cast<bool>(res);
}

void SetPerformanceCollectionEnabled(bool enabled) {
  MutexLock lock(g_app_mutex);
  FIREBASE_ASSERT_RETURN_VOID(g_app != nullptr);
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(g_performance_class_instance,
                      internal::performance_jni::GetMethodId(
                          internal::performance_jni::kSetEnabled),
                      static_cast<jboolean>(enabled));
  jni::CheckAndClearException(env);
}

}  // namespace performance
}  // namespace firebase
