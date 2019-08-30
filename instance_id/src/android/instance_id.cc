// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "instance_id/src/include/firebase/instance_id.h"

#include <jni.h>

#include <cstdint>
#include <string>

#include "app/memory/shared_ptr.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "instance_id/src/android/instance_id_internal.h"

namespace firebase {
namespace instance_id {

// clang-format off
#define INSTANCE_ID_METHODS(X)                                                \
  X(GetId, "getId", "()Ljava/lang/String;"),                                  \
  X(GetCreationTime, "getCreationTime", "()J"),                               \
  X(DeleteInstanceId, "deleteInstanceId", "()V"),                             \
  X(GetToken, "getToken", "(Ljava/lang/String;Ljava/lang/String;)"            \
    "Ljava/lang/String;"),                                                    \
  X(DeleteToken, "deleteToken", "(Ljava/lang/String;Ljava/lang/String;)V"),   \
  X(GetInstance, "getInstance", "(Lcom/google/firebase/FirebaseApp;)"         \
    "Lcom/google/firebase/iid/FirebaseInstanceId;",                           \
    firebase::util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(instance_id, INSTANCE_ID_METHODS)
METHOD_LOOKUP_DEFINITION(instance_id,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/iid/FirebaseInstanceId",
                         INSTANCE_ID_METHODS)

namespace {

// Number of times this module has been initialized.
static int g_initialization_count = 0;

static bool Initialize(const App& app) {
  if (g_initialization_count) {
    g_initialization_count++;
    return true;
  }
  JNIEnv* env = app.GetJNIEnv();
  if (!util::Initialize(env, app.activity())) return false;
  if (!instance_id::CacheMethodIds(env, app.activity())) {
    util::Terminate(env);
    return false;
  }
  g_initialization_count++;
  return true;
}

static void Terminate(const App& app) {
  if (!g_initialization_count) return;
  g_initialization_count--;
  if (!g_initialization_count) {
    JNIEnv* env = app.GetJNIEnv();
    instance_id::ReleaseClass(env);
    util::Terminate(env);
  }
}

// Maps an error message to an error code.
struct ErrorMessageToCode {
  const char* message;
  Error code;
};

// The Android implemenation of IID does not raise specific exceptions which
// means we can only use error strings to convert to error codes.
static Error ExceptionStringToError(const char* error_message) {
  static const ErrorMessageToCode kErrorMessageToCodes[] = {
      {/* ERROR_SERVICE_NOT_AVAILABLE */ "SERVICE_NOT_AVAILABLE",
       kErrorNoAccess},
      {/* ERROR_INSTANCE_ID_RESET */ "INSTANCE_ID_RESET", kErrorIdInvalid},
  };
  if (strlen(error_message)) {
    for (int i = 0; i < FIREBASE_ARRAYSIZE(kErrorMessageToCodes); ++i) {
      const auto& message_to_code = kErrorMessageToCodes[i];
      if (strcmp(message_to_code.message, error_message) == 0) {
        return message_to_code.code;
      }
    }
    return kErrorUnknown;
  }
  return kErrorNone;
}

}  // namespace

using internal::AsyncOperation;
using internal::InstanceIdInternal;

int64_t InstanceId::creation_time() const {
  if (!instance_id_internal_) return 0;

  JNIEnv* env = app().GetJNIEnv();
  return env->CallLongMethod(
      instance_id_internal_->java_instance_id(),
      instance_id::GetMethodId(instance_id::kGetCreationTime));
}

Future<std::string> InstanceId::GetId() const {
  if (!instance_id_internal_) return Future<std::string>();

  JNIEnv* env = app().GetJNIEnv();
  SharedPtr<AsyncOperation> operation =
      instance_id_internal_->AddOperation(new AsyncOperation(
          env, instance_id_internal_,
          instance_id_internal_
              ->FutureAlloc<std::string>(InstanceIdInternal::kApiFunctionGetId)
              .get()));
  util::RunOnBackgroundThread(
      env,
      [](void* function_data) {
        // Assume that when this callback is called, AsyncOperation should still
        // be in instance_id_internal->operations_ or this callback should not
        // be called at all due to the lock in CppThreadDispatcherContext.
        AsyncOperation* op_ptr = static_cast<AsyncOperation*>(function_data);
        InstanceIdInternal* instance_id_internal =
            op_ptr->instance_id_internal();
        // Hold a reference to AsyncOperation so that it will not be deleted
        // during this callback.
        SharedPtr<AsyncOperation> operation =
            instance_id_internal->GetOperationSharedPtr(op_ptr);
        if (!operation) return;

        JNIEnv* env = instance_id_internal->instance_id()->app().GetJNIEnv();
        jobject java_instance_id =
            env->NewLocalRef(instance_id_internal->java_instance_id());
        jmethodID java_instance_id_method =
            instance_id::GetMethodId(instance_id::kGetId);
        operation->ReleaseExecuteCancelLock();
        jobject id_jstring =
            env->CallObjectMethod(java_instance_id, java_instance_id_method);
        std::string error = util::GetAndClearExceptionMessage(env);
        std::string id = util::JniStringToString(env, id_jstring);
        env->DeleteLocalRef(java_instance_id);
        if (operation->AcquireExecuteCancelLock()) {
          instance_id_internal->CompleteOperationWithResult(
              operation, id, ExceptionStringToError(error.c_str()),
              error.c_str());
        }
      },
      &(*operation), InstanceIdInternal::CanceledWithResult<std::string>,
      &(*operation));
  return GetIdLastResult();
}

Future<void> InstanceId::DeleteId() {
  if (!instance_id_internal_) return Future<void>();

  JNIEnv* env = app().GetJNIEnv();
  SharedPtr<AsyncOperation> operation = instance_id_internal_->AddOperation(
      new AsyncOperation(env, instance_id_internal_,
                         instance_id_internal_
                             ->FutureAlloc<std::string>(
                                 InstanceIdInternal::kApiFunctionDeleteId)
                             .get()));
  util::RunOnBackgroundThread(
      env,
      [](void* function_data) {
        // Assume that when this callback is called, AsyncOperation should still
        // be in instance_id_internal->operations_ or this callback should not
        // be called at all due to the lock in CppThreadDispatcherContext.
        AsyncOperation* op_ptr = static_cast<AsyncOperation*>(function_data);
        InstanceIdInternal* instance_id_internal =
            op_ptr->instance_id_internal();
        // Hold a reference to AsyncOperation so that it will not be deleted
        // during this callback.
        SharedPtr<AsyncOperation> operation =
            instance_id_internal->GetOperationSharedPtr(op_ptr);
        if (!operation) return;
        JNIEnv* env = instance_id_internal->instance_id()->app().GetJNIEnv();
        jobject java_instance_id =
            env->NewLocalRef(instance_id_internal->java_instance_id());
        jmethodID java_instance_id_method =
            instance_id::GetMethodId(instance_id::kDeleteInstanceId);
        operation->ReleaseExecuteCancelLock();
        env->CallVoidMethod(java_instance_id, java_instance_id_method);
        std::string error = util::GetAndClearExceptionMessage(env);
        env->DeleteLocalRef(java_instance_id);
        if (operation->AcquireExecuteCancelLock()) {
          instance_id_internal->CompleteOperation(
              operation, ExceptionStringToError(error.c_str()), error.c_str());
        }
      },
      &(*operation), InstanceIdInternal::Canceled, &(*operation));
  return DeleteIdLastResult();
}

// Context for a token retrieve / delete operation.
class AsyncTokenOperation : public AsyncOperation {
 public:
  AsyncTokenOperation(JNIEnv* env, InstanceIdInternal* instance_id_internal,
                      FutureHandle future_handle, const char* entity,
                      const char* scope)
      : AsyncOperation(env, instance_id_internal, future_handle),
        entity_(entity),
        scope_(scope) {
    derived_ = this;
  }

  virtual ~AsyncTokenOperation() {}

  const std::string& entity() const { return entity_; }
  const std::string& scope() const { return scope_; }

 private:
  std::string entity_;
  std::string scope_;
};

Future<std::string> InstanceId::GetToken(const char* entity,
                                         const char* scope) {
  if (!instance_id_internal_) return Future<std::string>();

  JNIEnv* env = app().GetJNIEnv();
  SharedPtr<AsyncOperation> operation = instance_id_internal_->AddOperation(
      new AsyncTokenOperation(env, instance_id_internal_,
                              instance_id_internal_
                                  ->FutureAlloc<std::string>(
                                      InstanceIdInternal::kApiFunctionGetToken)
                                  .get(),
                              entity, scope));
  util::RunOnBackgroundThread(
      env,
      [](void* function_data) {
        // Assume that when this callback is called, AsyncOperation should still
        // be in instance_id_internal->operations_ or this callback should not
        // be called at all due to the lock in CppThreadDispatcherContext.
        AsyncTokenOperation* op_ptr =
            static_cast<AsyncTokenOperation*>(function_data);
        InstanceIdInternal* instance_id_internal =
            op_ptr->instance_id_internal();
        // Hold a reference to AsyncOperation so that it will not be deleted
        // during this callback.
        SharedPtr<AsyncOperation> operation =
            instance_id_internal->GetOperationSharedPtr(op_ptr);
        if (!operation) return;

        JNIEnv* env = instance_id_internal->instance_id()->app().GetJNIEnv();
        jobject java_instance_id =
            env->NewLocalRef(instance_id_internal->java_instance_id());
        jmethodID java_instance_id_method =
            instance_id::GetMethodId(instance_id::kGetToken);
        jobject entity_jstring = env->NewStringUTF(op_ptr->entity().c_str());
        jobject scope_jstring = env->NewStringUTF(op_ptr->scope().c_str());
        operation->ReleaseExecuteCancelLock();
        jobject token_jstring =
            env->CallObjectMethod(java_instance_id, java_instance_id_method,
                                  entity_jstring, scope_jstring);
        std::string error = util::GetAndClearExceptionMessage(env);
        std::string token = util::JniStringToString(env, token_jstring);
        env->DeleteLocalRef(java_instance_id);
        env->DeleteLocalRef(entity_jstring);
        env->DeleteLocalRef(scope_jstring);
        if (operation->AcquireExecuteCancelLock()) {
          instance_id_internal->CompleteOperationWithResult(
              operation, token, ExceptionStringToError(error.c_str()),
              error.c_str());
        }
      },
      &(*operation), InstanceIdInternal::CanceledWithResult<std::string>,
      &(*operation));
  return GetTokenLastResult();
}

Future<void> InstanceId::DeleteToken(const char* entity, const char* scope) {
  if (!instance_id_internal_) return Future<void>();

  JNIEnv* env = app().GetJNIEnv();
  SharedPtr<AsyncOperation> operation =
      instance_id_internal_->AddOperation(new AsyncTokenOperation(
          env, instance_id_internal_,
          instance_id_internal_
              ->FutureAlloc<std::string>(
                  InstanceIdInternal::kApiFunctionDeleteToken)
              .get(),
          entity, scope));
  util::RunOnBackgroundThread(
      env,
      [](void* function_data) {
        // Assume that when this callback is called, AsyncOperation should still
        // be in instance_id_internal->operations_ or this callback should not
        // be called at all due to the lock in CppThreadDispatcherContext.
        AsyncTokenOperation* op_ptr =
            static_cast<AsyncTokenOperation*>(function_data);
        InstanceIdInternal* instance_id_internal =
            op_ptr->instance_id_internal();
        // Hold a reference to AsyncOperation so that it will not be deleted
        // during this callback.
        SharedPtr<AsyncOperation> operation =
            instance_id_internal->GetOperationSharedPtr(op_ptr);
        if (!operation) return;

        JNIEnv* env = instance_id_internal->instance_id()->app().GetJNIEnv();
        jobject entity_jstring = env->NewStringUTF(op_ptr->entity().c_str());
        jobject scope_jstring = env->NewStringUTF(op_ptr->scope().c_str());
        jobject java_instance_id =
            env->NewLocalRef(instance_id_internal->java_instance_id());
        jmethodID java_instance_id_method =
            instance_id::GetMethodId(instance_id::kDeleteToken);
        operation->ReleaseExecuteCancelLock();
        env->CallVoidMethod(java_instance_id, java_instance_id_method,
                            entity_jstring, scope_jstring);
        std::string error = util::GetAndClearExceptionMessage(env);
        env->DeleteLocalRef(java_instance_id);
        env->DeleteLocalRef(entity_jstring);
        env->DeleteLocalRef(scope_jstring);
        if (operation->AcquireExecuteCancelLock()) {
          instance_id_internal->CompleteOperation(
              operation, ExceptionStringToError(error.c_str()), error.c_str());
        }
      },
      &(*operation), InstanceIdInternal::Canceled, &(*operation));
  return DeleteTokenLastResult();
}

InstanceId* InstanceId::GetInstanceId(App* app, InitResult* init_result_out) {
  FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, app, "App must be specified.");
  FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(*app, init_result_out);
  MutexLock lock(InstanceIdInternal::mutex());
  if (init_result_out) *init_result_out = kInitResultSuccess;
  auto instance_id = InstanceIdInternal::FindInstanceIdByApp(app);
  if (instance_id) return instance_id;
  if (!Initialize(*app)) {
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }
  JNIEnv* env = app->GetJNIEnv();
  jobject platform_app = app->GetPlatformApp();
  jobject java_instance_id = env->CallStaticObjectMethod(
      instance_id::GetClass(),
      instance_id::GetMethodId(instance_id::kGetInstance),
      platform_app);
  env->DeleteLocalRef(platform_app);
  if (firebase::util::CheckAndClearJniExceptions(env) || !java_instance_id) {
    Terminate(*app);
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }
  InstanceIdInternal* instance_id_internal = new InstanceIdInternal();
  instance_id = new InstanceId(app, instance_id_internal);
  instance_id_internal->Initialize(instance_id, java_instance_id);
  return instance_id;
}

}  // namespace instance_id
}  // namespace firebase
