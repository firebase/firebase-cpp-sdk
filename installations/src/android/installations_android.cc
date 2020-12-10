// Copyright 2020 Google LLC
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

#include "installations/src/android/installations_android.h"

#include "app/src/util.h"
#include "installations/src/common.h"

namespace firebase {
namespace installations {
namespace internal {

// Methods of FirebaseInstallations
// clang-format off
#define INSTALLATIONS_METHODS(X)                                               \
  X(GetId, "getId", "()Lcom/google/android/gms/tasks/Task;"),                  \
  X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;"),                \
  X(GetToken, "getToken", "(Z)Lcom/google/android/gms/tasks/Task;"),           \
  X(GetInstance, "getInstance", "(Lcom/google/firebase/FirebaseApp;)"          \
    "Lcom/google/firebase/installations/FirebaseInstallations;",               \
    firebase::util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(installations, INSTALLATIONS_METHODS)
METHOD_LOOKUP_DEFINITION(
    installations,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/installations/FirebaseInstallations",
    INSTALLATIONS_METHODS)

// Methods of InstallationTokenResult
// clang-format off
#define TOKEN_RESULT_METHODS(X) \
  X(GetToken, "getToken", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(token_result, TOKEN_RESULT_METHODS)
METHOD_LOOKUP_DEFINITION(
    token_result,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/installations/InstallationTokenResult",
    TOKEN_RESULT_METHODS)

using firebase::internal::ReferenceCount;
using firebase::internal::ReferenceCountLock;

ReferenceCount internal::InstallationsInternal::initializer_;  // NOLINT

static const char* kApiIdentifier = "Installations";

template <typename T>
struct FISDataHandle {
  FISDataHandle(ReferenceCountedFutureImpl* _future_api,
                const SafeFutureHandle<T>& _future_handle)
      : future_api(_future_api), future_handle(_future_handle) {}
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<T> future_handle;
};

static bool CacheJNIMethodIds(JNIEnv* env, jobject activity) {
  return installations::CacheMethodIds(env, activity) &&
         token_result::CacheMethodIds(env, activity);
}

static void ReleaseClasses(JNIEnv* env) {
  installations::ReleaseClass(env);
  token_result::ReleaseClass(env);
}

void CompleteVoidCallback(JNIEnv* env, jobject result,
                          util::FutureResult result_code,
                          const char* status_message, void* callback_data) {
  auto data_handle = reinterpret_cast<FISDataHandle<void>*>(callback_data);
  data_handle->future_api->Complete(data_handle->future_handle,
                                    result_code == util::kFutureResultSuccess
                                        ? kInstallationsErrorNone
                                        : kInstallationsErrorFailure);
  if (result) env->DeleteLocalRef(result);
}

void StringResultCallback(JNIEnv* env, jobject result,
                          util::FutureResult result_code,
                          const char* status_message, void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value = "";
  if (success && result) {
    result_value = util::JniStringToString(env, result);
  }
  auto data_handle =
      reinterpret_cast<FISDataHandle<std::string>*>(callback_data);
  data_handle->future_api->CompleteWithResult(
      data_handle->future_handle,
      success ? kInstallationsErrorNone : kInstallationsErrorFailure,
      status_message, result_value);
}

static std::string JTokenResultToString(JNIEnv* env, jobject jtoken) {
  FIREBASE_DEV_ASSERT(env->IsInstanceOf(jtoken, token_result::GetClass()));

  jobject jstring = env->CallObjectMethod(
      jtoken, token_result::GetMethodId(token_result::kGetToken));
  std::string result_value = util::JStringToString(env, jstring);
  env->DeleteLocalRef(jstring);
  env->DeleteLocalRef(jtoken);
  return result_value;
}

void TokenResultCallback(JNIEnv* env, jobject result,
                         util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value = "";
  if (success && result) {
    result_value = JTokenResultToString(env, result);
  }
  auto data_handle =
      reinterpret_cast<FISDataHandle<std::string>*>(callback_data);
  data_handle->future_api->CompleteWithResult(
      data_handle->future_handle,
      success ? kInstallationsErrorNone : kInstallationsErrorFailure,
      status_message, result_value);
}

InstallationsInternal::InstallationsInternal(const firebase::App& app)
    : app_(app), future_impl_(kInstallationsFnCount) {
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  LogDebug("%s API Initializing", kApiIdentifier);
  JNIEnv* env = app_.GetJNIEnv();

  if (lock.AddReference() == 0) {
    // Initialize
    jobject activity = app_.activity();
    if (!util::Initialize(env, activity)) {
      lock.RemoveReference();
      return;
    }

    // Cache method pointers.
    if (!CacheJNIMethodIds(env, activity)) {
      ReleaseClasses(env);
      util::Terminate(env);
      lock.RemoveReference();
      return;
    }
  }

  // Create the remote config class.
  jobject platform_app = app_.GetPlatformApp();

  jclass installations_class = installations::GetClass();
  jobject installations_instance_local = env->CallStaticObjectMethod(
      installations_class,
      installations::GetMethodId(installations::kGetInstance), platform_app);
  FIREBASE_ASSERT(installations_instance_local);
  internal_obj_ = env->NewGlobalRef(installations_instance_local);
  env->DeleteLocalRef(installations_instance_local);
  env->DeleteLocalRef(platform_app);
  LogDebug("%s API Initialized", kApiIdentifier);
}

InstallationsInternal::~InstallationsInternal() {}

bool InstallationsInternal::Initialized() const {
  return internal_obj_ != nullptr;
}

void InstallationsInternal::Cleanup() {
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  if (lock.RemoveReference() == 1) {
    JNIEnv* env = app_.GetJNIEnv();
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

Future<std::string> InstallationsInternal::GetId() {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetId);

  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kGetId));

  auto data_handle = new FISDataHandle<std::string>(&future_impl_, handle);

  util::RegisterCallbackOnTask(env, task, StringResultCallback,
                               reinterpret_cast<void*>(data_handle),
                               kApiIdentifier);

  env->DeleteLocalRef(task);

  return MakeFuture<std::string>(&future_impl_, handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetId));
}

Future<std::string> InstallationsInternal::GetToken(bool forceRefresh) {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetToken);

  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kGetToken),
      static_cast<jboolean>(forceRefresh));

  auto data_handle = new FISDataHandle<std::string>(&future_impl_, handle);

  util::RegisterCallbackOnTask(env, task, TokenResultCallback,
                               reinterpret_cast<void*>(data_handle),
                               kApiIdentifier);

  env->DeleteLocalRef(task);

  return MakeFuture<std::string>(&future_impl_, handle);
}

Future<std::string> InstallationsInternal::GetTokenLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetToken));
}

Future<void> InstallationsInternal::Delete() {
  const auto handle = future_impl_.SafeAlloc<void>(kInstallationsFnGetId);
  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kDelete));

  auto data_handle = new FISDataHandle<void>(&future_impl_, handle);

  util::RegisterCallbackOnTask(env, task, CompleteVoidCallback,
                               reinterpret_cast<void*>(data_handle),
                               kApiIdentifier);

  env->DeleteLocalRef(task);

  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> InstallationsInternal::DeleteLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kInstallationsFnDelete));
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
