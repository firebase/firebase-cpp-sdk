// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_OWNERSHIP_H_
#define FIREBASE_FIRESTORE_SRC_JNI_OWNERSHIP_H_

#include <jni.h>

#include <utility>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/traits.h"

namespace firebase::firestore::jni {

class GlobalRefReaper {
 public:
  static GlobalRefReaper& GetInstance();
  void DeferDelete(jobject ref);
  void Reap(JNIEnv* env);
};

template <typename T>
class Local : public T {
 public:
  using jni_type = JniType<T>;

  Local() = default;
  Local(JNIEnv* env, jni_type value) : T(value), env_(env) {}

  explicit Local(const Local& other) {  // NOLINT(google-explicit-constructor)
    if (other.get()) {
      if (EnsureEnv(other.env())) T::object_ = env_->NewLocalRef(other.get());
    } else if (other.env()) {
      env_ = other.env();
    }
  }

  Local& operator=(const Local& other) {
    if (this != &other) {
      clear();
      if (other.get()) {
        if (EnsureEnv(other.env())) T::object_ = env_->NewLocalRef(other.get());
      } else if (!env_ && other.env()) {
        env_ = other.env();
      }
    }
    return *this;
  }

  Local(Local&& other) noexcept : T(other.release()), env_(other.env_) {}

  Local& operator=(Local&& other) noexcept {
    if (this != &other) {
      if (other.env_) EnsureEnv(other.env_);
      clear();
      T::object_ = other.release();
    }
    return *this;
  }

  Local(const Global<T>& other) { *this = other; }

  Local& operator=(const Global<T>& other) {
    clear();
    if (other.get() && EnsureEnv()) T::object_ = env_->NewLocalRef(other.get());
    return *this;
  }

  Local(Global<T>&& other) noexcept { *this = std::move(other); }

  Local& operator=(Global<T>&& other) noexcept {
    clear();
    if (other.get() && EnsureEnv()) {
      T::object_ = env_->NewLocalRef(other.get());
      env_->DeleteGlobalRef(other.release());
    }
    return *this;
  }

  ~Local() { clear(); }

  template <typename U>
  U CastTo() const& { return U(static_cast<JniType<U>>(get())); }
  template <typename U>
  Local<U> CastTo() && {
    return Local<U>(env_, static_cast<JniType<U>>(release()));
  }

  jni_type get() const override { return static_cast<jni_type>(T::object_); }
  jni_type release() {
    return static_cast<jni_type>(std::exchange(T::object_, nullptr));
  }

  void clear() {
    if (T::object_) {
      if (env_) env_->DeleteLocalRef(T::object_);
      T::object_ = nullptr;
    }
  }

  JNIEnv* env() const { return env_; }

 private:
  JNIEnv* EnsureEnv(JNIEnv* other = nullptr) {
    if (!env_) env_ = other ? other : GetEnv();
    return env_;
  }
  JNIEnv* env_ = nullptr;
};

enum class AdoptExisting { kYes };

template <typename T>
class Global : public T {
 public:
  using jni_type = JniType<T>;

  Global() = default;
  Global(jni_type object, AdoptExisting) : T(object) {}
  Global(const Local<T>& other) { *this = other; }

  Global& operator=(const Local<T>& other) {
    JNIEnv* env = (T::object_ || other.get()) ? EnsureEnv(other.env()) : nullptr;
    DeleteRef(env);
    if (other.get() && env) T::object_ = env->NewGlobalRef(other.get());
    return *this;
  }

  Global(const T& other) { *this = other; }

  Global& operator=(const T& other) {
    if (this != &other) {
      JNIEnv* env = (T::object_ || other.get()) ? GetEnv() : nullptr;
      jobject ref =
          (other.get() && env) ? env->NewGlobalRef(other.get()) : nullptr;
      DeleteRef(env);
      T::object_ = ref;
    }
    return *this;
  }

  Global(const Global& other) : Global(static_cast<const T&>(other)) {}
  Global& operator=(const Global& other) {
    return *this = static_cast<const T&>(other);
  }

  Global(Global&& other) noexcept : T(other.release()) {}
  Global& operator=(Global&& other) noexcept {
    if (this != &other) {
      clear();
      T::object_ = other.release();
    }
    return *this;
  }

  Global(Local<T>&& other) noexcept { *this = std::move(other); }

  Global& operator=(Local<T>&& other) noexcept {
    JNIEnv* env = (T::object_ || other.get()) ? EnsureEnv(other.env()) : nullptr;
    DeleteRef(env);
    if (other.get() && env) {
      T::object_ = env->NewGlobalRef(other.get());
      env->DeleteLocalRef(other.release());
    }
    return *this;
  }

  ~Global() { clear(); }

  jni_type get() const override { return static_cast<jni_type>(T::object_); }
  jni_type release() {
    return static_cast<jni_type>(std::exchange(T::object_, nullptr));
  }
  void clear() { DeleteRef(T::object_ ? GetAttachedEnv() : nullptr); }

 private:
  void DeleteRef(JNIEnv* env) {
    if (!T::object_) return;
    if (env) {
      env->DeleteGlobalRef(T::object_);
    } else {
      GlobalRefReaper::GetInstance().DeferDelete(T::object_);
    }
    T::object_ = nullptr;
  }
  static JNIEnv* EnsureEnv(JNIEnv* other = nullptr) {
    return other ? other : GetEnv();
  }
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_OWNERSHIP_H_
