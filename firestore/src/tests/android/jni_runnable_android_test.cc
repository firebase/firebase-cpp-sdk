#include "firestore/src/android/jni_runnable_android.h"

#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#include "firestore/src/jni/throwable.h"
#include "firestore/src/tests/android/firestore_integration_test_android.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

namespace {

using jni::Env;
using jni::Global;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticMethod;
using jni::Task;
using jni::Throwable;

StaticMethod<Object> kGetMainLooper("getMainLooper", "()Landroid/os/Looper;");
Method<Object> kLooperGetThread("getThread", "()Ljava/lang/Thread;");
Method<void> kRunnableRun("run", "()V");
StaticMethod<Object> kCurrentThread("currentThread", "()Ljava/lang/Thread;");
Method<jlong> kThreadGetId("getId", "()J");

class JniRunnableTest : public FirestoreAndroidIntegrationTest {
 public:
  void SetUp() override {
    FirestoreAndroidIntegrationTest::SetUp();
    loader().LoadClass("android/os/Looper", kGetMainLooper, kLooperGetThread);
    loader().LoadClass("java/lang/Runnable", kRunnableRun);
    loader().LoadClass("java/lang/Thread", kCurrentThread, kThreadGetId);
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
  class ChainedMainThreadJniRunnable : public JniRunnableBase {
   public:
    using JniRunnableBase::JniRunnableBase;

    void Run() override {
      Env env;
      EXPECT_EQ(GetCurrentThreadId(env), GetMainThreadId(env));
      if (is_nested_call_) {
        return;
      }
      is_nested_call_ = true;
      Local<Task> task = RunOnMainThread(env);
      EXPECT_TRUE(task.IsComplete(env));
      EXPECT_TRUE(task.IsSuccessful(env));
      is_nested_call_ = false;
    }

   private:
    bool is_nested_call_ = false;
  };

  Env env;
  ChainedMainThreadJniRunnable runnable(env);

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

}  // namespace
}  // namespace firestore
}  // namespace firebase
