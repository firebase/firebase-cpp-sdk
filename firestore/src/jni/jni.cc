// Copyright 2020 Google LLC

#include "firestore/src/jni/jni.h"

#include <pthread.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "firestore/src/jni/env.h"

namespace firebase::firestore::jni {
namespace {

std::atomic<bool> g_reaper_has_refs{false};
std::mutex g_reaper_mutex;
std::vector<jobject>* g_reaper_refs = nullptr;
pthread_key_t jni_env_key;
std::atomic<JavaVM*> g_jvm{nullptr};

struct EnvOut {
  explicit EnvOut(JNIEnv** env) : env_(env) {}
  ~EnvOut() {
    if (void_env_) *env_ = static_cast<JNIEnv*>(void_env_);
  }
  operator JNIEnv**() { return env_; }
  operator void**() { return &void_env_; }
  JNIEnv** env_;
  void* void_env_ = nullptr;
};

JavaVM* GetGlobalJavaVM() {
  JavaVM* jvm = g_jvm.load(std::memory_order_acquire);
  FIREBASE_ASSERT_MESSAGE(
      jvm != nullptr, "Global JVM is unset; missing call to jni::Initialize()");
  return jvm;
}

void DetachCurrentThread(void* env) {
  JavaVM* jvm = g_jvm.load(std::memory_order_acquire);
  if (!jvm || !env) return;
  JNIEnv* current_env = nullptr;
  if (jvm->GetEnv(EnvOut(&current_env), JNI_VERSION_1_6) == JNI_EDETACHED ||
      !current_env) {
    return;
  }
  if (current_env->ExceptionCheck()) current_env->ExceptionClear();
  jint result = jvm->DetachCurrentThread();
  if (result != JNI_OK) {
    LogWarning("DetachCurrentThread failed to detach (result=%d)", result);
  }
}

}  // namespace

GlobalRefReaper& GlobalRefReaper::GetInstance() {
  static GlobalRefReaper instance;
  return instance;
}

void GlobalRefReaper::DeferDelete(jobject ref) {
  std::lock_guard<std::mutex> lock(g_reaper_mutex);
  if (!g_reaper_refs) g_reaper_refs = new std::vector<jobject>();
  g_reaper_refs->push_back(ref);
  g_reaper_has_refs.store(true, std::memory_order_release);
}

void GlobalRefReaper::Reap(JNIEnv* env) {
  if (!g_reaper_has_refs.load(std::memory_order_acquire)) return;
  std::vector<jobject> to_delete;
  {
    std::lock_guard<std::mutex> lock(g_reaper_mutex);
    if (g_reaper_refs) {
      to_delete = std::move(*g_reaper_refs);
      g_reaper_refs->clear();
    }
    g_reaper_has_refs.store(false, std::memory_order_release);
  }
  for (jobject ref : to_delete) env->DeleteGlobalRef(ref);
}

void Initialize(JavaVM* jvm) {
  FIREBASE_ASSERT_MESSAGE(jvm != nullptr,
                          "Cannot initialize JNI with a null JavaVM");
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, [] {
    int err = pthread_key_create(&jni_env_key, DetachCurrentThread);
    FIREBASE_ASSERT_MESSAGE(err == 0, "pthread_key_create failed (errno=%d)",
                            err);
  });
  g_jvm.store(jvm, std::memory_order_release);
}

JNIEnv* GetAttachedEnv() {
  JavaVM* jvm = GetGlobalJavaVM();
  JNIEnv* env = nullptr;
  if (jvm && jvm->GetEnv(EnvOut(&env), JNI_VERSION_1_6) == JNI_OK) {
    GlobalRefReaper::GetInstance().Reap(env);
    return env;
  }
  return nullptr;
}

JNIEnv* GetEnv() {
  JavaVM* jvm = GetGlobalJavaVM();
  JNIEnv* env = nullptr;
  jint result = jvm->GetEnv(EnvOut(&env), JNI_VERSION_1_6);
  if (result != JNI_OK) {
    FIREBASE_ASSERT_MESSAGE(
        result == JNI_EDETACHED,
        "GetEnv failed with an unexpected error (result=%d)", result);
    result = jvm->AttachCurrentThread(EnvOut(&env), nullptr);
    FIREBASE_ASSERT_MESSAGE(result == JNI_OK,
                            "JNI AttachCurrentThread failed (result=%d)",
                            result);
    result = pthread_setspecific(jni_env_key, env);
    FIREBASE_ASSERT_MESSAGE(result == 0,
                            "JNI pthread_setspecific failed (errno=%d)",
                            result);
  }
  GlobalRefReaper::GetInstance().Reap(env);
  return env;
}

}  // namespace firebase::firestore::jni
