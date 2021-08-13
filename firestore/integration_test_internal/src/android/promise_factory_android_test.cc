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

#include "firestore/src/android/promise_factory_android.h"

#include <utility>

#include "android/firestore_integration_test_android.h"
#include "android/task_completion_source.h"
#include "firebase/future.h"
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/integer.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Integer;
using jni::Local;
using jni::Task;

// Since `PromiseFactory` acts as a "constructor" of `Promise` objects, its
// ability to create `Promise` objects is thoroughly tested in the unit tests
// for `Promise` and therefore the tests here only cover the other aspects of
// `PromiseFactory`, such as move semantics.

class PromiseFactoryTest : public FirestoreAndroidIntegrationTest {
 public:
  // An enum of asynchronous functions to use in tests, as required by
  // `FutureManager`.
  enum class AsyncFn {
    kFn,
    kCount,  // Must be the last enum value.
  };

 protected:
  void AssertCreatesValidFutures(Env& env,
                                 PromiseFactory<AsyncFn>& promise_factory) {
    Local<TaskCompletionSource> tcs = TaskCompletionSource::Create(env);
    Local<Task> task = tcs.GetTask(env);
    Future<void> future =
        promise_factory.NewFuture<void>(env, AsyncFn::kFn, task);
    EXPECT_EQ(future.status(), FutureStatus::kFutureStatusPending);
    tcs.SetResult(env, jni::Integer::Create(env, 42));
    Await(future);
    EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  }

  void AssertCreatesInvalidFutures(Env& env,
                                   PromiseFactory<AsyncFn>& promise_factory) {
    Local<TaskCompletionSource> tcs = TaskCompletionSource::Create(env);
    Local<Task> task = tcs.GetTask(env);
    Future<void> future =
        promise_factory.NewFuture<void>(env, AsyncFn::kFn, task);
    EXPECT_EQ(future.status(), FutureStatus::kFutureStatusInvalid);
  }
};

TEST_F(PromiseFactoryTest, CopyConstructor) {
  Firestore* firestore = TestFirestore();
  PromiseFactory<AsyncFn> promise_factory1(GetFirestoreInternal(firestore));

  PromiseFactory<AsyncFn> promise_factory2(promise_factory1);

  Env env;
  {
    SCOPED_TRACE("promise_factory1");
    AssertCreatesValidFutures(env, promise_factory1);
  }
  {
    SCOPED_TRACE("promise_factory2");
    AssertCreatesValidFutures(env, promise_factory2);
  }
}

TEST_F(PromiseFactoryTest, CopyConstructorWithDeletedFirestore) {
  Firestore* firestore = TestFirestore();
  PromiseFactory<AsyncFn> promise_factory1(GetFirestoreInternal(firestore));
  DeleteFirestore(firestore);

  PromiseFactory<AsyncFn> promise_factory2(promise_factory1);

  Env env;
  {
    SCOPED_TRACE("promise_factory1");
    AssertCreatesInvalidFutures(env, promise_factory1);
  }
  {
    SCOPED_TRACE("promise_factory2");
    AssertCreatesInvalidFutures(env, promise_factory2);
  }
}

TEST_F(PromiseFactoryTest, MoveConstructor) {
  Firestore* firestore = TestFirestore();
  PromiseFactory<AsyncFn> promise_factory1(GetFirestoreInternal(firestore));

  PromiseFactory<AsyncFn> promise_factory2(std::move(promise_factory1));

  Env env;
  AssertCreatesValidFutures(env, promise_factory2);
}

TEST_F(PromiseFactoryTest, MoveConstructorWithDeletedFirestore) {
  Firestore* firestore = TestFirestore();
  PromiseFactory<AsyncFn> promise_factory1(GetFirestoreInternal(firestore));
  DeleteFirestore(firestore);

  PromiseFactory<AsyncFn> promise_factory2(std::move(promise_factory1));

  Env env;
  AssertCreatesInvalidFutures(env, promise_factory2);
}

TEST_F(PromiseFactoryTest, ShouldCreateInvalidPromisesIfFirestoreIsDeleted) {
  Firestore* firestore = TestFirestore();
  PromiseFactory<AsyncFn> promise_factory(GetFirestoreInternal(firestore));
  DeleteFirestore(firestore);
  Env env;

  AssertCreatesInvalidFutures(env, promise_factory);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
