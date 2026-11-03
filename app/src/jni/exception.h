// Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_SRC_JNI_EXCEPTION_H_
#define FIREBASE_APP_SRC_JNI_EXCEPTION_H_

#include <jni.h>

#include <string>

#include "app/src/jni/local.h"
#include "app/src/jni/ownership.h"
#include "app/src/jni/string.h"

namespace firebase {
namespace jni {

// Policy controlling how JNI exceptions are handled at scope exit or call boundaries.
enum class ExceptionPolicy {
  // Captures and clears pending JNI exceptions so C++ can handle them cleanly.
  kCaptureAndClear,
  // Preserves pending JNI exceptions on the JNIEnv* (for JNI callbacks that
  // intentionally throw a Java exception back to the JVM).
  kPreservePending,
};

// Wrapper for a non-owning jthrowable reference.
class Throwable : public Object {
 public:
  using JniType = jthrowable;

  constexpr Throwable() noexcept = default;
  explicit constexpr Throwable(jthrowable throwable) noexcept
      : Object(throwable) {}

  constexpr jthrowable get() const noexcept {
    return static_cast<jthrowable>(object_);
  }

  std::string GetMessage(Env& env) const;
};

// Saves and clears any currently pending JNI exception on construction, and
// restores the exact saved exception state on destruction (clearing any
// secondary exception that occurred within the guarded scope).
class ExceptionClearGuard {
 public:
  explicit ExceptionClearGuard(JNIEnv* env) noexcept : env_(env) {
    if (env_ && env_->ExceptionCheck()) {
      saved_exception_.reset(env_, env_->ExceptionOccurred());
      env_->ExceptionClear();
    }
  }

  explicit ExceptionClearGuard(Env& env) noexcept;

  ~ExceptionClearGuard() noexcept {
    if (!env_) return;
    if (env_->ExceptionCheck()) {
      env_->ExceptionClear();
    }
    if (saved_exception_) {
      env_->Throw(saved_exception_.get());
    }
  }

  ExceptionClearGuard(const ExceptionClearGuard&) = delete;
  ExceptionClearGuard& operator=(const ExceptionClearGuard&) = delete;
  ExceptionClearGuard(ExceptionClearGuard&&) = delete;
  ExceptionClearGuard& operator=(ExceptionClearGuard&&) = delete;

  bool had_exception() const noexcept { return static_cast<bool>(saved_exception_); }
  jthrowable saved_exception() const noexcept { return saved_exception_.get(); }

  // Consumes the saved exception so it will not be re-thrown on destruction.
  ScopedLocalRef<jthrowable> ReleaseSavedException() noexcept {
    return ScopedLocalRef<jthrowable>(env_, saved_exception_.release());
  }

 private:
  JNIEnv* env_ = nullptr;
  ScopedLocalRef<jthrowable> saved_exception_;
};

// Extracts a human-readable message from a jthrowable without disturbing any
// pending exception on `env`.
inline std::string ExtractExceptionMessage(JNIEnv* env, jthrowable exception) {
  if (!env || !exception) return std::string();
  ExceptionClearGuard guard(env);

  ScopedLocalRef<jclass> throwable_class(env, env->GetObjectClass(exception));
  if (!throwable_class || env->ExceptionCheck()) {
    env->ExceptionClear();
    return std::string();
  }

  auto try_method = [&](const char* method_name) -> std::string {
    jmethodID mid =
        env->GetMethodID(throwable_class.get(), method_name,
                         "()Ljava/lang/String;");
    if (!mid || env->ExceptionCheck()) {
      env->ExceptionClear();
      return std::string();
    }
    ScopedLocalRef<jobject> msg_obj(
        env, env->CallObjectMethod(exception, mid));
    if (env->ExceptionCheck() || !msg_obj) {
      env->ExceptionClear();
      return std::string();
    }
    return JStringToString(env, msg_obj.get());
  };

  std::string message = try_method("getLocalizedMessage");
  if (message.empty()) {
    message = try_method("getMessage");
  }
  if (message.empty()) {
    message = try_method("toString");
  }
  return message;
}

// Checks if a JNI exception is pending on `env`, and if so clears it and
// returns true.
inline bool CheckAndClearException(JNIEnv* env) noexcept {
  if (!env) return false;
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return true;
  }
  return false;
}

// Checks if a JNI exception is pending on `env`, and if so extracts its message,
// clears the exception, and returns the message (or a non-empty fallback).
inline std::string GetAndClearExceptionMessage(JNIEnv* env) {
  if (!env || !env->ExceptionCheck()) return std::string();
  ScopedLocalRef<jthrowable> exception(env, env->ExceptionOccurred());
  env->ExceptionClear();
  if (!exception) return "Unknown JNI exception";
  std::string msg = ExtractExceptionMessage(env, exception.get());
  return msg.empty() ? "Unknown JNI exception" : msg;
}

// RAII scope guard for JNI callbacks (`JNIEXPORT` functions invoked from Java).
// Flushes any deferred GlobalRef deletions on entry, and enforces the chosen
// ExceptionPolicy on scope exit.
class JniCallbackScope {
 public:
  explicit JniCallbackScope(
      JNIEnv* env,
      ExceptionPolicy policy = ExceptionPolicy::kCaptureAndClear) noexcept
      : env_(env), policy_(policy) {
    if (env_) {
      internal::GlobalRefReaper::RecordJavaVM(env_);
      internal::GlobalRefReaper::Flush(env_);
    }
  }

  ~JniCallbackScope() noexcept {
    if (!env_) return;
    if (policy_ == ExceptionPolicy::kCaptureAndClear &&
        env_->ExceptionCheck()) {
      env_->ExceptionClear();
    }
  }

  JniCallbackScope(const JniCallbackScope&) = delete;
  JniCallbackScope& operator=(const JniCallbackScope&) = delete;

  JNIEnv* env() const noexcept { return env_; }
  ExceptionPolicy policy() const noexcept { return policy_; }
  void set_policy(ExceptionPolicy policy) noexcept { policy_ = policy; }

 private:
  JNIEnv* env_ = nullptr;
  ExceptionPolicy policy_;
};

}  // namespace jni
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_JNI_EXCEPTION_H_
