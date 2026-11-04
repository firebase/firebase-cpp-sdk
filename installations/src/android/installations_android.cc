#include "app/src/jni/declaration.h"
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

#include "app/src/app_android.h"
#include "app/src/jni/string.h"
#include "app/src/jni/exception.h"
#include "installations/src/android/installations_android.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "app/src/log.h"
#include "app/src/util.h"
#include "installations/src/common.h"

namespace firebase {
namespace installations {
namespace internal {

// Methods of FirebaseInstallations
// clang-format off
#define INSTALLATIONS_METHODS(X)                                               \
  X(GetId, "getId", "()Lcom/google/android/gms/tasks/Task;") \
  X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;") \
  X(GetToken, "getToken", "(Z)Lcom/google/android/gms/tasks/Task;") \
  X(AddIdListener, "addFidListener",                                           \
    "(Lcom/google/firebase/installations/internal/FidListener;)"               \
    "Lcom/google/firebase/installations/internal/FidListenerHandle;",          \
    firebase::jni::kMethodTypeInstance, firebase::jni::kMethodOptional) \
  X(GetInstance, "getInstance", "(Lcom/google/firebase/FirebaseApp;)"          \
    "Lcom/google/firebase/installations/FirebaseInstallations;",               \
    firebase::jni::kMethodTypeStatic)
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(installations, INSTALLATIONS_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/installations/FirebaseInstallations")


// Methods of InstallationTokenResult
// clang-format off
#define TOKEN_RESULT_METHODS(X)                                                \
  X(GetToken, "getToken", "()Ljava/lang/String;") \
  X(GetTokenExpirationTimestamp, "getTokenExpirationTimestamp", "()J",         \
    firebase::jni::kMethodTypeInstance, firebase::jni::kMethodOptional) \
  X(GetTokenCreationTimestamp, "getTokenCreationTimestamp", "()J",             \
    firebase::jni::kMethodTypeInstance, firebase::jni::kMethodOptional)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(token_result, TOKEN_RESULT_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/installations/InstallationTokenResult")


using firebase::internal::ReferenceCount;
using firebase::internal::ReferenceCountLock;

ReferenceCount internal::InstallationsInternal::initializer_;  // NOLINT

struct FISCallbackData {
  virtual ~FISCallbackData() = default;
};

struct PendingCallbacks {
  std::mutex mutex;
  std::unordered_set<FISCallbackData*> active;

  void Register(FISCallbackData* data) {
    std::lock_guard<std::mutex> lock(mutex);
    active.insert(data);
  }

  bool Claim(FISCallbackData* data) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = active.find(data);
    if (it == active.end()) {
      return false;
    }
    active.erase(it);
    return true;
  }

  void CancelAll() {
    std::unordered_set<FISCallbackData*> to_delete;
    {
      std::lock_guard<std::mutex> lock(mutex);
      to_delete.swap(active);
    }
    for (FISCallbackData* data : to_delete) {
      delete data;
    }
  }
};

template <typename T>
struct FISDataHandle : public FISCallbackData {
  FISDataHandle(std::weak_ptr<ReferenceCountedFutureImpl> _future_api,
                const SafeFutureHandle<T>& _future_handle,
                std::shared_ptr<PendingCallbacks> _tracker)
      : future_api(std::move(_future_api)),
        future_handle(_future_handle),
        tracker(std::move(_tracker)) {}
  std::weak_ptr<ReferenceCountedFutureImpl> future_api;
  SafeFutureHandle<T> future_handle;
  std::shared_ptr<PendingCallbacks> tracker;
};

static bool CacheJNIMethodIds(JNIEnv* env, jobject activity) {
  if (!installations::CacheMethodIds(env, activity)) {
    return false;
  }
  if (!token_result::CacheMethodIds(env, activity)) {
    installations::ReleaseClass(env);
    return false;
  }
  return true;
}

static void ReleaseClasses(JNIEnv* env) {
  installations::ReleaseClass(env);
  token_result::ReleaseClass(env);
}

static Error MapAndroidError(firebase::util::FutureResult result_code,
                             const char* status_message) {
  if (result_code == util::kFutureResultSuccess) {
    return kInstallationsErrorNone;
  }
  if (status_message != nullptr) {
    if (std::strstr(status_message, "BAD_CONFIG") != nullptr) {
      return kErrorInvalidConfiguration;
    }
    if (std::strstr(status_message, "UNAVAILABLE") != nullptr ||
        std::strstr(status_message, "TOO_MANY_REQUESTS") != nullptr) {
      return kErrorNoAccess;
    }
  }
  return kInstallationsErrorFailure;
}

static void CompleteVoidCallback(JNIEnv* env, jobject result,
                                 firebase::util::FutureResult result_code,
                                 const char* status_message,
                                 void* callback_data) {
  if (result != nullptr) {
    env->DeleteLocalRef(result);
  }
  auto* data_handle = reinterpret_cast<FISDataHandle<void>*>(callback_data);
  if (data_handle == nullptr) {
    return;
  }
  std::shared_ptr<PendingCallbacks> tracker = data_handle->tracker;
  if (tracker && !tracker->Claim(data_handle)) {
    return;
  }
  if (auto api = data_handle->future_api.lock()) {
    api->Complete(data_handle->future_handle,
                  MapAndroidError(result_code, status_message),
                  status_message != nullptr ? status_message : "");
  }
  delete data_handle;
}

static void StringResultCallback(JNIEnv* env, jobject result,
                                 firebase::util::FutureResult result_code,
                                 const char* status_message,
                                 void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value;
  if (success && result != nullptr) {
    result_value = jni::JStringToString(env, result);
  }
  if (result != nullptr) {
    env->DeleteLocalRef(result);
  }
  auto* data_handle =
      reinterpret_cast<FISDataHandle<std::string>*>(callback_data);
  if (data_handle == nullptr) {
    return;
  }
  std::shared_ptr<PendingCallbacks> tracker = data_handle->tracker;
  if (tracker && !tracker->Claim(data_handle)) {
    return;
  }
  if (auto api = data_handle->future_api.lock()) {
    api->CompleteWithResult(
        data_handle->future_handle,
        MapAndroidError(result_code, status_message),
        status_message != nullptr ? status_message : "", result_value);
  }
  delete data_handle;
}

static InstallationTokenResult JTokenResultToStruct(JNIEnv* env,
                                                    jobject jtoken,
                                                    bool* out_valid) {
  InstallationTokenResult token_result;
  *out_valid = false;
  if (jtoken == nullptr) {
    return token_result;
  }
  FIREBASE_DEV_ASSERT(env->IsInstanceOf(jtoken, token_result::GetClass()));

  jobject jstring_obj = env->CallObjectMethod(
      jtoken, token_result::GetMethodId(token_result::kGetToken));
  if (jni::CheckAndClearException(env)) {
    if (jstring_obj != nullptr) {
      env->DeleteLocalRef(jstring_obj);
    }
    return token_result;
  }
  if (jstring_obj != nullptr) {
    token_result.token = jni::JStringToString(env, jstring_obj);
    env->DeleteLocalRef(jstring_obj);
    *out_valid = true;
  }

  jmethodID get_expiration_id =
      token_result::GetMethodId(token_result::kGetTokenExpirationTimestamp);
  if (get_expiration_id != nullptr) {
    jlong expiration = env->CallLongMethod(jtoken, get_expiration_id);
    if (!jni::CheckAndClearException(env)) {
      token_result.expiration_timestamp = static_cast<int64_t>(expiration);
    }
  }
  return token_result;
}

static void TokenResultCallback(JNIEnv* env, jobject result,
                                firebase::util::FutureResult result_code,
                                const char* status_message,
                                void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  InstallationTokenResult parsed;
  bool valid_token = false;
  if (success && result != nullptr) {
    parsed = JTokenResultToStruct(env, result, &valid_token);
  }
  if (result != nullptr) {
    env->DeleteLocalRef(result);
  }
  auto* data_handle =
      reinterpret_cast<FISDataHandle<std::string>*>(callback_data);
  if (data_handle == nullptr) {
    return;
  }
  std::shared_ptr<PendingCallbacks> tracker = data_handle->tracker;
  if (tracker && !tracker->Claim(data_handle)) {
    return;
  }
  if (auto api = data_handle->future_api.lock()) {
    Error error = (success && valid_token)
                      ? kInstallationsErrorNone
                      : MapAndroidError(result_code, status_message);
    api->CompleteWithResult(
        data_handle->future_handle, error,
        status_message != nullptr ? status_message : "", parsed.token);
  }
  delete data_handle;
}

static void TokenResultStructCallback(JNIEnv* env, jobject result,
                                      firebase::util::FutureResult result_code,
                                      const char* status_message,
                                      void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  InstallationTokenResult parsed;
  bool valid_token = false;
  if (success && result != nullptr) {
    parsed = JTokenResultToStruct(env, result, &valid_token);
  }
  if (result != nullptr) {
    env->DeleteLocalRef(result);
  }
  auto* data_handle =
      reinterpret_cast<FISDataHandle<InstallationTokenResult>*>(callback_data);
  if (data_handle == nullptr) {
    return;
  }
  std::shared_ptr<PendingCallbacks> tracker = data_handle->tracker;
  if (tracker && !tracker->Claim(data_handle)) {
    return;
  }
  if (auto api = data_handle->future_api.lock()) {
    Error error = (success && valid_token)
                      ? kInstallationsErrorNone
                      : MapAndroidError(result_code, status_message);
    api->CompleteWithResult(
        data_handle->future_handle, error,
        status_message != nullptr ? status_message : "", parsed);
  }
  delete data_handle;
}

InstallationsInternal::InstallationsInternal(const firebase::App& app)
    : app_(app),
      pending_callbacks_(std::make_shared<PendingCallbacks>()) {
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  static const char* kApiIdentifier = "Installations";
  LogDebug("%s API Initializing", kApiIdentifier);
  JNIEnv* env = app_.GetJNIEnv();

  if (lock.AddReference() == 0) {
    jobject activity = app_.activity();
    if (!true) {
      lock.RemoveReference();
      return;
    }

    if (!CacheJNIMethodIds(env, activity)) {
      
      lock.RemoveReference();
      return;
    }
  }
  has_initializer_ref_ = true;

  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);

  jobject platform_app = app_.GetPlatformApp();
  jclass installations_class = installations::GetClass();
  jobject installations_instance_local = env->CallStaticObjectMethod(
      installations_class,
      installations::GetMethodId(installations::kGetInstance), platform_app);
  bool had_exception = jni::CheckAndClearException(env);
  if (platform_app != nullptr) {
    env->DeleteLocalRef(platform_app);
  }
  if (had_exception || installations_instance_local == nullptr) {
    if (installations_instance_local != nullptr) {
      env->DeleteLocalRef(installations_instance_local);
    }
    LogError("Failed to obtain FirebaseInstallations instance.");
    return;
  }
  internal_obj_ = env->NewGlobalRef(installations_instance_local);
  env->DeleteLocalRef(installations_instance_local);
  LogDebug("%s API Initialized", kApiIdentifier);
}

InstallationsInternal::~InstallationsInternal() { Cleanup(); }

bool InstallationsInternal::Initialized() const {
  return internal_obj_ != nullptr;
}

void InstallationsInternal::Cleanup() {
  JNIEnv* env = app_.GetJNIEnv();
  if (!jni_task_id_.empty()) {
    if (env != nullptr) {
      firebase::util::CancelCallbacks(env, jni_task_id_.c_str());
    }
    jni_task_id_.clear();
  }
  if (pending_callbacks_) {
    pending_callbacks_->CancelAll();
  }
  if (internal_obj_ != nullptr) {
    if (env != nullptr) {
      env->DeleteGlobalRef(internal_obj_);
    }
    internal_obj_ = nullptr;
  }
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  if (has_initializer_ref_) {
    has_initializer_ref_ = false;
    if (lock.RemoveReference() == 1 && env != nullptr) {
      ReleaseClasses(env);
      
    }
  }
}

void InstallationsInternal::LogHeartbeat(const firebase::App& app) {
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  if (initializer_.references() == 0) {
    return;
  }
  jclass installations_class = installations::GetClass();
  if (installations_class == nullptr) {
    return;
  }
  JNIEnv* env = app.GetJNIEnv();
  jobject platform_app = app.GetPlatformApp();
  jobject installations_instance_local = env->CallStaticObjectMethod(
      installations_class,
      installations::GetMethodId(installations::kGetInstance), platform_app);
  ::firebase::jni::CheckAndClearException(env);
  if (installations_instance_local != nullptr) {
    env->DeleteLocalRef(installations_instance_local);
  }
  if (platform_app != nullptr) {
    env->DeleteLocalRef(platform_app);
  }
}

Future<std::string> InstallationsInternal::GetId() {
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetId>();
  if (internal_obj_ == nullptr) {
    futures_.Complete<kInstallationsFnGetId>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnGetId>(handle);
  }

  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kGetId));
  if (jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) {
      env->DeleteLocalRef(task);
    }
    futures_.Complete<kInstallationsFnGetId>(handle, kInstallationsErrorFailure,
                                             "Failed to invoke getId.");
    return futures_.MakeFuture<kInstallationsFnGetId>(handle);
  }

