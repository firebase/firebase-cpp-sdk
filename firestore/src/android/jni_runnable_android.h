#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_

#include "app/meta/move.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {

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
   * completed, and will cause new invocations of the Java `Runnable` object's
   * `run()` that occur while this method is blocked to also block until this
   * method completes.
   *
   * Calling `Detach()` multiple times is allowed, but invocations after the
   * first invocation have no effect.
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
 * it can be anything that can be "invoked" with zero arguments.
 *
 * Example:
 *
 * jni::Env env;
 * int invoke_count = 0;
 * auto runnable = MakeJniRunnable(env, [&invoke_count] { invoke_count++; });
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
      : JniRunnableBase(env), callback_(firebase::Move(callback)) {}

  void Run() override { callback_(); }

 private:
  CallbackT callback_;
};

/**
 * Creates and returns a new instance of `JniRunnable`.
 *
 * TODO: Remove this function in favor of just using the constructor once C++17
 * becomes the lowest-supported C++ version.
 */
template <typename CallbackT>
JniRunnable<CallbackT> MakeJniRunnable(jni::Env& env, CallbackT callback) {
  return JniRunnable<CallbackT>(env, firebase::Move(callback));
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_JNI_RUNNABLE_ANDROID_H_
