// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_

#include <type_traits>
#include <utility>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"

namespace firebase::firestore {

class JniRunnableBase {
 public:
  explicit JniRunnableBase(jni::Env& env);
  virtual ~JniRunnableBase();

  static void Initialize(jni::Loader& loader);
  virtual void Run() = 0;
  void Detach(jni::Env& env);
  jni::Local<jni::Object> GetJavaRunnable() const;
  jni::Local<jni::Task> RunOnMainThread(jni::Env& env);
  jni::Local<jni::Task> RunOnNewThread(jni::Env& env);

 private:
  jni::Global<jni::Object> java_runnable_;
};

template <typename CallbackT>
class JniRunnable : public JniRunnableBase {
 public:
  JniRunnable(jni::Env& env, CallbackT callback)
      : JniRunnableBase(env), callback_(std::move(callback)) {}

  ~JniRunnable() override {
    jni::Env env;
    Detach(env);
    if (!env.ok()) env.ExceptionClear();
  }

  void Run() override {
    if constexpr (std::is_invocable_v<CallbackT>) {
      callback_();
    } else {
      callback_(*this);
    }
  }

 private:
  CallbackT callback_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_
