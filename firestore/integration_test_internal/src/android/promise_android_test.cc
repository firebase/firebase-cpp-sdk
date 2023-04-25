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

#include "firestore/src/android/promise_android.h"

#include <memory>
#include <string>

#include "android/cancellation_token_source.h"
#include "android/firestore_integration_test_android.h"
#include "android/task_completion_source.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app_framework.h"
#include "firebase/firestore/firestore_errors.h"
#include "firebase_test_framework.h"
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/integer.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

// A specialization of the `MakePublic` template function to use in tests.
// This template function will be instantiated via the instantiation
// `Promise<std::string, int, TestAsyncFn>`.
template <>
std::string MakePublic<std::string, int>(jni::Env& env,
                                         FirestoreInternal* firestore,
                                         const jni::Object& object) {
  return object.ToString(env);
}

namespace {

class PromiseTest : public FirestoreAndroidIntegrationTest {
 public:
  PromiseTest() : promises_(GetFirestoreInternal(TestFirestore())) {}

  void SetUp() override {
    FirestoreAndroidIntegrationTest::SetUp();
    jni::Env env = GetEnv();
    cancellation_token_source_ = CancellationTokenSource::Create(env);
    task_completion_source_ = TaskCompletionSource::Create(
        env, cancellation_token_source_.GetToken(env));
  }

  // An enum of asynchronous functions to use in tests, as required by
  // `FutureManager`.
  enum class AsyncFn {
    kFn = 0,
    kCount,  // Must be the last enum value.
  };

 protected:
  PromiseFactory<AsyncFn>& promises() { return promises_; }

  jni::Local<jni::Task> GetTask() {
    jni::Env env = GetEnv();
    return task_completion_source_.GetTask(env);
  }

  void SetTaskResult(int result) {
    jni::Env env = GetEnv();
    task_completion_source_.SetResult(env, jni::Integer::Create(env, result));
  }

  void SetTaskException(Error error_code, const std::string& error_message) {
    jni::Env env = GetEnv();
    task_completion_source_.SetException(
        env, ExceptionInternal::Create(env, error_code, error_message.c_str()));
  }

  void CancelTask() {
    jni::Env env = GetEnv();
    cancellation_token_source_.Cancel(env);
  }

  static jni::Env GetEnv() { return jni::Env(app_framework::GetJniEnv()); }

 private:
  PromiseFactory<AsyncFn> promises_;
  jni::Local<CancellationTokenSource> cancellation_token_source_;
  jni::Local<TaskCompletionSource> task_completion_source_;
};

// An (partial) implementation of `Promise::Completion` to use in unit tests.
// Tests can call `AwaitCompletion()` to wait for `CompleteWith()` to be
// invoked and then retrieve the values specified to that invocation of
// `CompleteWith` to validate their correctness.
//
// There are two specializations of this template class: `TestVoidCompletion`,
// where `PublicType` and `InternalType` are `void`, and
// `TestCompletion`, where `PublicType` and `InternalType` are not `void`. The
// latter specialization provides access to the "result" specified to
// `CompleteWith`.
template <typename PublicType, typename InternalType>
class TestCompletionBase : public Promise<PublicType,
                                          InternalType,
                                          PromiseTest::AsyncFn>::Completion {
 public:
  void CompleteWith(Error error_code,
                    const char* error_message,
                    PublicType* result) override {
    MutexLock lock(mutex_);
    FIREBASE_ASSERT(invocation_count_ == 0);
    invocation_count_++;
    error_code_ = error_code;
    error_message_ = error_message;
    HandleResult(result);
  }

  // Waits for `CompleteWith()` to be invoked. Returns `true` if an invocation
  // occurred prior to timing out or `false` otherwise.
  bool AwaitCompletion() {
    int cycles_remaining = kTimeOutMillis / kCheckIntervalMillis;
    while (true) {
      {
        MutexLock lock(mutex_);
        if (invocation_count_ > 0) {
          return true;
        }
      }

      if (ProcessEvents(kCheckIntervalMillis)) {
        return false;
      }

      cycles_remaining--;
      if (cycles_remaining == 0) {
        return false;
      }
    }
  }

  // Returns the number of times that `CompleteWith()` has been invoked.
  int invocation_count() const {
    MutexLock lock(mutex_);
    return invocation_count_;
  }

  // Returns the `error_code` that was specified to the first invocation of
  // `CompleteWith()`.
  Error error_code() const {
    MutexLock lock(mutex_);
    return error_code_;
  }

  // Returns the `error_message` that was specified to the first invocation of
  // `CompleteWith()`.
  const std::string& error_message() const {
    MutexLock lock(mutex_);
    return error_message_;
  }

 protected:
  // Invoked from `CompleteWith()` with the `result` argument. This method will
  // be invoked while having acquired the lock on `mutex_`.
  virtual void HandleResult(PublicType* result) = 0;

  // The mutex acquired by `CompleteWith()` when reading and writing the
  // instance variables from the value specified to `CompleteWith()`.
  mutable Mutex mutex_;

 private:
  int invocation_count_ = 0;
  Error error_code_ = Error::kErrorOk;
  std::string error_message_;
};

// A specialization of `TestCompletionBase` that handles non-void `PublicType`
// and `InternalType` values. The `result` specified to `CompleteWith()` is
// copied and stored in an instance variable and can be retrieved via the
// `result()` method.
template <typename PublicType, typename InternalType>
class TestCompletion : public TestCompletionBase<PublicType, InternalType> {
 public:
  // Returns the `result` that was specified to the first invocation of
  // `CompleteWith()`.
  PublicType* result() const {
    MutexLock lock(this->mutex_);
    return result_.get();
  }

