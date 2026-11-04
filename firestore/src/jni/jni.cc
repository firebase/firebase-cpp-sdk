/*
 * Copyright 2020 Google LLC
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

#include "firestore/src/jni/jni.h"

#include <pthread.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "app/src/assert.h"
#include "app/src/log.h"


#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {
std::atomic<bool> g_reaper_has_refs{false};
std::mutex g_reaper_mutex;
std::vector<jobject>* g_reaper_refs = nullptr;
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
  for (jobject ref : to_delete) {
    env->DeleteGlobalRef(ref);
  }
}

namespace {

pthread_key_t jni_env_key;

std::atomic<JavaVM*> g_jvm{nullptr};

/**
 * Adapts `JNIEnv**` out parameters to either `JNIEnv**` or `void**` depending
 * on whether the platform's JNI header uses the Android NDK or standard JDK
 * signature for `GetEnv` and `AttachCurrentThread`.
 */
struct EnvOut {
  explicit EnvOut(JNIEnv** env) : env_(env), void_env_(nullptr) {}
  ~EnvOut() {
    if (void_env_ != nullptr) {
      *env_ = static_cast<JNIEnv*>(void_env_);
    }
  }

  operator JNIEnv**() { return env_; }
  operator void**() { return &void_env_; }

  JNIEnv** env_;
  void* void_env_;
};

JavaVM* GetGlobalJavaVM() {
  // TODO(mcg): Use dlsym to call JNI_GetCreatedJavaVMs.
  JavaVM* jvm = g_jvm.load(std::memory_order_acquire);
  FIREBASE_ASSERT_MESSAGE(
      jvm != nullptr,
      "Global JVM is unset; missing call to jni::Initialize()");
  return jvm;
}

/**
 * A callback used by `pthread_key_create` to clean up thread-specific storage
 * when a thread is destroyed.
 */
void DetachCurrentThread(void* env) {
  JavaVM* jvm = g_jvm.load(std::memory_order_acquire);
  if (jvm == nullptr || env == nullptr) {
    return;
  }

  JNIEnv* current_env = nullptr;
  jint get_env_result = jvm->GetEnv(EnvOut(&current_env), JNI_VERSION_1_6);
  if (get_env_result == JNI_EDETACHED || current_env == nullptr) {
    return;
  }

  if (current_env->ExceptionCheck()) {
    current_env->ExceptionClear();
  }

  jint result = jvm->DetachCurrentThread();
  if (result != JNI_OK) {
    LogWarning("DetachCurrentThread failed to detach (result=%d)", result);
  }
}

}  // namespace

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
  if (!jvm) {
    return nullptr;
  }
  JNIEnv* env = nullptr;
  if (jvm->GetEnv(EnvOut(&env), JNI_VERSION_1_6) == JNI_OK) {
    GlobalRefReaper::GetInstance().Reap(env);
    return env;
  }
  return nullptr;
}

JNIEnv* GetEnv() {
  JavaVM* jvm = GetGlobalJavaVM();

  JNIEnv* env = nullptr;
  jint result = jvm->GetEnv(EnvOut(&env), JNI_VERSION_1_6);
  if (result == JNI_OK) {
    // Called from a JVM-managed thread or from a thread that was previously
    // attached. In either case, there's no work to be done.
    GlobalRefReaper::GetInstance().Reap(env);
    return env;
  }

  // The only other documented error is JNI_EVERSION, but all supported Android
  // implementations support JNI 1.6 so this shouldn't happen.
  FIREBASE_ASSERT_MESSAGE(result == JNI_EDETACHED,
                          "GetEnv failed with an unexpected error (result=%d)",
                          result);

  // If we've gotten here, the current thread is a native thread that has not
  // been attached, so we need to attach it and set up a thread-local
  // destructor.
  result = jvm->AttachCurrentThread(EnvOut(&env), nullptr);
  FIREBASE_ASSERT_MESSAGE(result == JNI_OK,
                          "JNI AttachCurrentThread failed (result=%d)", result);

  result = pthread_setspecific(jni_env_key, env);
  FIREBASE_ASSERT_MESSAGE(result == 0,
                          "JNI pthread_setspecific failed (errno=%d)", result);
  GlobalRefReaper::GetInstance().Reap(env);
  return env;
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
