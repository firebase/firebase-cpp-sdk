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

#ifndef FIREBASE_APP_SRC_JNI_OWNERSHIP_H_
#define FIREBASE_APP_SRC_JNI_OWNERSHIP_H_

#include <jni.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include "app/src/jni/traits.h"

namespace firebase {
namespace jni {

class Env;

namespace internal {

// Converts a JNIEnv** to void** or JNIEnv** as required by the platform's
// JavaVM::GetEnv / AttachCurrentThread signature.
class JniEnvOut {
 public:
  explicit JniEnvOut(JNIEnv** env) noexcept : env_(env), void_env_(nullptr) {}
  ~JniEnvOut() {
    if (void_env_ != nullptr) {
      *env_ = static_cast<JNIEnv*>(void_env_);
    }
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator JNIEnv**() noexcept { return env_; }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator void**() noexcept { return &void_env_; }

 private:
  JNIEnv** env_;
  void* void_env_;
};

// Thread-safe storage for the process JavaVM* and deferred global reference
// reclamation when GlobalRef instances are destroyed on unattached C++ threads.
// Uses a function-local leaked singleton so static Global<T> destructors can
// safely enqueue/delete references during process shutdown without risking the
// Static Destruction Order Fiasco (SDOF).
class GlobalRefReaper {
 public:
  static void SetJavaVM(JavaVM* jvm) noexcept {
    if (jvm) {
      State& state = GetState();
      JavaVM* expected = nullptr;
      state.jvm.compare_exchange_strong(expected, jvm,
                                        std::memory_order_acq_rel);
    }
  }

  static JavaVM* RecordJavaVM(JNIEnv* env) noexcept {
    if (!env) return GetJavaVM();
    State& state = GetState();
    JavaVM* cached = state.jvm.load(std::memory_order_acquire);
    if (cached) return cached;
    if (env->ExceptionCheck()) return nullptr;
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) == JNI_OK && vm != nullptr) {
      SetJavaVM(vm);
      return vm;
    }
    return nullptr;
  }

  static JavaVM* GetJavaVM() noexcept {
    return GetState().jvm.load(std::memory_order_acquire);
  }

  // Returns JNIEnv* ONLY if the current thread is already attached to the JVM.
  // Never attaches an unattached thread.
  static JNIEnv* GetAttachedEnv(JavaVM* vm = nullptr) noexcept {
    if (!vm) vm = GetJavaVM();
    if (!vm) return nullptr;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(JniEnvOut(&env), JNI_VERSION_1_6) == JNI_OK) {
      return env;
    }
    return nullptr;
  }

  // Deletes any deferred global references using the provided attached JNIEnv*.
  // Fast-paths with an atomic acquire load when no deletions are pending.
  static void Flush(JNIEnv* env) noexcept {
    if (!env) return;
    State& state = GetState();
    if (state.pending_count.load(std::memory_order_acquire) == 0) return;
    if (env->ExceptionCheck()) return;
    std::vector<jobject> to_delete;
    {
      std::lock_guard<std::mutex> lock(state.mutex);
      if (state.pending_deletes.empty()) return;
      to_delete.swap(state.pending_deletes);
      state.pending_count.store(0, std::memory_order_release);
    }
    for (jobject ref : to_delete) {
      if (ref) {
        env->DeleteGlobalRef(ref);
      }
    }
  }

  // Creates a new JNI global reference for `ref`, temporarily attaching the
  // current thread if `env == nullptr` and the current thread is unattached so
  // copying `Global<T>` on native C++ worker threads never drops the reference.
  static jobject NewGlobalRef(JNIEnv* env, jobject ref,
                              JavaVM* vm = nullptr) noexcept {
    if (!ref) return nullptr;
    if (env) {
      RecordJavaVM(env);
      Flush(env);
      return NewGlobalRefWithEnv(env, ref);
    }
    if (!vm) vm = GetJavaVM();
    if (!vm) return nullptr;
    JNIEnv* attached_env = nullptr;
    const jint status = vm->GetEnv(JniEnvOut(&attached_env), JNI_VERSION_1_6);
    if (status == JNI_OK && attached_env != nullptr) {
      Flush(attached_env);
      return NewGlobalRefWithEnv(attached_env, ref);
    }
    if (status == JNI_EDETACHED) {
      if (vm->AttachCurrentThread(JniEnvOut(&attached_env), nullptr) ==
              JNI_OK &&
          attached_env != nullptr) {
        Flush(attached_env);
        jobject global_ref = NewGlobalRefWithEnv(attached_env, ref);
        if (attached_env->ExceptionCheck()) {
          attached_env->ExceptionClear();
        }
        vm->DetachCurrentThread();
        return global_ref;
      }
    }
    return nullptr;
  }

