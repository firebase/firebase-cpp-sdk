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

#include "firestore/src/android/jni_runnable_android.h"

#include "android/firestore_integration_test_android.h"
#include "app/memory/atomic.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#include "firestore/src/jni/throwable.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

namespace {

using jni::Env;
using jni::Global;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticField;
using jni::StaticMethod;
using jni::Task;
using jni::Throwable;

StaticMethod<Object> kGetMainLooper("getMainLooper", "()Landroid/os/Looper;");
Method<Object> kLooperGetThread("getThread", "()Ljava/lang/Thread;");
Method<void> kRunnableRun("run", "()V");
StaticMethod<Object> kCurrentThread("currentThread", "()Ljava/lang/Thread;");
Method<jlong> kThreadGetId("getId", "()J");
Method<Object> kThreadGetState("getState", "()Ljava/lang/Thread$State;");
StaticField<Object> kThreadStateBlocked("BLOCKED", "Ljava/lang/Thread$State;");

class JniRunnableTest : public FirestoreAndroidIntegrationTest {
 public:
  void SetUp() override {
    FirestoreAndroidIntegrationTest::SetUp();
    loader().LoadClass("android/os/Looper", kGetMainLooper, kLooperGetThread);
    loader().LoadClass("java/lang/Runnable", kRunnableRun);
    loader().LoadClass("java/lang/Thread", kCurrentThread, kThreadGetId,
                       kThreadGetState);
    loader().LoadClass("java/lang/Thread$State", kThreadStateBlocked);
    ASSERT_TRUE(loader().ok());
  }
};

/**
 * Returns the ID of the current Java thread.
 */
jlong GetCurrentThreadId(Env& env) {
  Local<Object> thread = env.Call(kCurrentThread);
  return env.Call(thread, kThreadGetId);
}

/**
 * Returns the ID of the Java main thread.
 */
jlong GetMainThreadId(Env& env) {
  Local<Object> main_looper = env.Call(kGetMainLooper);
  Local<Object> main_thread = env.Call(main_looper, kLooperGetThread);
  return env.Call(main_thread, kThreadGetId);
}

/**
 * Returns whether or not the given thread is in the "blocked" state.
 * See java.lang.Thread.State.BLOCKED.
 */
bool IsThreadBlocked(Env& env, Object& thread) {
  Local<Object> actual_state = env.Call(thread, kThreadGetState);
  Local<Object> expected_state = env.Get(kThreadStateBlocked);
  return Object::Equals(env, expected_state, actual_state);
}

TEST_F(JniRunnableTest, JavaRunCallsCppRun) {
  Env env;
  bool invoked = false;
  auto runnable = MakeJniRunnable(env, [&invoked] { invoked = true; });
  Local<Object> java_runnable = runnable.GetJavaRunnable();

  env.Call(java_runnable, kRunnableRun);

  EXPECT_TRUE(invoked);
  EXPECT_TRUE(env.ok());
}

TEST_F(JniRunnableTest, JavaRunCallsCppRunOncePerInvocation) {
  Env env;
  int invoke_count = 0;
  auto runnable = MakeJniRunnable(env, [&invoke_count] { invoke_count++; });
  Local<Object> java_runnable = runnable.GetJavaRunnable();

  env.Call(java_runnable, kRunnableRun);
  env.Call(java_runnable, kRunnableRun);
  env.Call(java_runnable, kRunnableRun);
  env.Call(java_runnable, kRunnableRun);
  env.Call(java_runnable, kRunnableRun);

  EXPECT_EQ(invoke_count, 5);
  EXPECT_TRUE(env.ok());
}

TEST_F(JniRunnableTest, JavaRunPropagatesExceptions) {
  Env env;
  Local<Throwable> exception = CreateException(env, "Forced exception");
  auto runnable = MakeJniRunnable(env, [exception] {
    Env env;
    env.Throw(exception);
  });
  Local<Object> java_runnable = runnable.GetJavaRunnable();

  env.Call(java_runnable, kRunnableRun);

  Local<Throwable> thrown_exception = env.ClearExceptionOccurred();
  EXPECT_TRUE(thrown_exception);
  EXPECT_TRUE(env.IsSameObject(exception, thrown_exception));
}

TEST_F(JniRunnableTest, DetachCausesJavaRunToDoNothing) {
  Env env;
  bool invoked = false;
  auto runnable = MakeJniRunnable(env, [&invoked] { invoked = true; });
  Local<Object> java_runnable = runnable.GetJavaRunnable();

  runnable.Detach(env);

  env.Call(java_runnable, kRunnableRun);
  EXPECT_FALSE(invoked);
  EXPECT_TRUE(env.ok());
}

TEST_F(JniRunnableTest, DetachCanBeInvokedMultipleTimes) {
  Env env;
  bool invoked = false;
  auto runnable = MakeJniRunnable(env, [&invoked] { invoked = true; });
  Local<Object> java_runnable = runnable.GetJavaRunnable();

  runnable.Detach(env);
  runnable.Detach(env);
  runnable.Detach(env);

  env.Call(java_runnable, kRunnableRun);
  EXPECT_FALSE(invoked);
  EXPECT_TRUE(env.ok());
}

TEST_F(JniRunnableTest, DetachDetachesEvenIfAnExceptionIsPending) {
  Env env;
  bool invoked = false;
  auto runnable = MakeJniRunnable(env, [&invoked] { invoked = true; });
  Local<Object> java_runnable = runnable.GetJavaRunnable();
  Local<Throwable> exception = CreateException(env, "Forced exception");
  env.Throw(exception);
  EXPECT_FALSE(env.ok());

  runnable.Detach(env);

  env.ExceptionClear();
  env.Call(java_runnable, kRunnableRun);
  EXPECT_FALSE(invoked);
  EXPECT_TRUE(env.ok());
}

// Verify that b/181129657 does not regress; that is, calling `Detach()` from
// `Run()` should not deadlock.
TEST_F(JniRunnableTest, DetachCanBeCalledFromRun) {
  Env env;
  int run_count = 0;
  auto runnable = MakeJniRunnable(env, [&run_count](JniRunnableBase& runnable) {
    ++run_count;
    Env env;
    runnable.Detach(env);
  });
  Local<Object> java_runnable = runnable.GetJavaRunnable();

  // Call `run()` twice to verify that the call to `Detach()` successfully
  // detaches and the second `run()` invocation does not call C++ `Run()`.
  env.Call(java_runnable, kRunnableRun);
  env.Call(java_runnable, kRunnableRun);

  EXPECT_TRUE(env.ok());
  EXPECT_EQ(run_count, 1);
}

TEST_F(JniRunnableTest, DestructionCausesJavaRunToDoNothing) {
  Env env;
  bool invoked = false;
  Local<Object> java_runnable;
  {
    auto runnable = MakeJniRunnable(env, [&invoked] { invoked = true; });
    java_runnable = runnable.GetJavaRunnable();
  }

  env.Call(java_runnable, kRunnableRun);

  EXPECT_FALSE(invoked);
  EXPECT_TRUE(env.ok());
}

TEST_F(JniRunnableTest, RunOnMainThreadRunsOnTheMainThread) {
  Env env;
  jlong captured_thread_id = 0;
  auto runnable = MakeJniRunnable(env, [&captured_thread_id] {
    Env env;
    captured_thread_id = GetCurrentThreadId(env);
  });

  Local<Task> task = runnable.RunOnMainThread(env);

  Await(env, task);
  EXPECT_EQ(captured_thread_id, GetMainThreadId(env));
}

TEST_F(JniRunnableTest, RunOnMainThreadTaskFailsIfRunThrowsException) {
  Env env;
  Global<Throwable> exception = CreateException(env, "Forced exception");
  auto runnable = MakeJniRunnable(env, [exception] {
    Env env;
    env.Throw(exception);
  });

  Local<Task> task = runnable.RunOnMainThread(env);

  Await(env, task);
  Local<Throwable> thrown_exception = task.GetException(env);
  EXPECT_TRUE(thrown_exception);
  EXPECT_TRUE(env.IsSameObject(exception, thrown_exception));
}

TEST_F(JniRunnableTest, RunOnMainThreadRunsSynchronouslyFromMainThread) {
  Env env;
  bool is_recursive_call = false;
  auto runnable =
      MakeJniRunnable(env, [&is_recursive_call](JniRunnableBase& runnable) {
        Env env;
        EXPECT_EQ(GetCurrentThreadId(env), GetMainThreadId(env));
        if (is_recursive_call) {
          return;
        }
        is_recursive_call = true;
        Local<Task> task = runnable.RunOnMainThread(env);
        EXPECT_TRUE(task.IsComplete(env));
        EXPECT_TRUE(task.IsSuccessful(env));
        is_recursive_call = false;
      });

  Local<Task> task = runnable.RunOnMainThread(env);

  Await(env, task);
}

TEST_F(JniRunnableTest, RunOnNewThreadRunsOnANonMainThread) {
  Env env;
  jlong captured_thread_id = 0;
  auto runnable = MakeJniRunnable(env, [&captured_thread_id] {
    Env env;
    captured_thread_id = GetCurrentThreadId(env);
  });

  Local<Task> task = runnable.RunOnNewThread(env);

  Await(env, task);
  EXPECT_NE(captured_thread_id, 0);
  EXPECT_NE(captured_thread_id, GetMainThreadId(env));
  EXPECT_NE(captured_thread_id, GetCurrentThreadId(env));
}

TEST_F(JniRunnableTest, RunOnNewThreadTaskFailsIfRunThrowsException) {
  Env env;
  Global<Throwable> exception = CreateException(env, "Forced exception");
  auto runnable = MakeJniRunnable(env, [exception] {
    Env env;
    env.Throw(exception);
  });

  Local<Task> task = runnable.RunOnNewThread(env);

  Await(env, task);
  Local<Throwable> thrown_exception = task.GetException(env);
  EXPECT_TRUE(thrown_exception);
  EXPECT_TRUE(env.IsSameObject(exception, thrown_exception));
}

TEST_F(JniRunnableTest, DetachReturnsAfterLastRunOnAnotherThreadCompletes) {
  Env env;
  compat::Atomic<int32_t> runnable1_run_invoke_count;
  runnable1_run_invoke_count.store(0);
  Mutex detach_thread_mutex;
  Global<Object> detach_thread;

  auto runnable1 = MakeJniRunnable(
      env, [&runnable1_run_invoke_count, &detach_thread, &detach_thread_mutex] {
        runnable1_run_invoke_count.fetch_add(1);
        Env env;
        // Wait for `detach()` to be called and start blocking; then, return to
        // allow `detach()` to unblock and do its job.
        while (env.ok()) {
          MutexLock lock(detach_thread_mutex);
          if (detach_thread && IsThreadBlocked(env, detach_thread)) {
            break;
          }
        }
        EXPECT_TRUE(env.ok()) << "IsThreadBlocked() failed with an exception";
      });

  auto runnable2 =
      MakeJniRunnable(env, [&runnable1, &detach_thread, &detach_thread_mutex] {
        Env env;
        {
          MutexLock lock(detach_thread_mutex);
          detach_thread = env.Call(kCurrentThread);
        }
        runnable1.Detach(env);
        EXPECT_TRUE(env.ok()) << "Detach() failed with an exception";
      });

  // Wait for the `runnable1.Run()` to start to ensure that the lock is held.
  Local<Task> task1 = runnable1.RunOnNewThread(env);
  while (true) {
    if (runnable1_run_invoke_count.load() != 0) {
      break;
    }
  }

  // Start a new thread to call `runnable1.Detach()`.
  Local<Task> task2 = runnable2.RunOnNewThread(env);

  Await(env, task1);
  Await(env, task2);

  // Invoke `run()` again and ensure that `Detach()` successfully did its job;
  // that is, verify that `Run()` is not invoked.
  env.Call(runnable1.GetJavaRunnable(), kRunnableRun);
  EXPECT_EQ(runnable1_run_invoke_count.load(), 1);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
