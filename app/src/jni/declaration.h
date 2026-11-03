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

#ifndef FIREBASE_APP_SRC_JNI_DECLARATION_H_
#define FIREBASE_APP_SRC_JNI_DECLARATION_H_

#include <jni.h>

#include <atomic>
#include <cstddef>
#include <tuple>

#include "app/src/jni/ownership.h"

namespace firebase {
namespace jni {

// Untyped base class for JNI constructor declarations.
//
// Thread-safety & lifetime note: `clazz_` must be a JVM global `jclass`
// reference whose lifetime is owned by the caller / `Loader` (never a
// frame-scoped local reference). `Set()` and `Reset()` use acquire/release
// atomic operations so concurrent readers never observe a partially initialized
// declaration.
class ConstructorBase {
 protected:
  struct State {
    jclass clazz;
    jmethodID id;
  };
 public:
  constexpr explicit ConstructorBase(const char* sig) noexcept
      : sig_(sig ? sig : "") {}
  explicit constexpr ConstructorBase(jclass global_clazz,
                                     jmethodID id) noexcept
      : sig_(""), clazz_{global_clazz}, id_{id} {}

  constexpr const char* name() const noexcept { return "<init>"; }
  constexpr const char* sig() const noexcept { return sig_; }

  State Get() const noexcept {
    return {clazz_.load(std::memory_order_acquire),
            id_.load(std::memory_order_acquire)};
  }

  explicit operator bool() const noexcept {
    return clazz_.load(std::memory_order_acquire) != nullptr &&
           id_.load(std::memory_order_acquire) != nullptr;
  }

  // Stores a JVM global `jclass` reference and resolved `jmethodID`.
  void Set(JNIEnv* env, jclass clazz, jmethodID id) noexcept {
    JavaVM* jvm = internal::GlobalRefReaper::RecordJavaVM(env);
    jobject global_clazz = internal::GlobalRefReaper::NewGlobalRef(env, clazz, jvm);
    id_.store(id, std::memory_order_release);
    jclass old_clazz = clazz_.exchange(static_cast<jclass>(global_clazz), std::memory_order_acq_rel);
    if (old_clazz) {
      internal::GlobalRefReaper::DeleteOrEnqueue(old_clazz, env, jvm);
    }
  }

  void Reset(JNIEnv* env = nullptr) noexcept {
    jclass old_clazz = clazz_.exchange(nullptr, std::memory_order_acq_rel);
    id_.store(nullptr, std::memory_order_release);
    if (old_clazz) {
      internal::GlobalRefReaper::DeleteOrEnqueue(old_clazz, env);
    }
  }

 private:
  const char* sig_ = "";
  std::atomic<jclass> clazz_{nullptr};
  std::atomic<jmethodID> id_{nullptr};
};

// Typed declaration of a Java constructor for type T (or signature T(Args...)).
template <typename T>
class Constructor : public ConstructorBase {
 public:
  using ResultType = T;
  static constexpr bool kHasTypedArgs = false;
  using ConstructorBase::ConstructorBase;
};

template <typename T, typename... Args>
class Constructor<T(Args...)> : public ConstructorBase {
 public:
  using ResultType = T;
  using ArgsTuple = std::tuple<Args...>;
  static constexpr bool kHasTypedArgs = true;
  static constexpr size_t kArity = sizeof...(Args);
  using ConstructorBase::ConstructorBase;
};

// Untyped base class for JNI instance method declarations.
class MethodBase {
 public:
  constexpr MethodBase(const char* name, const char* sig) noexcept
      : name_(name ? name : ""), sig_(sig ? sig : "") {}
  explicit constexpr MethodBase(jmethodID id) noexcept
      : name_(""), sig_(""), id_(id) {}

  constexpr const char* name() const noexcept { return name_; }
  constexpr const char* sig() const noexcept { return sig_; }
  jmethodID id() const noexcept {
    return id_.load(std::memory_order_acquire);
  }

  explicit operator bool() const noexcept {
    return id_.load(std::memory_order_acquire) != nullptr;
  }

  void Set(jmethodID id) noexcept {
    id_.store(id, std::memory_order_release);
  }

  void Reset() noexcept {
    id_.store(nullptr, std::memory_order_release);
  }

 private:
  const char* name_ = "";
  const char* sig_ = "";
  std::atomic<jmethodID> id_{nullptr};
};

// Typed declaration of a Java instance method returning T (or signature
// Ret(Args...)).
template <typename T>
class Method : public MethodBase {
 public:
  using ResultType = T;
  static constexpr bool kHasTypedArgs = false;
  using MethodBase::MethodBase;
};

template <typename Ret, typename... Args>
class Method<Ret(Args...)> : public MethodBase {
 public:
  using ResultType = Ret;
  using ArgsTuple = std::tuple<Args...>;
  static constexpr bool kHasTypedArgs = true;
  static constexpr size_t kArity = sizeof...(Args);
  using MethodBase::MethodBase;
};

// Untyped base class for JNI instance field declarations.
class FieldBase {
 public:
  constexpr FieldBase(const char* name, const char* sig) noexcept
      : name_(name ? name : ""), sig_(sig ? sig : "") {}
  explicit constexpr FieldBase(jfieldID id) noexcept
      : name_(""), sig_(""), id_(id) {}

  constexpr const char* name() const noexcept { return name_; }
  constexpr const char* sig() const noexcept { return sig_; }
  jfieldID id() const noexcept {
    return id_.load(std::memory_order_acquire);
  }

  explicit operator bool() const noexcept {
    return id_.load(std::memory_order_acquire) != nullptr;
  }

  void Set(jfieldID id) noexcept {
    id_.store(id, std::memory_order_release);
  }