  // Deletes `ref` immediately if the current thread is attached to the JVM, or
  // queues it for deletion on the next JNI operation if the current thread is
  // detached.
  static void DeleteOrEnqueue(jobject ref, JNIEnv* env = nullptr,
                              JavaVM* vm = nullptr) noexcept {
    if (!ref) return;
    if (!env) {
      env = GetAttachedEnv(vm);
    }
    if (env) {
      Flush(env);
      env->DeleteGlobalRef(ref);
      return;
    }
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
      state.pending_deletes.push_back(ref);
      state.pending_count.store(state.pending_deletes.size(),
                                std::memory_order_release);
    } catch (...) {
      // Just leak if we cannot allocate memory to queue the deletion.
    }
#else
    state.pending_deletes.push_back(ref);
    state.pending_count.store(state.pending_deletes.size(),
                              std::memory_order_release);
#endif
  }

  static size_t PendingCount() noexcept {
    return GetState().pending_count.load(std::memory_order_acquire);
  }

 private:
  struct State {
    std::atomic<JavaVM*> jvm{nullptr};
    std::atomic<size_t> pending_count{0};
    std::mutex mutex;
    std::vector<jobject> pending_deletes;
  };

  static State& GetState() noexcept {
    static State* const state = new State();
    return *state;
  }

  static jobject NewGlobalRefWithEnv(JNIEnv* env, jobject ref) noexcept {
    if (!env || !ref) return nullptr;
    jthrowable pending = nullptr;
    if (env->ExceptionCheck()) {
      pending = env->ExceptionOccurred();
      env->ExceptionClear();
    }
    jobject global_ref = env->NewGlobalRef(ref);
    if (!global_ref) {
      if (pending) {
        env->ExceptionClear();
        env->Throw(pending);
        env->DeleteLocalRef(pending);
      }
      return nullptr;
    }
    if (pending) {
      env->Throw(pending);
      env->DeleteLocalRef(pending);
    }
    return global_ref;
  }
};

// Base storage for Local/Global/Borrowed that supports both:
// 1. Raw JNI pointer types (jobject, jstring, jclass, jbyteArray, etc.)
// 2. Wrapper classes that inherit from Object (having .get() and JniType).
template <typename T, bool IsRawPointer = std::is_pointer_v<T>>
class ReferenceBase : public T {
 public:
  using JniType = typename T::JniType;

  constexpr ReferenceBase() noexcept = default;
  explicit constexpr ReferenceBase(JniType object) noexcept : T(object) {}
  explicit constexpr ReferenceBase(const T& object) noexcept
      : T(object.get()) {}
};

template <typename T>
class ReferenceBase<T, true> {
 public:
  using JniType = T;

  constexpr ReferenceBase() noexcept = default;
  explicit constexpr ReferenceBase(T object) noexcept : object_(object) {}

  constexpr explicit operator bool() const noexcept {
    return object_ != nullptr;
  }

  constexpr T get() const noexcept { return object_; }
  constexpr T operator*() const noexcept { return object_; }

 protected:
  T object_ = nullptr;
};

}  // namespace internal

// Base wrapper class for non-owning JNI object handles.
// Non-virtual so it has zero vptr overhead and supports C++17 constexpr
// construction, with protected assignment operators to prevent object slicing
// on Local<T> / Global<T> references.
class Object {
 public:
  using JniType = jobject;

  constexpr Object() noexcept = default;
  explicit constexpr Object(jobject object) noexcept : object_(object) {}
  constexpr Object(const Object&) noexcept = default;
  constexpr Object(Object&&) noexcept = default;
  ~Object() = default;

  constexpr explicit operator bool() const noexcept {
    return object_ != nullptr;
  }

  constexpr jobject get() const noexcept { return object_; }
  constexpr jobject operator*() const noexcept { return object_; }

  template <typename U>
  constexpr U CastTo() const noexcept {
    return U(static_cast<typename JniTypeMap<U>::type>(object_));
  }

 protected:
  constexpr Object& operator=(const Object&) noexcept = default;
  constexpr Object& operator=(Object&&) noexcept = default;

  jobject object_ = nullptr;
};

template <typename T>
class Local;
template <typename T>
class Global;
template <typename T>
class Borrowed;

// Non-owning wrapper around a JNI reference valid for the current scope.
template <typename T = jobject>
class Borrowed : public internal::ReferenceBase<T> {
  using Base = internal::ReferenceBase<T>;