 protected:
  void HandleResult(PublicType* result) override {
    if (result == nullptr) {
      result_.reset(nullptr);
    } else {
      result_ = std::make_unique<PublicType>(*result);
    }
  }

 private:
  std::unique_ptr<PublicType> result_;
};

// A specialization of `TestCompletionBase` that handles void `PublicType` and
// `InternalType` values.
class TestVoidCompletion : public TestCompletionBase<void, void> {
 public:
  // Returns the `result` that was specified to the first invocation of
  // `CompleteWith()`.
  void* result() const {
    MutexLock lock(this->mutex_);
    return result_;
  }

 protected:
  void HandleResult(void* result) override { result_ = result; }

 private:
  void* result_;
};

TEST_F(PromiseTest, FutureVoidShouldSucceedWhenTaskSucceeds) {
  jni::Env env = GetEnv();
  auto future = promises().NewFuture<void>(env, AsyncFn::kFn, GetTask());
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskResult(0);

  EXPECT_GT(WaitFor(future), 0);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), 0);
  EXPECT_EQ(future.result(), nullptr);
}

TEST_F(PromiseTest, FutureNonVoidShouldSucceedWhenTaskSucceeds) {
  jni::Env env = GetEnv();
  auto future =
      promises().NewFuture<std::string, int>(env, AsyncFn::kFn, GetTask());
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskResult(42);

  EXPECT_GT(WaitFor(future), 0);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), 0);
  EXPECT_EQ(*future.result(), "42");
}

TEST_F(PromiseTest, FutureVoidShouldFailWhenTaskFails) {
  jni::Env env = GetEnv();
  auto future = promises().NewFuture<void>(env, AsyncFn::kFn, GetTask());
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskException(Error::kErrorFailedPrecondition, "Simulated failure");

  EXPECT_GT(WaitFor(future), 0);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorFailedPrecondition);
  EXPECT_EQ(future.error_message(), std::string("Simulated failure"));
  EXPECT_EQ(future.result(), nullptr);
}

TEST_F(PromiseTest, FutureNonVoidShouldFailWhenTaskFails) {
  jni::Env env = GetEnv();
  auto future =
      promises().NewFuture<std::string, int>(env, AsyncFn::kFn, GetTask());
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskException(Error::kErrorFailedPrecondition, "Simulated failure");

  EXPECT_GT(WaitFor(future), 0);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorFailedPrecondition);
  EXPECT_EQ(future.error_message(), std::string("Simulated failure"));
  EXPECT_EQ(*future.result(), "");
}

TEST_F(PromiseTest, FutureVoidShouldCancelWhenTaskCancels) {
  jni::Env env = GetEnv();
  auto future = promises().NewFuture<void>(env, AsyncFn::kFn, GetTask());
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  CancelTask();

  EXPECT_GT(WaitFor(future), 0);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorCancelled);
  EXPECT_EQ(future.error_message(), std::string("cancelled"));
  EXPECT_EQ(future.result(), nullptr);
}

TEST_F(PromiseTest, FutureNonVoidShouldCancelWhenTaskCancels) {
  jni::Env env = GetEnv();
  auto future =
      promises().NewFuture<std::string, int>(env, AsyncFn::kFn, GetTask());
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  CancelTask();

  EXPECT_GT(WaitFor(future), 0);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorCancelled);
  EXPECT_EQ(future.error_message(), std::string("cancelled"));
  EXPECT_EQ(*future.result(), "");
}