  auto* data_handle = new FISDataHandle<std::string>(
      futures_.weak(), handle, pending_callbacks_);
  pending_callbacks_->Register(data_handle);

  firebase::util::RegisterCallbackOnTask(env, task, StringResultCallback,
                               reinterpret_cast<void*>(data_handle),
                               jni_task_id_.c_str());

  env->DeleteLocalRef(task);

  return futures_.MakeFuture<kInstallationsFnGetId>(handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() const {
  return futures_.LastResult<kInstallationsFnGetId>();
}

Future<std::string> InstallationsInternal::GetToken(bool force_refresh) {
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetToken>();
  if (internal_obj_ == nullptr) {
    futures_.Complete<kInstallationsFnGetToken>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnGetToken>(handle);
  }

  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kGetToken),
      static_cast<jboolean>(force_refresh));
  if (jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) {
      env->DeleteLocalRef(task);
    }
    futures_.Complete<kInstallationsFnGetToken>(
        handle, kInstallationsErrorFailure, "Failed to invoke getToken.");
    return futures_.MakeFuture<kInstallationsFnGetToken>(handle);
  }

  auto* data_handle = new FISDataHandle<std::string>(
      futures_.weak(), handle, pending_callbacks_);
  pending_callbacks_->Register(data_handle);

  firebase::util::RegisterCallbackOnTask(env, task, TokenResultCallback,
                               reinterpret_cast<void*>(data_handle),
                               jni_task_id_.c_str());

  env->DeleteLocalRef(task);

  return futures_.MakeFuture<kInstallationsFnGetToken>(handle);
}

