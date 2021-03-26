/*
 * Copyright 2016 Google LLC
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

#include "app/src/include/firebase/future.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ctime>
#include <functional>
#include <vector>

#include "app/src/reference_counted_future_impl.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "app/src/time.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;

// Namespace to use to access library components under test.
#if !defined(TEST_FIREBASE_NAMESPACE)
#define TEST_FIREBASE_NAMESPACE firebase
#endif  // !defined(TEST_FIREBASE_NAMESPACE)

namespace TEST_FIREBASE_NAMESPACE {
namespace detail {
namespace testing {

struct TestResult {
  int number;
  std::string text;
};

class FutureTest : public ::testing::Test {
 protected:
  enum FutureTestFn { kFutureTestFnOne, kFutureTestFnTwo, kFutureTestFnCount };

  FutureTest() : future_impl_(kFutureTestFnCount) {}
  void SetUp() override {
    handle_ = future_impl_.SafeAlloc<TestResult>();
    future_ = MakeFuture(&future_impl_, handle_);
  }

 public:
  ReferenceCountedFutureImpl future_impl_;
  SafeFutureHandle<TestResult> handle_;
  Future<TestResult> future_;
};

// Some arbitrary result and error values.
const int kResultNumber = 8675309;
const int kResultError = -1729;
const char* const kResultText = "Hello, world!";

// Check that a future can be completed by the same thread.
TEST_F(FutureTest, TestFutureCompletesInSameThread) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  future_impl_.Complete<TestResult>(handle_, 0, [](TestResult* data) {
    data->number = kResultNumber;
    data->text = kResultText;
  });

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
}

static void FutureCallback(TestResult* data) {
  data->number = kResultNumber;
  data->text = kResultText;
}

// Check that the future completion can be done with a callback function
// instead of a lambda.
TEST_F(FutureTest, TestFutureCompletesWithCallback) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  future_impl_.Complete<TestResult>(handle_, 0, FutureCallback);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
}

// Check that the LastResult() futures are properly set and completed.
TEST_F(FutureTest, TestLastResult) {
  const auto handle = future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);

  Future<TestResult> future = static_cast<const Future<TestResult>&>(
      future_impl_.LastResult(kFutureTestFnOne));
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));

  future_impl_.Complete(handle, 0);

  EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
}

// Check that CompleteWithResult() works (i.e. data copy instead of lambda).
TEST_F(FutureTest, TestCompleteWithCopy) {
  TestResult result;
  result.number = kResultNumber;
  result.text = kResultText;
  future_impl_.CompleteWithResult<TestResult>(handle_, 0, result);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
}

// Check that Complete() with a lambda works.
TEST_F(FutureTest, TestCompleteWithLambda) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  future_impl_.Complete<TestResult>(handle_, 0, [](TestResult* data) {
    data->number = kResultNumber;
    data->text = kResultText;
  });

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
}

// Check that Complete() with a lambda with a capture works.
TEST_F(FutureTest, TestCompleteWithLambdaCapture) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
  bool captured = true;
  future_impl_.Complete<TestResult>(handle_, 0, [&captured](TestResult* data) {
    data->number = kResultNumber;
    data->text = kResultText;
    captured = true;
  });

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
  EXPECT_THAT(captured, Eq(true));
}

// Test that the result of a Pending future is null.
TEST_F(FutureTest, TestPendingResultIsNull) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
  EXPECT_THAT(future_.result(), IsNull());
  EXPECT_THAT(future_.result_void(), IsNull());
}

// Check that a future can be completed from another thread.
TEST_F(FutureTest, TestFutureCompletesInAnotherThread) {
  Thread child(
      [](void* test_void) {
        FutureTest* test = static_cast<FutureTest*>(test_void);
        test->future_impl_.Complete<TestResult>(test->handle_, 0,
                                                [](TestResult* data) {
                                                  data->number = kResultNumber;
                                                  data->text = kResultText;
                                                });
      },
      this);
  child.Join();  // Blocks until the thread function is done

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
}

// Check that the future error can be set.
TEST_F(FutureTest, TestSettingErrorValue) {
  future_impl_.Complete(handle_, kResultError);
  EXPECT_THAT(future_.error(), Eq(kResultError));
}

// Check that the void and typed results match.
TEST_F(FutureTest, TestTypedAndVoidMatch) {
  future_impl_.Complete(handle_, kResultError);

  EXPECT_THAT(future_.result(), NotNull());
  EXPECT_THAT(future_.result_void(), NotNull());
  EXPECT_THAT(future_.result(), Eq(future_.result_void()));
}

TEST_F(FutureTest, TestReleasedBackingData) {
  FutureHandleId id;
  {
    Future<TestResult> future;
    {
      SafeFutureHandle<TestResult> handle =
          future_impl_.SafeAlloc<TestResult>();
      EXPECT_TRUE(future_impl_.ValidFuture(handle));
      id = handle.get().id();
      EXPECT_TRUE(future_impl_.ValidFuture(id));
      future = MakeFuture(&future_impl_, handle);
      EXPECT_TRUE(future_impl_.ValidFuture(handle));
      EXPECT_TRUE(future_impl_.ValidFuture(id));
    }
    EXPECT_TRUE(future_impl_.ValidFuture(id));
  }
  EXPECT_FALSE(future_impl_.ValidFuture(id));
}

TEST_F(FutureTest, TestDetachFutureHandle) {
  FutureHandleId id;
  {
    Future<TestResult> future;
    SafeFutureHandle<TestResult> handle = future_impl_.SafeAlloc<TestResult>();
    EXPECT_TRUE(future_impl_.ValidFuture(handle));
    id = handle.get().id();
    EXPECT_TRUE(future_impl_.ValidFuture(id));
    future = MakeFuture(&future_impl_, handle);
    EXPECT_TRUE(future_impl_.ValidFuture(handle));
    EXPECT_TRUE(future_impl_.ValidFuture(id));
    future = Future<TestResult>();
    EXPECT_TRUE(future_impl_.ValidFuture(handle));
    EXPECT_TRUE(future_impl_.ValidFuture(id));
    handle.Detach();
    EXPECT_FALSE(future_impl_.ValidFuture(handle));
    EXPECT_FALSE(future_impl_.ValidFuture(id));
  }
  EXPECT_FALSE(future_impl_.ValidFuture(id));
}

// Test that a future becomes invalid when you release it.
TEST_F(FutureTest, TestReleasedFutureGoesInvalid) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
  future_.Release();
  EXPECT_THAT(future_.status(), Eq(kFutureStatusInvalid));
}

// Test that an invalid future returns an error.
TEST_F(FutureTest, TestReleasedFutureHasError) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
  future_.Release();
  EXPECT_THAT(future_.status(), Eq(kFutureStatusInvalid));
  EXPECT_THAT(future_.error(), Ne(0));
}

TEST_F(FutureTest, TestCompleteSetsStatusToComplete) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  future_impl_.Complete<TestResult>(handle_, 0, [](TestResult* data) {
    data->number = kResultNumber;
    data->text = kResultText;
  });

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
}

// Can't mock a simple function pointer, so we use these globals to ensure
// expectations about the callback running.
static int g_callback_times_called = -99;
static int g_callback_result_number = -99;
static void* g_callback_user_data = nullptr;

// Test whether an OnCompletion callback is called when the future is completed
// with the templated version of Complete().
TEST_F(FutureTest, TestCallbackCalledWhenSettingResult) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.OnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

// Test whether an AddOnCompletion callback is called when the future is
// completed with the templated version of Complete().
TEST_F(FutureTest, TestAddCallbackCalledWhenSettingResult) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.AddOnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

// Test whether an OnCompletion callback is called when the future is completed
// with a lambda with a capture.
TEST_F(FutureTest, TestCallbackCalledWithTypedLambdaCapture) {
  int callback_times_called = 0;
  int callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.OnCompletion([&](const Future<TestResult>& result) {
    callback_times_called++;
    callback_result_number = result.result()->number;
  });

  // Callback should not be called until it is completed.
  EXPECT_THAT(callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(callback_times_called, Eq(1));
  EXPECT_THAT(callback_result_number, Eq(kResultNumber));
}

// Test whether an AddOnCompletion callback is called when the future is
// completed with a lambda with a capture.
TEST_F(FutureTest, TestAddCallbackCalledWithTypedLambdaCapture) {
  int callback_times_called = 0;
  int callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.AddOnCompletion([&](const Future<TestResult>& result) {
    callback_times_called++;
    callback_result_number = result.result()->number;
  });

  // Callback should not be called until it is completed.
  EXPECT_THAT(callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(callback_times_called, Eq(1));
  EXPECT_THAT(callback_result_number, Eq(kResultNumber));
}

// Test whether an OnCompletion callback is called when the future is completed
// with a lambda with a capture.
TEST_F(FutureTest, TestCallbackCalledWithBaseLambdaCapture) {
  int callback_times_called = 0;

  // Set the callback before setting the status to complete.
  static_cast<FutureBase>(future_).OnCompletion(
      [&](const FutureBase& result) { callback_times_called++; });

  // Callback should not be called until it is completed.
  EXPECT_THAT(callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(callback_times_called, Eq(1));
}

// Test whether an AddOnCompletion callback is called when the future is
// completed with a lambda with a capture.
TEST_F(FutureTest, TestAddCallbackCalledWithBaseLambdaCapture) {
  int callback_times_called = 0;

  // Set the callback before setting the status to complete.
  static_cast<FutureBase>(future_).AddOnCompletion(
      [&](const FutureBase& result) { callback_times_called++; });

  // Callback should not be called until it is completed.
  EXPECT_THAT(callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(callback_times_called, Eq(1));
}

void OnCompletionCallback(const Future<TestResult>& result,
                          void* /*user_data*/) {
  g_callback_times_called++;
  g_callback_result_number = result.result()->number;
}

