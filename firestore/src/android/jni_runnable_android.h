/*
 * Copyright 2021 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_

#include <type_traits>
#include <utility>
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"

namespace firebase::firestore {

/**
 * A proxy for a Java `Runnable` that calls a C++ function.
 *
 * Note: Typically, this class is not used directly but rather its templated
 * subclass `JniRunnable`.
 *
 * Subclasses must implement the `Run()` method to perform the desired work when
 * the Java `Runnable` object's `run()` method is invoked. `GetJavaRunnable()`
 * will return the Java `Runnable` object whose `run()` method will invoke this
 * object's `Run()` method. When this object is destroyed, or `Detach()` is
 * invoked, then the Java `Runnable` will be "detached" from this C++ object and
 * its `run()` method will do nothing.
 */
class JniRunnableBase {
 public:
  explicit JniRunnableBase(jni::Env& env);

  /**
   * Calls `Detach()`.
   */
  virtual ~JniRunnableBase();

  /**
   * Initializes this class.
   *
   * This method should be called once during application initialization.
   */
  static void Initialize(jni::Loader& loader);

  /**
   * Implements the logic to execute when the Java `Runnable` object's `run()`
   * method is invoked.
   */
  virtual void Run() = 0;

  /**
   * Detaches this object from its companion Java `Runnable` object.
   *
   * After calling this method, all future invocations of the Java `Runnable`
   * object's `run()` method will do nothing and complete as if successful.
   *
   * This method will block until all active invocations of `Run()` have
   * completed.
   *
   * This method may be safely invoked multiple times. Subsequent invocations
   * have no side effects but will still block while there are active
   * invocations of `Run()`.
   */
  void Detach(jni::Env& env);

  /**
   * Returns the companion Java `Runnable` object whose `run()` method will
   * invoke this object's `Run()` method.
   */
  jni::Local<jni::Object> GetJavaRunnable() const;

  /**
   * Schedules this object's `Run()` method to be invoked asynchronously on the
   * Android main event thread.
   *
   * If this method is invoked from the main thread then `Run()` will be invoked
   * synchronously and the returned task will be in the "completed" state.
   *
   * The returned `Task` will complete after this object's `Run()` method has
   * been invoked. If the `Run()` method throws a Java exception then the task
   * will complete with that exception.
   */
  jni::Local<jni::Task> RunOnMainThread(jni::Env& env);

  /**
   * Schedules this object's `Run()` method to be invoked asynchronously on a
   * newly-created thread.
   *
   * The returned `Task` will complete after this object's `Run()` method has
   * been invoked. If the `Run()` method throws a Java exception then the task
   * will complete with that exception.
   */
  jni::Local<jni::Task> RunOnNewThread(jni::Env& env);

 private:
  jni::Global<jni::Object> java_runnable_;
};

/**
 * A proxy for a Java `Runnable` that calls a C++ function.
 *
 * The template parameter `CallbackT` is typically a lambda or function pointer;
 * it can be anything that can be "invoked" with either zero arguments or one
 * argument whose type is `JniRunnable<CallbackT>&` (or `JniRunnableBase&`).
 *
 * Example:
 *
 * jni::Env env;
 * int invoke_count = 0;
 * JniRunnable runnable(env, [&invoke_count] { invoke_count++; });
 * jni::Local<jni::Object> java_runnable = runnable.GetJavaRunnable();
 * for (int i=0; i<5; ++i) {
 *   env.Call(java_runnable, kRunnableRun);
 * }
 * EXPECT_EQ(invoke_count, 5);
 *
 */
template <typename CallbackT>
class JniRunnable : public JniRunnableBase {
 public:
  JniRunnable(jni::Env& env, CallbackT callback)
      : JniRunnableBase(env), callback_(std::move(callback)) {}

  ~JniRunnable() override {
    jni::Env env;
    Detach(env);
    if (!env.ok()) {
      env.ExceptionClear();
    }
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
