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

#include "firestore/src/jni/task.h"

#include "android/cancellation_token_source.h"
#include "android/firestore_integration_test_android.h"
#include "android/task_completion_source.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/jni/throwable.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {

namespace {

class TaskTest : public FirestoreAndroidIntegrationTest {
 protected:
  Local<Task> CreateIncompleteTask(Env& env) {
    return TaskCompletionSource::Create(env).GetTask(env);
  }

  template <typename T>
  Local<Task> CreateSuccessfulTask(Env& env, T result) {
    auto tcs = TaskCompletionSource::Create(env);
    tcs.SetResult(env, result);
    return tcs.GetTask(env);
  }

  Local<Task> CreateSuccessfulTask(Env& env) {
    auto result = env.NewStringUtf("Fake Result");
    return CreateSuccessfulTask(env, result);
  }

  Local<Task> CreateFailedTask(Env& env, Throwable exception) {
    auto tcs = TaskCompletionSource::Create(env);
    tcs.SetException(env, exception);
    return tcs.GetTask(env);
  }

  Local<Task> CreateFailedTask(Env& env) {
    auto exception = CreateException(env, "Test Exception");
    return CreateFailedTask(env, exception);
  }

  Local<Task> CreateCancelledTask(Env& env) {
    auto cts = CancellationTokenSource::Create(env);
    auto tcs = TaskCompletionSource::Create(env, cts.GetToken(env));
    cts.Cancel(env);
    auto task = tcs.GetTask(env);
    // Wait for the `Task` to be "completed" because `Cancel()` marks the `Task`
    // as "completed" asynchronously.
    Await(env, task);
    return task;
  }
};

TEST_F(TaskTest, GetResultShouldReturnTheResult) {
  Env env;
  Local<String> result = env.NewStringUtf("Fake Result");
  Local<Task> task = CreateSuccessfulTask(env, result);

  Local<Object> actual_result = task.GetResult(env);

  ASSERT_THAT(actual_result, JavaEq(result));
}

TEST_F(TaskTest, GetExceptionShouldReturnTheException) {
  Env env;
  Local<Throwable> exception = CreateException(env, "Test Exception");
  Local<Task> task = CreateFailedTask(env, exception);

  Local<Throwable> actual_exception = task.GetException(env);

  ASSERT_THAT(actual_exception, JavaEq(exception));
}

// Tests for Task.IsComplete()

TEST_F(TaskTest, IsCompleteShouldReturnFalseForIncompleteTask) {
  Env env;
  Local<Task> task = CreateIncompleteTask(env);

  ASSERT_FALSE(task.IsComplete(env));
}

TEST_F(TaskTest, IsCompleteShouldReturnTrueForSucceededTask) {
  Env env;
  Local<Task> task = CreateSuccessfulTask(env);

  ASSERT_TRUE(task.IsComplete(env));
}

TEST_F(TaskTest, IsCompleteShouldReturnTrueForFailedTask) {
  Env env;
  Local<Task> task = CreateFailedTask(env);

  ASSERT_TRUE(task.IsComplete(env));
}

TEST_F(TaskTest, IsCompleteShouldReturnTrueForCanceledTask) {
  Env env;
  Local<Task> task = CreateCancelledTask(env);

  ASSERT_TRUE(task.IsComplete(env));
}

// Tests for Task.IsSuccessful()

TEST_F(TaskTest, IsSuccessfulShouldReturnFalseForIncompleteTask) {
  Env env;
  Local<Task> task = CreateIncompleteTask(env);

  ASSERT_FALSE(task.IsSuccessful(env));
}

TEST_F(TaskTest, IsSuccessfulShouldReturnTrueForSucceededTask) {
  Env env;
  Local<Task> task = CreateSuccessfulTask(env);

  ASSERT_TRUE(task.IsSuccessful(env));
}

TEST_F(TaskTest, IsSuccessfulShouldReturnFalseForFailedTask) {
  Env env;
  Local<Task> task = CreateFailedTask(env);

  ASSERT_FALSE(task.IsSuccessful(env));
}

TEST_F(TaskTest, IsSuccessfulShouldReturnFalseForCanceledTask) {
  Env env;
  Local<Task> task = CreateCancelledTask(env);

  ASSERT_FALSE(task.IsSuccessful(env));
}

// Tests for Task.IsCanceled()

TEST_F(TaskTest, IsCanceledShouldReturnFalseForIncompleteTask) {
  Env env;
  Local<Task> task = CreateIncompleteTask(env);

  ASSERT_FALSE(task.IsCanceled(env));
}

TEST_F(TaskTest, IsCanceledShouldReturnFalseForSucceededTask) {
  Env env;
  Local<Task> task = CreateSuccessfulTask(env);

  ASSERT_FALSE(task.IsCanceled(env));
}

TEST_F(TaskTest, IsCanceledShouldReturnFalseForFailedTask) {
  Env env;
  Local<Task> task = CreateFailedTask(env);

  ASSERT_FALSE(task.IsCanceled(env));
}

TEST_F(TaskTest, IsCanceledShouldReturnTrueForCanceledTask) {
  Env env;
  Local<Task> task = CreateCancelledTask(env);

  ASSERT_TRUE(task.IsCanceled(env));
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