// Test whether an OnCompletion callback is called when the callback is a
// function pointer instead of a lambda.
TEST_F(FutureTest, TestCallbackCalledWhenFunctionPointer) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.OnCompletion(OnCompletionCallback, nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

// Test whether an AddOnCompletion callback is called when the callback is a
// function pointer instead of a lambda.
TEST_F(FutureTest, TestAddCallbackCalledWhenFunctionPointer) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.AddOnCompletion(OnCompletionCallback, nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

// Test whether an OnCompletion callback is called when the future is completed
// with the non-templated version of Complete().
TEST_F(FutureTest, TestCallbackCalledWhenNotSettingResults) {
  g_callback_times_called = 0;

  // Set the callback before setting the status to complete.
  future_.OnCompletion([](const Future<TestResult>& result,
                          void*) { g_callback_times_called++; },
                       nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(1));
}

// Test whether an AddOnCompletion callback is called when the future is
// completed with the non-templated version of Complete().
TEST_F(FutureTest, TestAddCallbackCalledWhenNotSettingResults) {
  g_callback_times_called = 0;

  // Set the callback before setting the status to complete.
  future_.AddOnCompletion([](const Future<TestResult>& result,
                             void*) { g_callback_times_called++; },
                          nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(1));
}

// Test whether an OnCompletion callback is called even if the future was
// already completed before OnCompletion() was called.
TEST_F(FutureTest, TestCallbackCalledWhenAlreadyComplete) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  // Callback should not be called until the callback is set.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  // Set the callback *after* the future was already completed.
  future_.OnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Ensure the callback was still called.
  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

// Test whether an AddOnCompletion callback is called even if the future was
// already completed before AddOnCompletion() was set.
TEST_F(FutureTest, TestAddCallbackCalledWhenAlreadyComplete) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  // Callback should not be called until the callback is set.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  // Set the callback *after* the future was already completed.
  future_.AddOnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Ensure the callback was still called.
  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

TEST_F(FutureTest, TestCallbackCalledFromAnotherThread) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  future_.OnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  Thread child(
      [](void* test_void) {
        FutureTest* test = static_cast<FutureTest*>(test_void);
        test->future_impl_.Complete<TestResult>(
            test->handle_, 0,
            [](TestResult* data) { data->number = kResultNumber; });
      },
      this);

  child.Join();  // Blocks until the thread function is done
  // Ensure the callback was still called.
  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

TEST_F(FutureTest, TestAddCallbackCalledFromAnotherThread) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  future_.AddOnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  Thread child(
      [](void* test_void) {
        FutureTest* test = static_cast<FutureTest*>(test_void);
        test->future_impl_.Complete<TestResult>(
            test->handle_, 0,
            [](TestResult* data) { data->number = kResultNumber; });
      },
      this);

  child.Join();  // Blocks until the fiber function is done
  // Ensure the callback was still called.
  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

TEST_F(FutureTest, TestCallbackUserData) {
  g_callback_times_called = 0;
  g_callback_user_data = nullptr;
  future_.OnCompletion(
      [](const Future<TestResult>&, void* user_data) {
        g_callback_times_called++;
        g_callback_user_data = user_data;
      },
      this);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_user_data, Eq(this));
}

TEST_F(FutureTest, TestAddCallbackUserData) {
  g_callback_times_called = 0;
  g_callback_user_data = nullptr;
  future_.AddOnCompletion(
      [](const Future<TestResult>&, void* user_data) {
        g_callback_times_called++;
        g_callback_user_data = user_data;
      },
      this);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_user_data, Eq(this));
}