  void Reset() noexcept {
    id_.store(nullptr, std::memory_order_release);
  }

 private:
  const char* name_ = "";
  const char* sig_ = "";
  std::atomic<jfieldID> id_{nullptr};
};

// Typed declaration of a Java instance field of type T.
template <typename T>
class Field : public FieldBase {
 public:
  using ResultType = T;
  using FieldBase::FieldBase;
};

// Untyped base class for JNI static field declarations.
//
// `clazz_` must be a JVM global `jclass` reference owned by the caller /
// `Loader`.
class StaticFieldBase {
 protected:
  struct State {
    jclass clazz;
    jfieldID id;
  };
 public:
  constexpr StaticFieldBase(const char* name, const char* sig) noexcept
      : name_(name ? name : ""), sig_(sig ? sig : "") {}
  explicit constexpr StaticFieldBase(jclass global_clazz,
                                     jfieldID id) noexcept
      : name_(""), sig_(""), clazz_{global_clazz}, id_{id} {}

  constexpr const char* name() const noexcept { return name_; }
  constexpr const char* sig() const noexcept { return sig_; }

  State Get() const noexcept {
    return {clazz_.load(std::memory_order_acquire),
            id_.load(std::memory_order_acquire)};
  }

  explicit operator bool() const noexcept {
    return clazz_.load(std::memory_order_acquire) != nullptr &&
           id_.load(std::memory_order_acquire) != nullptr;
  }

  void Set(JNIEnv* env, jclass clazz, jfieldID id) noexcept {
    JavaVM* jvm = internal::GlobalRefReaper::RecordJavaVM(env);
    jobject global_clazz = internal::GlobalRefReaper::NewGlobalRef(env, clazz, jvm);
    id_.store(id, std::memory_order_release);
    jclass old_clazz = clazz_.exchange(static_cast<jclass>(global_clazz), std::memory_order_acq_rel);
    if (old_clazz) {
      internal::GlobalRefReaper::DeleteOrEnqueue(old_clazz, env, jvm);
    }
  }

  void Reset(JNIEnv* env = nullptr) noexcept {
    jclass old_clazz = clazz_.exchange(nullptr, std::memory_order_acq_rel);
    id_.store(nullptr, std::memory_order_release);
    if (old_clazz) {
      internal::GlobalRefReaper::DeleteOrEnqueue(old_clazz, env);
    }
  }

 private:
  const char* name_ = "";
  const char* sig_ = "";
  std::atomic<jclass> clazz_{nullptr};
  std::atomic<jfieldID> id_{nullptr};
};

// Typed declaration of a Java static field of type T.
template <typename T>
class StaticField : public StaticFieldBase {
 public:
  using ResultType = T;
  using StaticFieldBase::StaticFieldBase;
};

// Untyped base class for JNI static method declarations.
//
// `clazz_` must be a JVM global `jclass` reference owned by the caller /
// `Loader`.
class StaticMethodBase {
 protected:
  struct State {
    jclass clazz;
    jmethodID id;
  };
 public:
  constexpr StaticMethodBase(const char* name, const char* sig) noexcept
      : name_(name ? name : ""), sig_(sig ? sig : "") {}
  explicit constexpr StaticMethodBase(jclass global_clazz,
                                      jmethodID id) noexcept
      : name_(""), sig_(""), clazz_{global_clazz}, id_{id} {}

  constexpr const char* name() const noexcept { return name_; }
  constexpr const char* sig() const noexcept { return sig_; }

  State Get() const noexcept {
    return {clazz_.load(std::memory_order_acquire),
            id_.load(std::memory_order_acquire)};
  }

  explicit operator bool() const noexcept {
    return clazz_.load(std::memory_order_acquire) != nullptr &&
           id_.load(std::memory_order_acquire) != nullptr;
  }

  void Set(JNIEnv* env, jclass clazz, jmethodID id) noexcept {
    JavaVM* jvm = internal::GlobalRefReaper::RecordJavaVM(env);
    jobject global_clazz = internal::GlobalRefReaper::NewGlobalRef(env, clazz, jvm);
    id_.store(id, std::memory_order_release);
    jclass old_clazz = clazz_.exchange(static_cast<jclass>(global_clazz), std::memory_order_acq_rel);
    if (old_clazz) {
      internal::GlobalRefReaper::DeleteOrEnqueue(old_clazz, env, jvm);
    }
  }

  void Reset(JNIEnv* env = nullptr) noexcept {
    jclass old_clazz = clazz_.exchange(nullptr, std::memory_order_acq_rel);
    id_.store(nullptr, std::memory_order_release);
    if (old_clazz) {
      internal::GlobalRefReaper::DeleteOrEnqueue(old_clazz, env);
    }
  }

 private:
  const char* name_ = "";
  const char* sig_ = "";
  std::atomic<jclass> clazz_{nullptr};
  std::atomic<jmethodID> id_{nullptr};
};

// Typed declaration of a Java static method returning T (or signature
// Ret(Args...)).
template <typename T>
class StaticMethod : public StaticMethodBase {
 public:
  using ResultType = T;
  static constexpr bool kHasTypedArgs = false;
  using StaticMethodBase::StaticMethodBase;
};

template <typename Ret, typename... Args>
class StaticMethod<Ret(Args...)> : public StaticMethodBase {
 public:
  using ResultType = Ret;
  using ArgsTuple = std::tuple<Args...>;
  static constexpr bool kHasTypedArgs = true;
  static constexpr size_t kArity = sizeof...(Args);
  using StaticMethodBase::StaticMethodBase;
};

}  // namespace jni
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_JNI_DECLARATION_H_
