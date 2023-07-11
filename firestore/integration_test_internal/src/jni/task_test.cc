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
  static Local<Task> CreateIncompleteTask() {
    return TaskCompletionSource::Create(env()).GetTask(env());
  }

  template <typename T>
  static Local<Task> CreateSuccessfulTask(T result) {
    auto tcs = TaskCompletionSource::Create(env());
    tcs.SetResult(env(), result);
    return tcs.GetTask(env());
  }

  static Local<Task> CreateSuccessfulTask() {
    Local<String> result = env().NewStringUtf("Fake Result");
    return CreateSuccessfulTask(result);
  }

  static Local<Task> CreateFailedTask(const Throwable& exception) {
    auto tcs = TaskCompletionSource::Create(env());
    tcs.SetException(env(), exception);
    return tcs.GetTask(env());
  }

  static Local<Task> CreateFailedTask() {
    return CreateFailedTask(CreateException());
  }

  static Local<Task> CreateCancelledTask() {
    auto cts = CancellationTokenSource::Create(env());
    auto tcs = TaskCompletionSource::Create(env(), cts.GetToken(env()));
    cts.Cancel(env());
    auto task = tcs.GetTask(env());
    // Wait for the `Task` to be "completed" because `Cancel()` marks the `Task`
    // as "completed" asynchronously.
    Await(task);
    return task;
  }
};

TEST_F(TaskTest, GetResultShouldReturnTheResult) {
  Local<String> result = env().NewStringUtf("Fake Result");
  Local<Task> task = CreateSuccessfulTask(result);

  Local<Object> actual_result = task.GetResult(env());

  ASSERT_THAT(actual_result, RefersToSameJavaObjectAs(result));
}

TEST_F(TaskTest, GetExceptionShouldReturnTheException) {
  Local<Throwable> exception = CreateException();
  Local<Task> task = CreateFailedTask(exception);

  Local<Throwable> actual_exception = task.GetException(env());

  ASSERT_THAT(actual_exception, RefersToSameJavaObjectAs(exception));
}

// Tests for Task.IsComplete()

TEST_F(TaskTest, IsCompleteShouldReturnFalseForIncompleteTask) {
  Local<Task> task = CreateIncompleteTask();

  ASSERT_FALSE(task.IsComplete(env()));
}

TEST_F(TaskTest, IsCompleteShouldReturnTrueForSucceededTask) {
  Local<Task> task = CreateSuccessfulTask();

  ASSERT_TRUE(task.IsComplete(env()));
}

TEST_F(TaskTest, IsCompleteShouldReturnTrueForFailedTask) {
  Local<Task> task = CreateFailedTask();

  ASSERT_TRUE(task.IsComplete(env()));
}

TEST_F(TaskTest, IsCompleteShouldReturnTrueForCanceledTask) {
  Local<Task> task = CreateCancelledTask();

  ASSERT_TRUE(task.IsComplete(env()));
}

// Tests for Task.IsSuccessful()

TEST_F(TaskTest, IsSuccessfulShouldReturnFalseForIncompleteTask) {
  Local<Task> task = CreateIncompleteTask();

  ASSERT_FALSE(task.IsSuccessful(env()));
}

TEST_F(TaskTest, IsSuccessfulShouldReturnTrueForSucceededTask) {
  Local<Task> task = CreateSuccessfulTask();

  ASSERT_TRUE(task.IsSuccessful(env()));
}

TEST_F(TaskTest, IsSuccessfulShouldReturnFalseForFailedTask) {
  Local<Task> task = CreateFailedTask();

  ASSERT_FALSE(task.IsSuccessful(env()));
}

TEST_F(TaskTest, IsSuccessfulShouldReturnFalseForCanceledTask) {
  Local<Task> task = CreateCancelledTask();

  ASSERT_FALSE(task.IsSuccessful(env()));
}

// Tests for Task.IsCanceled()

TEST_F(TaskTest, IsCanceledShouldReturnFalseForIncompleteTask) {
  Local<Task> task = CreateIncompleteTask();

  ASSERT_FALSE(task.IsCanceled(env()));
}

TEST_F(TaskTest, IsCanceledShouldReturnFalseForSucceededTask) {
  Local<Task> task = CreateSuccessfulTask();

  ASSERT_FALSE(task.IsCanceled(env()));
}

TEST_F(TaskTest, IsCanceledShouldReturnFalseForFailedTask) {
  Local<Task> task = CreateFailedTask();

  ASSERT_FALSE(task.IsCanceled(env()));
}

TEST_F(TaskTest, IsCanceledShouldReturnTrueForCanceledTask) {
  Local<Task> task = CreateCancelledTask();

  ASSERT_TRUE(task.IsCanceled(env()));
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