 public:
  using JniType = typename Base::JniType;

  constexpr Borrowed() noexcept = default;
  explicit constexpr Borrowed(JniType object) noexcept : Base(object) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Borrowed(const Local<T>& local) noexcept : Base(static_cast<JniType>(local.get())) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Borrowed(const Global<T>& global) noexcept : Base(static_cast<JniType>(global.get())) {}

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Borrowed(const Borrowed<U>& other) noexcept : Base(static_cast<JniType>(other.get())) {}

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Borrowed(const Local<U>& local) noexcept : Base(static_cast<JniType>(local.get())) {}

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Borrowed(const Global<U>& global) noexcept : Base(static_cast<JniType>(global.get())) {}

  constexpr Borrowed(const Borrowed&) noexcept = default;
  constexpr Borrowed(Borrowed&&) noexcept = default;
  constexpr Borrowed& operator=(const Borrowed&) noexcept = default;
  constexpr Borrowed& operator=(Borrowed&&) noexcept = default;

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  constexpr Borrowed& operator=(const Borrowed<U>& other) noexcept {
    this->object_ = static_cast<JniType>(other.get());
    return *this;
  }

  template <typename U>
  constexpr Borrowed<U> CastTo() const noexcept {
    return Borrowed<U>(static_cast<typename JniTypeMap<U>::type>(this->get()));
  }
};

template <typename T = jobject>
using NonOwning = Borrowed<T>;

// RAII wrapper for a JNI local reference. Deletes the local reference when it
// goes out of scope. Works with both raw JNI types (e.g. Local<jobject>,
// Local<jstring>, Local<jclass>) and Object-derived wrapper types.
template <typename T = jobject>
class Local : public internal::ReferenceBase<T> {
  using Base = internal::ReferenceBase<T>;

 public:
  using JniType = typename Base::JniType;

  constexpr Local() noexcept = default;

  Local(JNIEnv* env, JniType object, bool adopt_existing_local = false) noexcept : Base(nullptr), env_(env) {
    if (env) {
      internal::GlobalRefReaper::RecordJavaVM(env);
    }
    if (adopt_existing_local) {
      this->object_ = object;
    } else if (object) {
      AssignFromLocal(env, object);
    }
  }

  explicit Local(const Local& other) noexcept : Base(nullptr) {
    CopyFrom(other.env_, other);
  }

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  explicit Local(const Local<U>& other) noexcept : Base(nullptr) {
    CopyFrom(other.env(), other);
  }

  explicit Local(const Global<T>& other) noexcept : Base(nullptr) {
    if (other.get()) {
      JNIEnv* env = internal::GlobalRefReaper::GetAttachedEnv(other.jvm());
      CopyFrom(env, other);
    }
  }

  Local(Local&& other) noexcept
      : Base(other.release()), env_(other.env_) {}

  template <typename U,
            typename = std::enable_if_t<
                !std::is_same_v<T, U> &&
                (std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                 std::is_base_of_v<T, U>)>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Local(Local<U>&& other) noexcept
      : Base(static_cast<JniType>(other.release())), env_(other.env()) {}

  ~Local() noexcept { reset(); }