Future<std::string> InstallationsInternal::GetTokenLastResult() const {
  return futures_.LastResult<kInstallationsFnGetToken>();
}

Future<InstallationTokenResult> InstallationsInternal::GetTokenResult(
    bool force_refresh) {
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetTokenResult>();
  if (internal_obj_ == nullptr) {
    futures_.Complete<kInstallationsFnGetTokenResult>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnGetTokenResult>(handle);
  }

  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kGetToken),
      static_cast<jboolean>(force_refresh));
  if (jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) {
      env->DeleteLocalRef(task);
    }
    futures_.Complete<kInstallationsFnGetTokenResult>(
        handle, kInstallationsErrorFailure, "Failed to invoke getToken.");
    return futures_.MakeFuture<kInstallationsFnGetTokenResult>(handle);
  }

  auto* data_handle = new FISDataHandle<InstallationTokenResult>(
      futures_.weak(), handle, pending_callbacks_);
  pending_callbacks_->Register(data_handle);

  firebase::util::RegisterCallbackOnTask(env, task, TokenResultStructCallback,
                               reinterpret_cast<void*>(data_handle),
                               jni_task_id_.c_str());

  env->DeleteLocalRef(task);

  return futures_.MakeFuture<kInstallationsFnGetTokenResult>(handle);
}