TEST_F(FutureTest, TestCallbackUserDataFromBaseClass) {
  g_callback_times_called = 0;
  g_callback_user_data = nullptr;
  static_cast<FutureBase>(future_).OnCompletion(
      [](const FutureBase&, void* user_data) {
        g_callback_times_called++;
        g_callback_user_data = user_data;
      },
      this);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_user_data, Eq(this));
}

TEST_F(FutureTest, TestAddCallbackUserDataFromBaseClass) {
  g_callback_times_called = 0;
  g_callback_user_data = nullptr;
  static_cast<FutureBase>(future_).AddOnCompletion(
      [](const FutureBase&, void* user_data) {
        g_callback_times_called++;
        g_callback_user_data = user_data;
      },
      this);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_user_data, Eq(this));
}

TEST_F(FutureTest, TestUntypedCallback) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;
  static_cast<FutureBase>(future_).OnCompletion(
      [](const FutureBase& untyped_result, void*) {
        g_callback_times_called++;
        const Future<TestResult>& typed_result =
            reinterpret_cast<const Future<TestResult>&>(untyped_result);
        g_callback_result_number = typed_result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

TEST_F(FutureTest, TestAddUntypedCallback) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;
  static_cast<FutureBase>(future_).AddOnCompletion(
      [](const FutureBase& untyped_result, void*) {
        g_callback_times_called++;
        const Future<TestResult>& typed_result =
            reinterpret_cast<const Future<TestResult>&>(untyped_result);
        g_callback_result_number = typed_result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

// Test that you can deal with many simultaneous Futures at once.
TEST_F(FutureTest, TestSimultaneousFutures) {
  const int kMilliseconds = 1000;
  const int kNumToTest = 100;

  // Initialize a bunch of futures and threads.
  std::vector<SafeFutureHandle<TestResult>> handles_;
  std::vector<Future<TestResult>> futures_;
  std::vector<Thread*> children_;
  struct Context {
    FutureTest* test;
    SafeFutureHandle<TestResult> handle;
    int test_number;
  } thread_context[kNumToTest];
  for (int i = 0; i < kNumToTest; i++) {
    auto handle = future_impl_.SafeAlloc<TestResult>();
    handles_.push_back(handle);
    futures_.push_back(MakeFuture(&future_impl_, handle));
    auto* context = &thread_context[i];
    context->test = this;
    context->handle = handle;
    context->test_number = i;
    children_.push_back(new Thread(
        [](void* current_context_void) {
          Context* current_context =
              static_cast<Context*>(current_context_void);
          // Each thread should wait a moment, then set the result and
          // complete.
          internal::Sleep(rand() % kMilliseconds);  // NOLINT
          current_context->test->future_impl_.Complete<TestResult>(
              current_context->handle, 0, [current_context](TestResult* data) {
                data->number = kResultNumber + current_context->test_number;
              });
        },
        context));
  }
  // Give threads time to run.
  internal::Sleep(kMilliseconds);

  // Check that each future completed successfully, then clean it up.
  for (int i = 0; i < kNumToTest; i++) {
    children_[i]->Join();
    EXPECT_THAT(futures_[i].result()->number, Eq(kResultNumber + i));
    delete children_[i];
    children_[i] = nullptr;
  }
}

TEST_F(FutureTest, TestCallbackOnFutureOutOfScope) {
  g_callback_times_called = 0;
  g_callback_result_number = 0;

  // Set the callback before setting the status to complete.
  future_.OnCompletion(
      [](const Future<TestResult>& result, void*) {
        g_callback_times_called++;
        g_callback_result_number = result.result()->number;
      },
      nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_ = Future<TestResult>();
  handle_.Detach();
  // The Future we were holding onto is now out of scope.

  future_impl_.Complete<TestResult>(
      handle_, 0, [](TestResult* data) { data->number = kResultNumber; });

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_result_number, Eq(kResultNumber));
}

TEST_F(FutureTest, TestOverridingHandle) {
  // Ensure that FutureHandles can't be deallocated while still in use.
  // Generally, do this by allocating a handle into a function slot, then
  // allocating another handle into the same slot, and then creating a future
  // from the first handle. If all goes well it should be fine, but if the
  // handle was deallocated then making a future from it will fail.

  {
    // Basic test, create 2 FutureHandles in the same slot, then make Future
    // instances from both.
    SafeFutureHandle<TestResult> handle1 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    SafeFutureHandle<TestResult> handle2 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    Future<TestResult> future1 = MakeFuture(&future_impl_, handle1);
    EXPECT_EQ(future1.status(), kFutureStatusPending);
    Future<TestResult> future2 = MakeFuture(&future_impl_, handle2);
    EXPECT_EQ(future2.status(), kFutureStatusPending);
  }
  {
    // Same as above, but complete the first Future and make sure it doesn't
    // affect the second.
    SafeFutureHandle<TestResult> handle1 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    SafeFutureHandle<TestResult> handle2 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    future_impl_.Complete<TestResult>(
        handle1, 0, [](TestResult* data) { data->number = kResultNumber; });
    Future<TestResult> future1 = MakeFuture(&future_impl_, handle1);
    EXPECT_EQ(future1.status(), kFutureStatusComplete);
    EXPECT_EQ(future1.result()->number, kResultNumber);
    Future<TestResult> future2 = MakeFuture(&future_impl_, handle2);
    EXPECT_EQ(future2.status(), kFutureStatusPending);
  }
  {
    // Complete the second Future and make sure it doesn't affect the first.
    SafeFutureHandle<TestResult> handle1 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    SafeFutureHandle<TestResult> handle2 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    future_impl_.Complete<TestResult>(
        handle2, 0, [](TestResult* data) { data->number = kResultNumber; });
    Future<TestResult> future1 = MakeFuture(&future_impl_, handle1);
    EXPECT_EQ(future1.status(), kFutureStatusPending);
    Future<TestResult> future2 = MakeFuture(&future_impl_, handle2);
    EXPECT_EQ(future2.status(), kFutureStatusComplete);
    EXPECT_EQ(future2.result()->number, kResultNumber);
  }
  {
    // Ensure that both Futures can be completed with different result values.
    SafeFutureHandle<TestResult> handle1 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    SafeFutureHandle<TestResult> handle2 =
        future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
    future_impl_.Complete<TestResult>(
        handle1, 0, [](TestResult* data) { data->number = kResultNumber; });
    future_impl_.Complete<TestResult>(
        handle2, 0, [](TestResult* data) { data->number = 2 * kResultNumber; });
    Future<TestResult> future1 = MakeFuture(&future_impl_, handle1);
    EXPECT_EQ(future1.status(), kFutureStatusComplete);
    EXPECT_EQ(future1.result()->number, kResultNumber);
    Future<TestResult> future2 = MakeFuture(&future_impl_, handle2);
    EXPECT_EQ(future2.status(), kFutureStatusComplete);
    EXPECT_EQ(future2.result()->number, 2 * kResultNumber);
  }
}

TEST_F(FutureTest, TestHighQps) {
  const int kNumToTest = 10000;

  future_ = Future<TestResult>();

  std::vector<Thread*> children_;
  for (int i = 0; i < kNumToTest; i++) {
    children_.push_back(new Thread(
        [](void* this_void) {
          FutureTest* this_ = reinterpret_cast<FutureTest*>(this_void);
          SafeFutureHandle<TestResult> handle =
              this_->future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);

          this_->future_impl_.Complete<TestResult>(
              handle, 0,
              [](TestResult* data) { data->number = kResultNumber; });
          Future<TestResult> future = MakeFuture(&this_->future_impl_, handle);
        },
        this));
  }
  for (int i = 0; i < kNumToTest; i++) {
    children_[i]->Join();
    delete children_[i];
    children_[i] = nullptr;
  }
}

// Test that accessing a future as const compiles.
TEST_F(FutureTest, TestConstFuture) {
  g_callback_times_called = 0;

  const Future<TestResult> const_future = future_;
  // Set the callback before setting the status to complete.
  const_future.OnCompletion([](const Future<TestResult>& result,
                               void*) { g_callback_times_called++; },
                            nullptr);
  const_future.AddOnCompletion([](const Future<TestResult>& result,
                                  void*) { g_callback_times_called++; },
                               nullptr);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(2));
}

// Test that we can remove an AddOnCompletion callback using RemoveOnCompletion.
TEST_F(FutureTest, TestAddCompletionCallbackRemoval) {
  g_callback_times_called = 0;
  auto callback_handle = future_.AddOnCompletion(
      [&](const Future<TestResult>&) { ++g_callback_times_called; });
  future_.RemoveOnCompletion(callback_handle);

  future_impl_.Complete(handle_, 0);

  EXPECT_THAT(g_callback_times_called, Eq(0));
}

// Test that multiple callbacks are called in the documented order,
// and that OnCompletion() doesn't interfere with AddOnCompletion()
// and vice versa.
TEST_F(FutureTest, TestCallbackOrdering) {
  std::vector<int> ordered_results;

  // Set the callback before setting the status to complete.
  future_.AddOnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(5); });
  future_.AddOnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(4); });
  auto callback_handle = future_.AddOnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(3); });
  future_.OnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(-3); });
  future_.AddOnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(2); });
  future_.OnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(-2); });
  future_.OnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(-1); });
  future_.AddOnCompletion(
      [&](const Future<TestResult>&) { ordered_results.push_back(1); });
  future_.RemoveOnCompletion(callback_handle);

  // Callback should not be called until it is completed.
  EXPECT_THAT(ordered_results, Eq(std::vector<int>{}));

  future_impl_.Complete(handle_, 0);

  // The last OnCompletionCallback (-1) should get called before AddOnCompletion
  // callbacks, and the AddOnCompletion callbacks should get called in
  // the order that they were registered (5, 4, 3, 2, 1), except that callbacks
  // which have been removed (3) should not be called.
  EXPECT_THAT(ordered_results, Eq(std::vector<int>{-1, 5, 4, 2, 1}));
}

