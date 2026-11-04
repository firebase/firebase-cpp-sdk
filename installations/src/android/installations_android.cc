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

#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "app/src/app_android.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "installations/src/common.h"

namespace firebase {
namespace installations {
namespace internal {

// clang-format off
#define INSTALLATIONS_METHODS(X)                                               \
  X(GetId, "getId", "()Lcom/google/android/gms/tasks/Task;")                   \
  X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;")                 \
  X(GetToken, "getToken", "(Z)Lcom/google/android/gms/tasks/Task;")            \
  X(AddIdListener, "addFidListener",                                           \
    "(Lcom/google/firebase/installations/internal/FidListener;)"               \
    "Lcom/google/firebase/installations/internal/FidListenerHandle;",          \
    firebase::jni::kMethodTypeInstance, firebase::jni::kMethodOptional)        \
  X(GetInstance, "getInstance", "(Lcom/google/firebase/FirebaseApp;)"          \
    "Lcom/google/firebase/installations/FirebaseInstallations;",               \
    firebase::jni::kMethodTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(
    installations, INSTALLATIONS_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/installations/FirebaseInstallations")

// clang-format off
#define TOKEN_RESULT_METHODS(X)                                                \
  X(GetToken, "getToken", "()Ljava/lang/String;")                              \
  X(GetTokenExpirationTimestamp, "getTokenExpirationTimestamp", "()J",         \
    firebase::jni::kMethodTypeInstance, firebase::jni::kMethodOptional)        \
  X(GetTokenCreationTimestamp, "getTokenCreationTimestamp", "()J",             \
    firebase::jni::kMethodTypeInstance, firebase::jni::kMethodOptional)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(
    token_result, TOKEN_RESULT_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/installations/InstallationTokenResult")

namespace {
std::mutex g_init_mutex;
int g_init_count = 0;

struct TaskCallback {
  std::weak_ptr<PendingCallbacks> tracker;
  std::weak_ptr<ReferenceCountedFutureImpl> future_api;
  std::function<void(ReferenceCountedFutureImpl*, JNIEnv*, jobject, Error,
                     const char*)>
      complete;
};
}  // namespace

struct PendingCallbacks {
  static std::mutex& Mutex() {
    static auto* m = new std::mutex();
    return *m;
  }
  static std::unordered_set<TaskCallback*>& Global() {
    static auto* s = new std::unordered_set<TaskCallback*>();
    return *s;
  }
  std::unordered_set<TaskCallback*> active;
  ~PendingCallbacks() { CancelAll(); }

  void Register(TaskCallback* cb) {
    std::lock_guard<std::mutex> lock(Mutex());
    active.insert(cb);
    Global().insert(cb);
  }
  static bool Claim(TaskCallback* cb) {
    std::lock_guard<std::mutex> lock(Mutex());
    if (!cb || !Global().erase(cb)) return false;
    if (auto t = cb->tracker.lock()) t->active.erase(cb);
    return true;
  }
  void CancelAll() {
    std::unordered_set<TaskCallback*> to_del;
    {
      std::lock_guard<std::mutex> lock(Mutex());
      for (auto* cb : active) Global().erase(cb);
      to_del.swap(active);
    }
    for (auto* cb : to_del) delete cb;
  }
};

namespace {

Error MapAndroidError(util::FutureResult code, const char* msg) {
  if (code == util::kFutureResultSuccess) return kInstallationsErrorNone;
  if (msg) {
    if (std::strstr(msg, "BAD_CONFIG")) return kErrorInvalidConfiguration;
    if (std::strstr(msg, "UNAVAILABLE") ||
        std::strstr(msg, "TOO_MANY_REQUESTS"))
      return kErrorNoAccess;
  }
  return kInstallationsErrorFailure;
}

InstallationTokenResult JTokenResultToStruct(JNIEnv* env, jobject jtoken,
                                             bool* out_valid) {
  InstallationTokenResult res;
  *out_valid = false;
  jclass cls = token_result::GetClass();
  if (!jtoken || !cls) return res;
  jobject jstr = env->CallObjectMethod(
      jtoken, token_result::GetMethodId(token_result::kGetToken));
  if (!jni::CheckAndClearException(env) && jstr) {
    res.token = jni::JStringToString(env, jstr);
    *out_valid = true;
  }
  if (jstr) env->DeleteLocalRef(jstr);
  if (jmethodID exp_id = token_result::GetMethodId(
          token_result::kGetTokenExpirationTimestamp)) {
    jlong exp = env->CallLongMethod(jtoken, exp_id);
    if (!jni::CheckAndClearException(env)) {
      res.expiration_timestamp = static_cast<int64_t>(exp);
    }
  }
  return res;
}

void OnTaskComplete(JNIEnv* env, jobject result, util::FutureResult code,
                    const char* status_message, void* callback_data) {
  auto* cb = static_cast<TaskCallback*>(callback_data);
  if (PendingCallbacks::Claim(cb)) {
    if (auto api = cb->future_api.lock()) {
      cb->complete(api.get(), env, result,
                   MapAndroidError(code, status_message),
                   status_message ? status_message : "");
    }
    delete cb;
  }
  if (result) env->DeleteLocalRef(result);
}

template <InstallationsFn Slot, typename CompleteFn>
Future<SlotResultT<Slot>> DispatchTask(
    JNIEnv* env, jobject internal_obj, const std::string& task_id,
    InstallationsFutures& futures,
    const std::shared_ptr<PendingCallbacks>& pending, const char* fail_msg,
    jobject (*invoke)(JNIEnv*, jobject, void*), void* arg,
    CompleteFn complete_fn) {
  const auto handle = futures.SafeAlloc<Slot>();
  if (!internal_obj) {
    futures.Complete<Slot>(handle, kErrorInvalidConfiguration,
                           "Installations is not initialized.");
    return futures.MakeFuture<Slot>(handle);
  }
  jobject task = invoke(env, internal_obj, arg);
  if (jni::CheckAndClearException(env) || !task) {
    if (task) env->DeleteLocalRef(task);
    futures.Complete<Slot>(handle, kInstallationsErrorFailure, fail_msg);
    return futures.MakeFuture<Slot>(handle);
  }
  auto* cb = new TaskCallback{pending, futures.weak(),
                              [handle, complete_fn](
                                  ReferenceCountedFutureImpl* api, JNIEnv* e,
                                  jobject res, Error err, const char* msg) {
                                complete_fn(api, handle, e, res, err, msg);
                              }};
  pending->Register(cb);
  util::RegisterCallbackOnTask(env, task, OnTaskComplete, cb, task_id.c_str());
  env->DeleteLocalRef(task);
  return futures.MakeFuture<Slot>(handle);
}

}  // namespace

InstallationsInternal::InstallationsInternal(const firebase::App& app)
    : app_(app), pending_callbacks_(std::make_shared<PendingCallbacks>()) {
  static const char* kApiIdentifier = "Installations";
  JNIEnv* env = app_.GetJNIEnv();
  {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    if (g_init_count == 0) {
      jobject activity = app_.activity();
      if (!util::Initialize(env, activity)) return;
      if (!installations::CacheMethodIds(env, activity) ||
          !token_result::CacheMethodIds(env, activity)) {
        installations::ReleaseClass(env);
        token_result::ReleaseClass(env);
        util::Terminate(env);
        return;
      }
    }
    ++g_init_count;
    has_initializer_ref_ = true;
  }
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);
  jobject platform_app = app_.GetPlatformApp();
  jobject local_inst = env->CallStaticObjectMethod(
      installations::GetClass(),
      installations::GetMethodId(installations::kGetInstance), platform_app);
  bool had_exc = jni::CheckAndClearException(env);
  if (platform_app) env->DeleteLocalRef(platform_app);
  if (!had_exc && local_inst) {
    internal_obj_ = env->NewGlobalRef(local_inst);
  } else {
    LogError("Failed to obtain FirebaseInstallations instance.");
  }
  if (local_inst) env->DeleteLocalRef(local_inst);
}

InstallationsInternal::~InstallationsInternal() { Cleanup(); }

void InstallationsInternal::Cleanup() {
  JNIEnv* env = app_.GetJNIEnv();
  if (!jni_task_id_.empty()) {
    if (env) util::CancelCallbacks(env, jni_task_id_.c_str());
    jni_task_id_.clear();
  }
  if (pending_callbacks_) pending_callbacks_->CancelAll();
  if (internal_obj_) {
    if (env) env->DeleteGlobalRef(internal_obj_);
    internal_obj_ = nullptr;
  }
  std::lock_guard<std::mutex> lock(g_init_mutex);
  if (has_initializer_ref_) {
    has_initializer_ref_ = false;
    if (--g_init_count == 0 && env) {
      installations::ReleaseClass(env);
      token_result::ReleaseClass(env);
      util::Terminate(env);
    }
  }
}

void InstallationsInternal::LogHeartbeat(const firebase::App& app) {
  std::lock_guard<std::mutex> lock(g_init_mutex);
  if (g_init_count == 0 || !installations::GetClass()) return;
  JNIEnv* env = app.GetJNIEnv();
  jobject platform_app = app.GetPlatformApp();
  jobject local_inst = env->CallStaticObjectMethod(
      installations::GetClass(),
      installations::GetMethodId(installations::kGetInstance), platform_app);
  jni::CheckAndClearException(env);
  if (local_inst) env->DeleteLocalRef(local_inst);
  if (platform_app) env->DeleteLocalRef(platform_app);
}

Future<std::string> InstallationsInternal::GetId() {
  return DispatchTask<kInstallationsFnGetId>(
      app_.GetJNIEnv(), internal_obj_, jni_task_id_, futures_,
      pending_callbacks_, "Failed to invoke getId.",
      [](JNIEnv* e, jobject obj, void*) {
        return e->CallObjectMethod(
            obj, installations::GetMethodId(installations::kGetId));
      },
      nullptr,
      [](ReferenceCountedFutureImpl* api, SafeFutureHandle<std::string> h,
         JNIEnv* e, jobject res, Error err, const char* msg) {
        std::string val = (err == kInstallationsErrorNone && res)
                              ? jni::JStringToString(e, res)
                              : std::string();
        api->CompleteWithResult(h, err, msg, val);
      });
}

Future<std::string> InstallationsInternal::GetToken(bool force_refresh) {
  return DispatchTask<kInstallationsFnGetToken>(
      app_.GetJNIEnv(), internal_obj_, jni_task_id_, futures_,
      pending_callbacks_, "Failed to invoke getToken.",
      [](JNIEnv* e, jobject obj, void* arg) {
        return e->CallObjectMethod(
            obj, installations::GetMethodId(installations::kGetToken),
            static_cast<jboolean>(*static_cast<bool*>(arg)));
      },
      &force_refresh,
      [](ReferenceCountedFutureImpl* api, SafeFutureHandle<std::string> h,
         JNIEnv* e, jobject res, Error err, const char* msg) {
        bool valid = false;
        auto parsed = (err == kInstallationsErrorNone && res)
                          ? JTokenResultToStruct(e, res, &valid)
                          : InstallationTokenResult{};
        if (err == kInstallationsErrorNone && !valid)
          err = kInstallationsErrorFailure;
        api->CompleteWithResult(h, err, msg, parsed.token);
      });
}

Future<InstallationTokenResult> InstallationsInternal::GetTokenResult(
    bool force_refresh) {
  return DispatchTask<kInstallationsFnGetTokenResult>(
      app_.GetJNIEnv(), internal_obj_, jni_task_id_, futures_,
      pending_callbacks_, "Failed to invoke getToken.",
      [](JNIEnv* e, jobject obj, void* arg) {
        return e->CallObjectMethod(
            obj, installations::GetMethodId(installations::kGetToken),
            static_cast<jboolean>(*static_cast<bool*>(arg)));
      },
      &force_refresh,
      [](ReferenceCountedFutureImpl* api,
         SafeFutureHandle<InstallationTokenResult> h, JNIEnv* e, jobject res,
         Error err, const char* msg) {
        bool valid = false;
        auto parsed = (err == kInstallationsErrorNone && res)
                          ? JTokenResultToStruct(e, res, &valid)
                          : InstallationTokenResult{};
        if (err == kInstallationsErrorNone && !valid)
          err = kInstallationsErrorFailure;
        api->CompleteWithResult(h, err, msg, parsed);
      });
}

Future<void> InstallationsInternal::Delete() {
  return DispatchTask<kInstallationsFnDelete>(
      app_.GetJNIEnv(), internal_obj_, jni_task_id_, futures_,
      pending_callbacks_, "Failed to invoke delete.",
      [](JNIEnv* e, jobject obj, void*) {
        return e->CallObjectMethod(
            obj, installations::GetMethodId(installations::kDelete));
      },
      nullptr,
      [](ReferenceCountedFutureImpl* api, SafeFutureHandle<void> h, JNIEnv*,
         jobject, Error err, const char* msg) { api->Complete(h, err, msg); });
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
