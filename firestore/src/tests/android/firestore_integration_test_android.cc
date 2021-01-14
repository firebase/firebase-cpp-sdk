#include "firestore/src/tests/android/firestore_integration_test_android.h"

#include "app/src/assert.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/tests/android/cancellation_token_source.h"
#include "firestore/src/tests/android/task_completion_source.h"

namespace firebase {
namespace firestore {

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
    : loader_(app()) {
  CancellationTokenSource::Initialize(loader_);
  TaskCompletionSource::Initialize(loader_);
  loader_.LoadClass(kExceptionClassName, kExceptionConstructor);
  FIREBASE_ASSERT(loader_.ok());
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