// Verify futures are not leaked when copied, using the implicit memory leak
// checker. When futures are allocated in the same LastResult function slot, a
// new handle should be allocated the old handle should be removed and hence be
// invalid.
TEST_F(FutureTest, VerifyNotLeakedWhenOverridden) {
  FutureHandleId id;
  {
    SafeFutureHandle<TestResult> last_result_handle;
    last_result_handle = future_impl_.SafeAlloc<TestResult>(0);
    EXPECT_THAT(last_result_handle.get(),
                Ne(SafeFutureHandle<TestResult>::kInvalidHandle.get()));
    EXPECT_TRUE(future_impl_.ValidFuture(last_result_handle));
    id = last_result_handle.get().id();
  }
  {
    auto new_last_result_handle = future_impl_.SafeAlloc<TestResult>(0);
    EXPECT_THAT(new_last_result_handle.get(),
                Ne(SafeFutureHandle<TestResult>::kInvalidHandle.get()));
    EXPECT_FALSE(future_impl_.ValidFuture(id));
  }
}

// Verify that trying to complete a future twice causes death.
TEST_F(FutureTest, VerifyCompletingFutureTwiceAsserts) {
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  future_impl_.Complete(handle_, 0);
  EXPECT_DEATH(future_impl_.Complete(handle_, 0), "SIGABRT");
}

