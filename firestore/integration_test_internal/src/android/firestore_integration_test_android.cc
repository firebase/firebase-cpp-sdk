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
  // Fail the test if there is a pending Java exception. Clear the pending
  // exception as well so that it doesn't bleed into the next test.
  Env env;
  Local<Throwable> pending_exception = env.ClearExceptionOccurred();
  EXPECT_FALSE(pending_exception)
      << "Test completed with a pending Java exception: "
      << pending_exception.ToString(env);
  env.ExceptionClear();
  FirestoreIntegrationTest::TearDown();
}

Local<Throwable> FirestoreAndroidIntegrationTest::CreateException(
    Env& env, const std::string& message) {
  ExceptionClearGuard block(env);
  Local<String> java_message = env.NewStringUtf(message);
  return env.New(kExceptionConstructor, java_message);
}

void FirestoreAndroidIntegrationTest::Await(Env& env, const Task& task) {
  int cycles = kTimeOutMillis / kCheckIntervalMillis;
  while (env.ok() && !task.IsComplete(env)) {
    if (ProcessEvents(kCheckIntervalMillis)) {
      std::cout << "WARNING: app receives an event requesting exit."
                << std::endl;
      break;
    }
    --cycles;
  }
  if (env.ok()) {
    EXPECT_GT(cycles, 0) << "Waiting for Task timed out.";
  }
}

}  // namespace firestore
}  // namespace firebase