TEST_F(PromiseTest, FutureVoidShouldCallCompletionWhenTaskSucceeds) {
  jni::Env env = GetEnv();
  TestVoidCompletion completion;
  auto future = promises().NewFuture<void, void>(env, AsyncFn::kFn, GetTask(),
                                                 &completion);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskResult(0);

  EXPECT_EQ(completion.AwaitCompletion(), true);
  EXPECT_EQ(completion.invocation_count(), 1);
  EXPECT_EQ(completion.error_code(), Error::kErrorOk);
  EXPECT_EQ(completion.error_message(), "");
  EXPECT_EQ(completion.result(), nullptr);
}

TEST_F(PromiseTest, FutureNonVoidShouldCallCompletionWhenTaskSucceeds) {
  jni::Env env = GetEnv();
  TestCompletion<std::string, int> completion;
  auto future = promises().NewFuture<std::string, int>(env, AsyncFn::kFn,
                                                       GetTask(), &completion);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskResult(42);

  EXPECT_EQ(completion.AwaitCompletion(), true);
  EXPECT_EQ(completion.invocation_count(), 1);
  EXPECT_EQ(completion.error_code(), Error::kErrorOk);
  EXPECT_EQ(completion.error_message(), "");
  EXPECT_EQ(*completion.result(), "42");
}

TEST_F(PromiseTest, FutureVoidShouldCallCompletionWhenTaskFails) {
  jni::Env env = GetEnv();
  TestVoidCompletion completion;
  auto future = promises().NewFuture<void, void>(env, AsyncFn::kFn, GetTask(),
                                                 &completion);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskException(Error::kErrorFailedPrecondition, "Simulated failure");

  EXPECT_EQ(completion.AwaitCompletion(), true);
  EXPECT_EQ(completion.invocation_count(), 1);
  EXPECT_EQ(completion.error_code(), Error::kErrorFailedPrecondition);
  EXPECT_EQ(completion.error_message(), "Simulated failure");
  EXPECT_EQ(completion.result(), nullptr);
}

TEST_F(PromiseTest, FutureNonVoidShouldCallCompletionWhenTaskFails) {
  jni::Env env = GetEnv();
  TestCompletion<std::string, int> completion;
  auto future = promises().NewFuture<std::string, int>(env, AsyncFn::kFn,
                                                       GetTask(), &completion);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  SetTaskException(Error::kErrorFailedPrecondition, "Simulated failure");

  EXPECT_EQ(completion.AwaitCompletion(), true);
  EXPECT_EQ(completion.invocation_count(), 1);
  EXPECT_EQ(completion.error_code(), Error::kErrorFailedPrecondition);
  EXPECT_EQ(completion.error_message(), "Simulated failure");
  EXPECT_EQ(completion.result(), nullptr);
}

TEST_F(PromiseTest, FutureVoidShouldCallCompletionWhenTaskCancels) {
  jni::Env env = GetEnv();
  TestVoidCompletion completion;
  auto future = promises().NewFuture<void, void>(env, AsyncFn::kFn, GetTask(),
                                                 &completion);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  CancelTask();

  EXPECT_EQ(completion.AwaitCompletion(), true);
  EXPECT_EQ(completion.invocation_count(), 1);
  EXPECT_EQ(completion.error_code(), Error::kErrorCancelled);
  EXPECT_EQ(completion.error_message(), "cancelled");
  EXPECT_EQ(completion.result(), nullptr);
}

TEST_F(PromiseTest, FutureNonVoidShouldCallCompletionWhenTaskCancels) {
  jni::Env env = GetEnv();
  TestCompletion<std::string, int> completion;
  auto future = promises().NewFuture<std::string, int>(env, AsyncFn::kFn,
                                                       GetTask(), &completion);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);

  CancelTask();

  EXPECT_EQ(completion.AwaitCompletion(), true);
  EXPECT_EQ(completion.invocation_count(), 1);
  EXPECT_EQ(completion.error_code(), Error::kErrorCancelled);
  EXPECT_EQ(completion.error_message(), "cancelled");
  EXPECT_EQ(completion.result(), nullptr);
}

TEST_F(PromiseTest, RegisterForTaskShouldNotCrashIfFirestoreWasDeleted) {
  jni::Env env = GetEnv();
  auto promise = promises().MakePromise<void>();
  DeleteFirestore(TestFirestore());

  promise.RegisterForTask(env, AsyncFn::kFn, GetTask());
}

TEST_F(PromiseTest, GetFutureShouldNotCrashIfFirestoreWasDeleted) {
  jni::Env env = GetEnv();
  auto promise = promises().MakePromise<void>();
  promise.RegisterForTask(env, AsyncFn::kFn, GetTask());
  DeleteFirestore(TestFirestore());

  auto future = promise.GetFuture();
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusInvalid);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