// Verify that IsSafeToDelete() return the correct value.
TEST_F(FutureTest, VerifyIsSafeToDelete) {
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    EXPECT_TRUE(impl.IsSafeToDelete());
  }

  // Test if a FutureHandle is allocated but no external Future has ever
  // reference it.
  // Note: This will result in a warning message "Future with handle x still
  // exists though its backing API y is being deleted" because there is no
  // chance to remove the backing at all.
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    {
      auto handle_pending = impl.SafeAlloc<TestResult>();
      EXPECT_FALSE(impl.IsSafeToDelete());
      impl.Complete(handle_pending, 0);
    }
    EXPECT_TRUE(impl.IsSafeToDelete());
  }

  // Test if a FutureHandle is allocated and an external Future has referenced
  // it.
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    {
      auto handle_complete = impl.SafeAlloc<TestResult>();
      EXPECT_FALSE(impl.IsSafeToDelete());
      Future<TestResult>* future =
          new Future<TestResult>(&impl, handle_complete.get());
      EXPECT_FALSE(impl.IsSafeToDelete());
      delete future;
    }
    // This is true because ReferenceCountedFutureImpl::last_results_ never
    // keeps a copy of this future.  That is, the backing will be deleted when
    // the future above is deleted.
    EXPECT_TRUE(impl.IsSafeToDelete());
  }

  // Test if a FutureHandle is allocated with function id but no external
  // Future has ever reference it.
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    {
      auto handle_fn_pending = impl.SafeAlloc<TestResult>(kFutureTestFnOne);
      EXPECT_FALSE(impl.IsSafeToDelete());
      impl.Complete(handle_fn_pending, 0);
    }
    EXPECT_TRUE(impl.IsSafeToDelete());
  }

  // Test if a FutureHandle is allocated with function id and an external Future
  // has referenced it.
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    auto handle_fn_complete = impl.SafeAlloc<TestResult>(kFutureTestFnOne);
    EXPECT_FALSE(impl.IsSafeToDelete());
    Future<TestResult>* future =
        new Future<TestResult>(&impl, handle_fn_complete.get());
    EXPECT_FALSE(impl.IsSafeToDelete());
    delete future;
    // This is false because ReferenceCountedFutureImpl::last_results_ keeps
    // a copy of this future.
    EXPECT_FALSE(impl.IsSafeToDelete());
    impl.Complete(handle_fn_complete, 0);
    EXPECT_TRUE(impl.IsSafeToDelete());
  }

  // Test that a ReferenceCountedFutureImpl isn't considered for deletion while
  // it's running a user callback.
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    auto handle = impl.SafeAlloc<TestResult>(kFutureTestFnOne);
    EXPECT_FALSE(impl.IsSafeToDelete());
    Future<TestResult> future = MakeFuture(&impl, handle);
    EXPECT_FALSE(impl.IsSafeToDelete());

    Semaphore semaphore(0);
    future.OnCompletion([&](const Future<TestResult>& future) {
      EXPECT_FALSE(impl.IsSafeToDelete());  // Because the callback is running.
      semaphore.Post();
    });
    future.Release();
    EXPECT_FALSE(impl.IsSafeToDelete());
    impl.Complete(handle, 0, "");

    semaphore.Wait();

    // Note: despite the semaphore, the check for `impl.IsSafeToDelete` is racy
    // (it could be false if the check happens in-between when the semaphore
    // posts the signal and when user callback actually finishes running), which
    // necessitates sleeping.
    const int kSleepTimeMs = 50;
    int timeout_left = 1000;
    while (!impl.IsSafeToDelete() && timeout_left >= 0) {
      timeout_left -= kSleepTimeMs;
      internal::Sleep(kSleepTimeMs);
    }
    EXPECT_TRUE(impl.IsSafeToDelete());
  }

  // Like the test above, but with AddOnCompletion instead of OnCompletion.
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    auto handle = impl.SafeAlloc<TestResult>(kFutureTestFnOne);
    EXPECT_FALSE(impl.IsSafeToDelete());
    Future<TestResult> future = MakeFuture(&impl, handle);
    EXPECT_FALSE(impl.IsSafeToDelete());

    Semaphore semaphore(0);
    future.AddOnCompletion([&](const Future<TestResult>& future) {
      EXPECT_FALSE(impl.IsSafeToDelete());  // Because the callback is running.
      semaphore.Post();
    });
    future.Release();
    EXPECT_FALSE(impl.IsSafeToDelete());
    impl.Complete(handle, 0, "");

    semaphore.Wait();

    // Note: despite the semaphore, the check for `impl.IsSafeToDelete` is racy
    // (it could be false if the check happens in-between when the semaphore
    // posts the signal and when user callback actually finishes running), which
    // necessitates sleeping.
    const int kSleepTimeMs = 50;
    int timeout_left = 1000;
    while (!impl.IsSafeToDelete() && timeout_left >= 0) {
      timeout_left -= kSleepTimeMs;
      internal::Sleep(kSleepTimeMs);
    }
    EXPECT_TRUE(impl.IsSafeToDelete());
  }
}

