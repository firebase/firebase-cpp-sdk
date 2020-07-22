#include "firestore/src/jni/jni.h"

#include <pthread.h>

#include "app/src/assert.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

pthread_key_t jni_env_key;

JavaVM* g_jvm = nullptr;

/**
 * Reinterprets `JNIEnv**` out parameters to `void**` on platforms where that's
 * required.
 */
void** EnvOut(JNIEnv** env) { return reinterpret_cast<void**>(env); }

JavaVM* GetGlobalJavaVM() {
  // TODO(mcg): Use dlsym to call JNI_GetCreatedJavaVMs.
  FIREBASE_ASSERT_MESSAGE(
      g_jvm != nullptr,
      "Global JVM is unset; missing call to jni::Initialize()");
  return g_jvm;
}

/**
 * A callback used by `pthread_key_create` to clean up thread-specific storage
 * when a thread is destroyed.
 */
void DetachCurrentThread(void* env) {
  JavaVM* jvm = g_jvm;
  if (jvm == nullptr || env == nullptr) {
    return;
  }

  jint result = jvm->DetachCurrentThread();
  if (result != JNI_OK) {
    LogWarning("DetachCurrentThread failed to detach (result=%d)", result);
  }
}

}  // namespace

void Initialize(JavaVM* jvm) {
  g_jvm = jvm;

  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, [] {
    int err = pthread_key_create(&jni_env_key, DetachCurrentThread);
    FIREBASE_ASSERT_MESSAGE(err == 0, "pthread_key_create failed (errno=%d)",
                            err);
  });
}

JNIEnv* GetEnv() {
  JavaVM* jvm = GetGlobalJavaVM();

  JNIEnv* env = nullptr;
  jint result = jvm->GetEnv(EnvOut(&env), JNI_VERSION_1_6);
  if (result == JNI_OK) {
    // Called from a JVM-managed thread or from a thread that was previously
    // attached. In either case, there's no work to be done.
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
  result = jvm->AttachCurrentThread(&env, nullptr);
  FIREBASE_ASSERT_MESSAGE(result == JNI_OK,
                          "JNI AttachCurrentThread failed (result=%d)", result);

  result = pthread_setspecific(jni_env_key, env);
  FIREBASE_ASSERT_MESSAGE(result == 0,
                          "JNI pthread_setspecific failed (errno=%d)", result);
  return env;
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