  Local& operator=(const Local& other) noexcept {
    if (this != &other) {
      AssignFromLocal(other.env_, static_cast<JniType>(other.get()));
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                !std::is_same_v<T, U> &&
                (std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                 std::is_base_of_v<T, U>)>>
  Local& operator=(const Local<U>& other) noexcept {
    AssignFromLocal(other.env(), static_cast<JniType>(other.get()));
    return *this;
  }

  Local& operator=(Local&& other) noexcept {
    if (this != &other) {
      JNIEnv* next_env = other.env_ ? other.env_ : env_;
      JniType next_obj = other.release();
      reset(next_env, next_obj);
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                !std::is_same_v<T, U> &&
                (std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                 std::is_base_of_v<T, U>)>>
  Local& operator=(Local<U>&& other) noexcept {
    JNIEnv* next_env = other.env() ? other.env() : env_;
    JniType next_obj = static_cast<JniType>(other.release());
    reset(next_env, next_obj);
    return *this;
  }

  void clear() noexcept { reset(); }

  void reset() noexcept {
    if (env_ && this->object_) {
      env_->DeleteLocalRef(this->object_);
    }
    this->object_ = nullptr;
  }

  void reset(JNIEnv* env, JniType object) noexcept {
    if (env) {
      internal::GlobalRefReaper::RecordJavaVM(env);
    }
    if (object != nullptr && object == this->object_) {
      if (env) env_ = env;
      return;
    }
    if (env_ && this->object_) {
      env_->DeleteLocalRef(this->object_);
    }
    env_ = env;
    this->object_ = object;
  }

  JniType release() noexcept {
    JniType result = static_cast<JniType>(this->get());
    this->object_ = nullptr;
    return result;
  }

  JNIEnv* env() const noexcept { return env_; }

  template <typename U>
  U CastTo() const& noexcept {
    return U(static_cast<typename JniTypeMap<U>::type>(this->get()));
  }

  template <typename U>
  Local<U> CastTo() && noexcept {
    return Local<U>(env_,
                    static_cast<typename JniTypeMap<U>::type>(release()),
                    /*adopt_existing_local=*/true);
  }

 private:
  template <typename Ref>
  void CopyFrom(JNIEnv* env, const Ref& other) noexcept {
    env_ = env;
    if (env_ && other.get()) {
      jthrowable pending = nullptr;
      if (env_->ExceptionCheck()) {
        pending = env_->ExceptionOccurred();
        env_->ExceptionClear();
      }
      this->object_ = static_cast<JniType>(env_->NewLocalRef(other.get()));
      if (pending) {
        env_->Throw(pending);
        env_->DeleteLocalRef(pending);
      }
    } else {
      this->object_ = nullptr;
    }
  }

  void AssignFromLocal(JNIEnv* src_env, JniType src_obj) noexcept {
    if (!src_obj) {
      reset();
      return;
    }
    JNIEnv* target_env = src_env ? src_env : env_;
    JniType new_ref = nullptr;
    if (target_env) {
      jthrowable pending = nullptr;
      if (target_env->ExceptionCheck()) {
        pending = target_env->ExceptionOccurred();
        target_env->ExceptionClear();
      }
      new_ref = static_cast<JniType>(target_env->NewLocalRef(src_obj));
      if (pending) {
        target_env->Throw(pending);
        target_env->DeleteLocalRef(pending);
      }
    }
    if (target_env && !new_ref) {
      return;
    }
    reset(target_env, new_ref);
  }

  JNIEnv* env_ = nullptr;
};

template <typename T = jobject>
using ScopedLocalRef = Local<T>;

// RAII wrapper for a JNI global reference. Deletes the global reference when it
// goes out of scope (or queues it in GlobalRefReaper if destroyed on an
// unattached thread).
template <typename T = jobject>
class Global : public internal::ReferenceBase<T> {
  using Base = internal::ReferenceBase<T>;
  template <typename U>
  friend class Global;

 public:
  using JniType = typename Base::JniType;

  constexpr Global() noexcept = default;

  // Adopts or promotes a JNI reference using an explicit JNIEnv*.
  Global(JNIEnv* env, JniType object, bool adopt_existing_global = false) noexcept
      : Base(nullptr), jvm_(internal::GlobalRefReaper::RecordJavaVM(env)) {
    if (object) {
      if (adopt_existing_global) {
        if (env) internal::GlobalRefReaper::Flush(env);
        this->object_ = object;
      } else {
        this->object_ = static_cast<JniType>(
            internal::GlobalRefReaper::NewGlobalRef(env, object, jvm_));
      }
    }
  }

  explicit Global(const Global& other) noexcept
      : Base(nullptr), jvm_(other.jvm_) {
    if (other.get()) {
      CopyFrom(nullptr, static_cast<JniType>(other.get()));
    }
  }

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  explicit Global(const Global<U>& other) noexcept
      : Base(nullptr), jvm_(other.jvm()) {
    if (other.get()) {
      CopyFrom(nullptr, static_cast<JniType>(other.get()));
    }
  }

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  explicit Global(const Local<U>& other) noexcept
      : Base(nullptr),
        jvm_(internal::GlobalRefReaper::RecordJavaVM(other.env())) {
    if (other.get()) {
      CopyFrom(other.env(), static_cast<JniType>(other.get()));
    }
  }

  template <typename U = T,
            typename = std::enable_if_t<!std::is_pointer_v<U>>>
  explicit Global(const T& other) noexcept
      : Base(nullptr), jvm_(internal::GlobalRefReaper::GetJavaVM()) {
    if (other.get()) {
      CopyFrom(nullptr, static_cast<JniType>(other.get()));
    }
  }

  Global(Global&& other) noexcept
      : Base(other.release()), jvm_(other.jvm_) {
    other.jvm_ = nullptr;
  }

  template <typename U,
            typename = std::enable_if_t<
                !std::is_same_v<T, U> &&
                (std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                 std::is_base_of_v<T, U>)>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Global(Global<U>&& other) noexcept
      : Base(static_cast<JniType>(other.release())), jvm_(other.jvm()) {}

  ~Global() noexcept { reset(); }

  Global& operator=(const Global& other) noexcept {
    if (this != &other) {
      if (!other.get()) {
        reset();
      } else {
        if (other.jvm_) jvm_ = other.jvm_;
        CopyAssign(nullptr, static_cast<JniType>(other.get()));
      }
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                !std::is_same_v<T, U> &&
                (std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                 std::is_base_of_v<T, U>)>>
  Global& operator=(const Global<U>& other) noexcept {
    if (!other.get()) {
      reset();
    } else {
      if (other.jvm()) jvm_ = other.jvm();
      CopyAssign(nullptr, static_cast<JniType>(other.get()));
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                std::is_base_of_v<T, U>>>
  Global& operator=(const Local<U>& other) noexcept {
    if (!other.get()) {
      reset(other.env(), nullptr);
    } else {
      if (other.env()) {
        jvm_ = internal::GlobalRefReaper::RecordJavaVM(other.env());
      }
      CopyAssign(other.env(), static_cast<JniType>(other.get()));
    }
    return *this;
  }

  Global& operator=(Global&& other) noexcept {
    if (this != &other) {
      reset();
      this->object_ = other.release();
      jvm_ = other.jvm_;
      other.jvm_ = nullptr;
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                !std::is_same_v<T, U> &&
                (std::is_convertible_v<typename JniTypeMap<U>::type, JniType> ||
                 std::is_base_of_v<T, U>)>>
  Global& operator=(Global<U>&& other) noexcept {
    reset();
    this->object_ = static_cast<JniType>(other.release());
    jvm_ = other.jvm();
    return *this;
  }

  void clear() noexcept { reset(); }

  void reset() noexcept {
    if (this->object_) {
      internal::GlobalRefReaper::DeleteOrEnqueue(this->object_, nullptr, jvm_);
      this->object_ = nullptr;
    }
  }

  void reset(JNIEnv* env, JniType object = nullptr,
             bool adopt_existing_global = false) noexcept {
    if (env) {
      jvm_ = internal::GlobalRefReaper::RecordJavaVM(env);
    }
    if (object != nullptr && object == this->object_) {
      return;
    }
    JniType new_ref = nullptr;
    if (object != nullptr) {
      if (adopt_existing_global) {
        if (env) internal::GlobalRefReaper::Flush(env);
        new_ref = object;
      } else {
        new_ref = static_cast<JniType>(
            internal::GlobalRefReaper::NewGlobalRef(env, object, jvm_));
        if (!new_ref) return;
      }
    }
    if (this->object_) {
      internal::GlobalRefReaper::DeleteOrEnqueue(this->object_, env, jvm_);
    }
    this->object_ = new_ref;
  }

  JniType release() noexcept {
    JniType result = static_cast<JniType>(this->get());
    this->object_ = nullptr;
    return result;
  }

  JavaVM* jvm() const noexcept { return jvm_; }

  template <typename U>
  U CastTo() const noexcept {
    return U(static_cast<typename JniTypeMap<U>::type>(this->get()));
  }

  template <typename U>
  Global<U> CastTo() && noexcept {
    Global<U> result;
    result.jvm_ = jvm_;
    result.object_ = static_cast<typename JniTypeMap<U>::type>(release());
    return result;
  }

 private:
  void CopyFrom(JNIEnv* env, JniType src) noexcept {
    if (!src) {
      this->object_ = nullptr;
      return;
    }
    this->object_ = static_cast<JniType>(
        internal::GlobalRefReaper::NewGlobalRef(env, src, jvm_));
  }

  void CopyAssign(JNIEnv* env, JniType src) noexcept {
    if (src != nullptr && src == this->object_) {
      return;
    }
    JniType new_ref = nullptr;
    if (src != nullptr) {
      new_ref = static_cast<JniType>(
          internal::GlobalRefReaper::NewGlobalRef(env, src, jvm_));
      if (!new_ref) return;
    }
    if (this->object_) {
      internal::GlobalRefReaper::DeleteOrEnqueue(this->object_, env, jvm_);
    }
    this->object_ = new_ref;
  }

  JavaVM* jvm_ = nullptr;
};

template <typename T = jobject>
using GlobalRef = Global<T>;

}  // namespace jni
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_JNI_OWNERSHIP_H_