// Verify that IsReferencedExternally() returns the correct value.
TEST_F(FutureTest, VerifyIsReferencedExternally) {
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    EXPECT_FALSE(impl.IsReferencedExternally());
  }

  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    {
      EXPECT_FALSE(impl.IsReferencedExternally());
      auto handle = impl.SafeAlloc<TestResult>();
      EXPECT_TRUE(impl.IsReferencedExternally());
      Future<TestResult>* future = new Future<TestResult>(&impl, handle.get());
      EXPECT_TRUE(impl.IsReferencedExternally());
      delete future;
    }
    EXPECT_FALSE(impl.IsReferencedExternally());
  }

  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    {
      EXPECT_FALSE(impl.IsReferencedExternally());
      auto handle = impl.SafeAlloc<TestResult>();
      EXPECT_TRUE(impl.IsReferencedExternally());
      Future<TestResult>* future = new Future<TestResult>(&impl, handle.get());
      EXPECT_TRUE(impl.IsReferencedExternally());
      impl.Complete(handle, 0);
      EXPECT_TRUE(impl.IsReferencedExternally());
      delete future;
    }
    EXPECT_FALSE(impl.IsReferencedExternally());
  }
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    {
      EXPECT_FALSE(impl.IsReferencedExternally());
      auto handle = impl.SafeAlloc<TestResult>(kFutureTestFnOne);
      EXPECT_TRUE(impl.IsReferencedExternally());
      {
        Future<TestResult>* future =
            new Future<TestResult>(&impl, handle.get());
        delete future;
      }
      EXPECT_TRUE(impl.IsReferencedExternally());
      handle.Detach();
      EXPECT_FALSE(impl.IsReferencedExternally());
    }
    EXPECT_FALSE(impl.IsReferencedExternally());
  }
}

