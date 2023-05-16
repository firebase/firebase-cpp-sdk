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

#include "android/firestore_integration_test_android.h"

#include "android/cancellation_token_source.h"
#include "android/task_completion_source.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/string.h"

namespace firebase {
namespace firestore {

std::string ToDebugString(const jni::Object& object) {
  if (!object) {
    return "null";
  }
  jni::Env env;
  jni::ExceptionClearGuard block(env);
  return object.ToString(env);
}

namespace jni {

void PrintTo(const Object& object, std::ostream* os) {
  *os << ToDebugString(object);
}

}  // namespace jni

using jni::Constructor;
using jni::Env;
using jni::ExceptionClearGuard;
using jni::Local;
using jni::String;
using jni::Task;
using jni::Throwable;

namespace {

constexpr char kExceptionClassName[] = "java/lang/Exception";
Constructor<Throwable> kExceptionConstructor("(Ljava/lang/String;)V");

}  // namespace

FirestoreAndroidIntegrationTest::FirestoreAndroidIntegrationTest()
    : loader_(app()) {}

void FirestoreAndroidIntegrationTest::SetUp() {
  FirestoreIntegrationTest::SetUp();
  CancellationTokenSource::Initialize(loader_);
  TaskCompletionSource::Initialize(loader_);
  loader_.LoadClass(kExceptionClassName, kExceptionConstructor);
  ASSERT_TRUE(loader_.ok());
}

void FirestoreAndroidIntegrationTest::TearDown() {
  FailTestIfPendingException();
  FirestoreIntegrationTest::TearDown();
}

void FirestoreAndroidIntegrationTest::FailTestIfPendingException() {
  // Fail the test if there is a pending Java exception. Clear the pending
  // exception as well so that it doesn't bleed into the next test.
  Local<Throwable> pending_exception = env().ClearExceptionOccurred();
  if (!pending_exception) {
    return;
  }

  // Ignore the exception if an invocation of ClearCurrentExceptionAfterTest()
  // requested that it be ignored.
  if (env().IsSameObject(pending_exception, exception_to_clear_after_test_)) {
    return;
  }

  // Fail the test since the test completed with an unexpected pending
  // exception.
  std::string pending_exception_as_string = pending_exception.ToString(env());
  env().ExceptionClear();
  FAIL() << "Test completed with a pending Java exception: "
         << pending_exception_as_string;
}

Local<Throwable> FirestoreAndroidIntegrationTest::CreateException() {
  return CreateException(
      "Test exception created by "
      "FirestoreAndroidIntegrationTest::CreateException()");
}

Local<Throwable> FirestoreAndroidIntegrationTest::CreateException(
    const std::string& message) {
  ExceptionClearGuard block(env());
  Local<String> java_message = env().NewStringUtf(message);
  return env().New(kExceptionConstructor, java_message);
}

Local<Throwable> FirestoreAndroidIntegrationTest::ThrowException() {
  return ThrowException(
      "Test exception thrown by "
      "FirestoreAndroidIntegrationTest::ThrowException()");
}

Local<Throwable> FirestoreAndroidIntegrationTest::ThrowException(
    const std::string& message) {
  if (!env().ok()) {
    ADD_FAILURE() << "ThrowException() invoked while there is already a "
                     "pending exception";
    return {};
  }
  Local<Throwable> exception = CreateException(message);
  env().Throw(exception);
  return exception;
}

Local<Throwable>
FirestoreAndroidIntegrationTest::ClearCurrentExceptionAfterTest() {
  if (exception_to_clear_after_test_) {
    ADD_FAILURE() << "ClearCurrentExceptionAfterTest() may only be invoked at "
                     "most once per test";
    return {};
  }

  Local<Throwable> exception = env().ExceptionOccurred();
  if (!exception) {
    ADD_FAILURE() << "ClearCurrentExceptionAfterTest() must be invoked when "
                     "there is a pending Java exception";
    return {};
  }

  {
    ExceptionClearGuard exception_clear_guard(env());
    exception_to_clear_after_test_ = exception;
  }

  return exception;
}

void FirestoreAndroidIntegrationTest::Await(const Task& task) {
  int cycles = kTimeOutMillis / kCheckIntervalMillis;
  while (env().ok() && !task.IsComplete(env())) {
    if (ProcessEvents(kCheckIntervalMillis)) {
      std::cout << "WARNING: app receives an event requesting exit."
                << std::endl;
      break;
    }
    --cycles;
  }
  if (env().ok()) {
    EXPECT_GT(cycles, 0) << "Waiting for Task timed out.";
  }
}

}  // namespace firestore
}  // namespace firebase