Future<InstallationTokenResult>
InstallationsInternal::GetTokenResultLastResult() const {
  return futures_.LastResult<kInstallationsFnGetTokenResult>();
}

Future<void> InstallationsInternal::Delete() {
  const auto handle = futures_.SafeAlloc<kInstallationsFnDelete>();
  if (internal_obj_ == nullptr) {
    futures_.Complete<kInstallationsFnDelete>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnDelete>(handle);
  }

  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, installations::GetMethodId(installations::kDelete));
  if (jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) {
      env->DeleteLocalRef(task);
    }
    futures_.Complete<kInstallationsFnDelete>(
        handle, kInstallationsErrorFailure, "Failed to invoke delete.");
    return futures_.MakeFuture<kInstallationsFnDelete>(handle);
  }

  auto* data_handle = new FISDataHandle<void>(
      futures_.weak(), handle, pending_callbacks_);
  pending_callbacks_->Register(data_handle);

  firebase::util::RegisterCallbackOnTask(env, task, CompleteVoidCallback,
                               reinterpret_cast<void*>(data_handle),
                               jni_task_id_.c_str());

  env->DeleteLocalRef(task);

  return futures_.MakeFuture<kInstallationsFnDelete>(handle);
}

Future<void> InstallationsInternal::DeleteLastResult() const {
  return futures_.LastResult<kInstallationsFnDelete>();
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