// Verify that when a ReferenceCountedFutureImpl is deleted, any
// Futures it gave out are invalidated (rather than crashing).
TEST_F(FutureTest, VerifyFutureInvalidatedWhenImplIsDeleted) {
  Future<TestResult> future_pending, future_complete, future_fn_pending,
      future_fn_complete, future_invalid;
  {
    ReferenceCountedFutureImpl impl(kFutureTestFnCount);
    // Allocate a variety of futures, completing some of them.
    SafeFutureHandle<TestResult> handle_pending, handle_complete,
        handle_fn_pending, handle_fn_complete;

    handle_pending = impl.SafeAlloc<TestResult>();
    future_pending = MakeFuture(&impl, handle_pending);

    handle_complete = impl.SafeAlloc<TestResult>();
    future_complete = MakeFuture(&impl, handle_complete);
    impl.Complete(handle_complete, 0);

    handle_fn_pending = impl.SafeAlloc<TestResult>(kFutureTestFnOne);
    future_fn_pending = MakeFuture(&impl, handle_fn_pending);

    handle_fn_complete = impl.SafeAlloc<TestResult>(kFutureTestFnTwo);
    future_fn_complete = MakeFuture(&impl, handle_fn_complete);
    impl.Complete(handle_fn_complete, 0);

    EXPECT_THAT(future_invalid.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(future_pending.status(), Eq(kFutureStatusPending));
    EXPECT_THAT(future_complete.status(), Eq(kFutureStatusComplete));
    EXPECT_THAT(future_fn_pending.status(), Eq(kFutureStatusPending));
    EXPECT_THAT(future_fn_complete.status(), Eq(kFutureStatusComplete));
  }
  // Ensure that all different types/statuses of future are now invalid.
  EXPECT_THAT(future_invalid.status(), Eq(kFutureStatusInvalid));
  EXPECT_THAT(future_pending.status(), Eq(kFutureStatusInvalid));
  EXPECT_THAT(future_complete.status(), Eq(kFutureStatusInvalid));
  EXPECT_THAT(future_fn_pending.status(), Eq(kFutureStatusInvalid));
  EXPECT_THAT(future_fn_complete.status(), Eq(kFutureStatusInvalid));
}

// Verify that Future instances are cleaned up properly even if they've
// been copied and moved, even between FutureImpls, or released.
TEST_F(FutureTest, TestCleaningUpFuturesThatWereCopied) {
  Future<TestResult> future1, future2, future3;
  Future<TestResult> copy, move, release;
  Future<TestResult> move_c, copy_c;  // Constructor versions.
  {
    ReferenceCountedFutureImpl impl_a(kFutureTestFnCount);
    {
      ReferenceCountedFutureImpl impl_b(kFutureTestFnCount);
      // Allocate a variety of futures, completing some of them.
      SafeFutureHandle<TestResult> handle1, handle2, handle3;

      handle1 = impl_a.SafeAlloc<TestResult>();
      future1 = MakeFuture(&impl_a, handle1);

      handle2 = impl_a.SafeAlloc<TestResult>();
      future2 = MakeFuture(&impl_a, handle2);

      handle3 = impl_b.SafeAlloc<TestResult>();
      future3 = MakeFuture(&impl_b, handle3);

      EXPECT_THAT(future1.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(future2.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(future3.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(copy.status(), Eq(kFutureStatusInvalid));
      EXPECT_THAT(move.status(), Eq(kFutureStatusInvalid));

      // Make some copies/moves.
      copy = future3;
      move = std::move(future3);
      EXPECT_THAT(copy.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(move.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(future3.status(), Eq(kFutureStatusInvalid));  // NOLINT

      future1 = copy;
      future2 = move;  // actually a copy
      EXPECT_THAT(future1.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(future2.status(), Eq(kFutureStatusPending));

      release = copy;
      EXPECT_THAT(copy.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(release.status(), Eq(kFutureStatusPending));

      release.Release();
      EXPECT_THAT(future1.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(future2.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(copy.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(move.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(release.status(), Eq(kFutureStatusInvalid));

      // Ensure that the move/copy constructors also work.
      Future<TestResult> move_constructor(std::move(move));
      Future<TestResult> copy_constructor(copy);
      EXPECT_THAT(copy_constructor.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(copy.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(move_constructor.status(), Eq(kFutureStatusPending));

      move_c = std::move(move_constructor);
      copy_c = copy_constructor;
      EXPECT_THAT(copy_c.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(copy_constructor.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(copy.status(), Eq(kFutureStatusPending));
      EXPECT_THAT(move_c.status(), Eq(kFutureStatusPending));
    }
    // Ensure that all Futures are now invalid.
    EXPECT_THAT(future1.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(future2.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(future3.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(copy.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(move.status(), Eq(kFutureStatusInvalid));  // NOLINT
    EXPECT_THAT(copy_c.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(move_c.status(), Eq(kFutureStatusInvalid));
  }
}

// Test Wait() method (without callback), with infinite timeout.
TEST_F(FutureTest, TestFutureWaitInfinite) {
  Semaphore semaphore(0);
  using This = decltype(this);
  struct ThreadArgs {
    This test_fixture;
    Semaphore* semaphore;
  } args{this, &semaphore};
  Thread child(
      [](ThreadArgs* args_ptr) {
        args_ptr->semaphore->Wait();  // Wait until main thread is ready.
        args_ptr->test_fixture->future_impl_.Complete<TestResult>(
            args_ptr->test_fixture->handle_, 0, [&](TestResult* data) {
              internal::Sleep(/*milliseconds=*/100);
              data->number = kResultNumber;
              data->text = kResultText;
            });
      },
      &args);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  semaphore.Post();  // Allow other thread to continue.

  future_.Wait(FutureBase::kWaitTimeoutInfinite);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  ASSERT_THAT(future_.result(), Ne(nullptr));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));

  child.Join();  // Clean up.
}

// Test Wait() method with callback, with infinite timeout.
TEST_F(FutureTest, TestFutureWaitWithCallback) {
  g_callback_times_called = 0;
  g_callback_user_data = nullptr;
  future_.OnCompletion(
      [](const Future<TestResult>&, void* user_data) {
        g_callback_times_called++;
        g_callback_user_data = user_data;
      },
      this);

  // Callback should not be called until it is completed.
  EXPECT_THAT(g_callback_times_called, Eq(0));

  Semaphore semaphore(0);

  using This = decltype(this);
  struct ThreadArgs {
    This test_fixture;
    Semaphore* semaphore;
  } args{this, &semaphore};
  Thread child(
      [](ThreadArgs* args_ptr) {
        args_ptr->semaphore->Wait();  // Wait until main thread is ready.
        args_ptr->test_fixture->future_impl_.Complete<TestResult>(
            args_ptr->test_fixture->handle_, 0, [&](TestResult* data) {
              internal::Sleep(/*milliseconds=*/100);
              data->number = kResultNumber;
              data->text = kResultText;
            });
      },
      &args);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  semaphore.Post();  // Allow other thread to continue.

  future_.Wait(FutureBase::kWaitTimeoutInfinite);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  ASSERT_THAT(future_.result(), Ne(nullptr));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));

  EXPECT_THAT(g_callback_times_called, Eq(1));
  EXPECT_THAT(g_callback_user_data, Eq(this));

  child.Join();  // Clean up.
}

// Test Wait() method with lambda callback.
TEST_F(FutureTest, TestFutureWaitWithCallbackLambda) {
  int callback_times_called = 0;
  future_.OnCompletion(
      [&](const Future<TestResult>&) { callback_times_called++; });

  // Callback should not be called until it is completed.
  EXPECT_THAT(callback_times_called, Eq(0));

  Semaphore semaphore(0);

  using This = decltype(this);
  struct ThreadArgs {
    This test_fixture;
    Semaphore* semaphore;
  } args{this, &semaphore};
  Thread child(
      [](ThreadArgs* args_ptr) {
        args_ptr->semaphore->Wait();  // Wait until main thread is ready.
        args_ptr->test_fixture->future_impl_.Complete<TestResult>(
            args_ptr->test_fixture->handle_, 0, [&](TestResult* data) {
              internal::Sleep(/*milliseconds=*/100);
              data->number = kResultNumber;
              data->text = kResultText;
            });
      },
      &args);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  semaphore.Post();  // Allow other thread to continue.

  future_.Wait(FutureBase::kWaitTimeoutInfinite);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  ASSERT_THAT(future_.result(), Ne(nullptr));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));

  EXPECT_THAT(callback_times_called, Eq(1));

  child.Join();  // Clean up.
}

// Test Await() method, with infinite timeout.
TEST_F(FutureTest, TestFutureAwait) {
  Semaphore semaphore(0);
  using This = decltype(this);
  struct ThreadArgs {
    This test_fixture;
    Semaphore* semaphore;
  } args{this, &semaphore};
  Thread child(
      [](ThreadArgs* args_ptr) {
        args_ptr->semaphore->Wait();  // Wait until main thread is ready.
        args_ptr->test_fixture->future_impl_.Complete<TestResult>(
            args_ptr->test_fixture->handle_, 0, [&](TestResult* data) {
              internal::Sleep(/*milliseconds=*/100);
              data->number = kResultNumber;
              data->text = kResultText;
            });
      },
      &args);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  semaphore.Post();  // Allow other thread to continue.

  const TestResult* result = future_.Await(FutureBase::kWaitTimeoutInfinite);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  ASSERT_THAT(future_.result(), Ne(nullptr));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));
  ASSERT_THAT(result, Ne(nullptr));
  EXPECT_THAT(result->number, Eq(kResultNumber));
  EXPECT_THAT(result->text, Eq(kResultText));

  child.Join();  // Clean up.
}

// Test Await() method, with finite timeout.
TEST_F(FutureTest, TestFutureTimedAwait) {
  using This = decltype(this);
  Thread child(
      [](This test_fixture) {
        internal::Sleep(/*milliseconds=*/300);
        test_fixture->future_impl_.Complete<TestResult>(
            test_fixture->handle_, 0, [](TestResult* data) {
              data->number = kResultNumber;
              data->text = kResultText;
            });
      },
      this);

  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
  EXPECT_THAT(future_.result(), Eq(nullptr));

  const TestResult* result = future_.Await(100);  // Wait for 100ms.

  // Thread should not have completed yet, for another 200ms...
  EXPECT_THAT(result, Eq(nullptr));
  EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));

  result = future_.Await(500);  // Wait for 500ms.

  // Thread should have completed by now.
  ASSERT_THAT(result, Ne(nullptr));
  EXPECT_THAT(result->number, Eq(kResultNumber));
  EXPECT_THAT(result->text, Eq(kResultText));
  EXPECT_THAT(future_.status(), Eq(kFutureStatusComplete));
  EXPECT_THAT(future_.result()->number, Eq(kResultNumber));
  EXPECT_THAT(future_.result()->text, Eq(kResultText));

  child.Join();  // Clean up.
}

// Helper functions to get memory usage. Linux only.
namespace {
extern "C" int get_memory_used_kb() {
  int result = -1;
#ifdef __linux__
  FILE* file = fopen("/proc/self/status", "r");
  char line[128];

  while (fgets(line, sizeof(line), file) != nullptr) {
    if (strncmp(line, "VmSize:", 7) == 0) {
      const char* nchar = &line[strlen(line) - 1];
      bool got_num = false;
      while (nchar >= line) {
        if (!got_num) {
          if (isdigit(*nchar)) {
            got_num = true;
          }
        } else {
          if (!isdigit(*nchar)) {
            result = atoi(nchar);  // NOLINT
            break;
          }
        }
        nchar--;
      }
    }
  }
  fclose(file);
#endif  // __linux__
  return result;
}
}  // namespace

TEST_F(FutureTest, MemoryStressTest) {
  size_t kIterations = 4000000;  // 4 million

  int memory_usage_before = get_memory_used_kb();
  for (size_t i = 0; i < kIterations; ++i) {
    {
      SafeFutureHandle<TestResult> handle =
          i % 2 == 0 ? future_impl_.SafeAlloc<TestResult>()
                     : future_impl_.SafeAlloc<TestResult>(kFutureTestFnOne);
      {
        Future<TestResult> future = MakeFuture(&future_impl_, handle);
        EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
      }

      if (i % 2 != 0) {
        FutureBase future = future_impl_.LastResult(kFutureTestFnOne);
        EXPECT_THAT(future_.status(), Eq(kFutureStatusPending));
      }
      future_impl_.Complete<TestResult>(handle, 0, [i](TestResult* data) {
        data->number = kResultNumber + i;
        data->text = kResultText;
      });
      {
        Future<TestResult> future = MakeFuture(&future_impl_, handle);
        EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
        EXPECT_THAT(future.result()->number, Eq(kResultNumber + i));
        EXPECT_THAT(future.result()->text, Eq(kResultText));
      }
    }
    if (i % 2 != 0) {
      FutureBase future_base = future_impl_.LastResult(kFutureTestFnOne);
      Future<TestResult>& future =
          *static_cast<Future<TestResult>*>(&future_base);
      EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
      EXPECT_THAT(future.result()->number, Eq(kResultNumber + i));
      EXPECT_THAT(future.result()->text, Eq(kResultText));
    }
  }
  int memory_usage_after = get_memory_used_kb();

  if (memory_usage_before != -1 && memory_usage_after != -1) {
    // Ensure that after creating a few million futures, memory usage has not
    // changed by more than half a megabyte.
    const int kMaxAllowedMemoryChange = 512;  // in kilobytes
    EXPECT_NEAR(memory_usage_before, memory_usage_after,
                kMaxAllowedMemoryChange);
  }
}

}  // namespace testing
}  // namespace detail
}  // namespace TEST_FIREBASE_NAMESPACE
