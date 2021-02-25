#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_OWNERSHIP_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_OWNERSHIP_H_

#include <jni.h>

#include <string>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * An RAII wrapper for a local JNI reference that automatically deletes the JNI
 * local reference when it goes out of scope. Copies and moves are handled by
 * creating additional references as required.
 *
 * @tparam T A JNI reference wrapper, usually a subclass of `Object`.
 */
template <typename T>
class Local : public T {
 public:
  using jni_type = JniType<T>;

  Local() = default;

  /**
   * Adopts a local reference that is the result of a JNI invocation.
   */
  Local(JNIEnv* env, jni_type value) : T(value), env_(env) {}

  /**
   * An explicit copy constructor, which prevents accidental copies at function
   * call sites. Copies of a local reference should rarely be needed. For
   * example, when keeping a reference as a member of a C++ object or lambda,
   * you're almost exclusively better off promoting the local reference to a
   * global one to avoid problems that arise from the thread-local restrictions
   * of a local reference.
   */
  explicit Local(const Local& other) {  // NOLINT(google-explicit-constructor)
    EnsureEnv(other.env());
    T::object_ = env_->NewLocalRef(other.get());
  }

  Local& operator=(const Local& other) {
    if (T::object_ != other.get()) {
      EnsureEnv(other.env());
      env_->DeleteLocalRef(T::object_);
      T::object_ = env_->NewLocalRef(other.get());
    }
    return *this;
  }

  Local(Local&& other) noexcept : T(other.release()), env_(other.env_) {}

  Local& operator=(Local&& other) noexcept {
    if (T::object_ != other.get()) {
      EnsureEnv(other.env());
      env_->DeleteLocalRef(T::object_);
      T::object_ = other.release();
    }
    return *this;
  }

  Local(const Global<T>& other) {
    EnsureEnv();
    T::object_ = env_->NewLocalRef(other.get());
  }

  Local& operator=(const Global<T>& other) {
    EnsureEnv();
    env_->DeleteLocalRef(T::object_);
    T::object_ = env_->NewLocalRef(other.get());
    return *this;
  }

  Local(Global<T>&& other) noexcept {
    EnsureEnv();
    T::object_ = env_->NewLocalRef(other.get());
    env_->DeleteGlobalRef(other.release());
  }

  Local& operator=(Global<T>&& other) noexcept {
    EnsureEnv();
    env_->DeleteLocalRef(T::object_);
    T::object_ = env_->NewLocalRef(other.get());
    env_->DeleteGlobalRef(other.release());
    return *this;
  }

  ~Local() {
    if (env_ && T::object_) {
      env_->DeleteLocalRef(T::object_);
    }
  }

  /** Returns a non-owning proxy of type `U` that points to this object. */
  template <typename U>
  U CastTo() const& {
    return U(get());
  }

  /**
   * Converts this instance to a new local proxy of `U` that points to this
   * object. Equivalent to passing the result of `release()` to a new `Local<U>`
   * instance.
   */
  template <typename U>
  Local<U> CastTo() && {
    return Local<U>(env_, release());
  }

  jni_type get() const override { return static_cast<jni_type>(T::object_); }

  jni_type release() {
    jobject result = T::object_;
    T::object_ = nullptr;
    return static_cast<jni_type>(result);
  }

  void clear() {
    if (env_ && T::object_) {
      env_->DeleteLocalRef(T::object_);
      T::object_ = nullptr;
    }
  }

  JNIEnv* env() const { return env_; }

 private:
  void EnsureEnv(JNIEnv* other = nullptr) {
    if (env_ == nullptr) {
      if (other != nullptr) {
        env_ = other;
      } else {
        env_ = GetEnv();
      }
    }
  }

  JNIEnv* env_ = nullptr;
};

/**
 * Global references are almost always created by promoting local references.
 * Aside from `NewGlobalRef`, there are no JNI APIs that return global
 * references. You can construct a `Global` wrapper with `AdoptExisting::kYes`
 * in the rare case that you're interoperating with other APIs that produce
 * global JNI references.
 */
enum class AdoptExisting { kYes };

/**
 * An RAII wrapper for a global JNI reference, that automatically deletes the
 * JNI global reference when it goes out of scope. Copies and moves are handled
 * by creating additional references as required.
 *
 * @tparam T A JNI reference wrapper, usually a subclass of `Object`.
 */
template <typename T>
class Global : public T {
 public:
  using jni_type = JniType<T>;

  Global() = default;

  Global(jni_type object, AdoptExisting) : T(object) {}

  Global(const Local<T>& other) {
    JNIEnv* env = EnsureEnv(other.env());
    T::object_ = env->NewGlobalRef(other.get());
  }

  Global& operator=(const Local<T>& other) {
    JNIEnv* env = EnsureEnv(other.env());
    env->DeleteGlobalRef(T::object_);
    T::object_ = env->NewGlobalRef(other.get());
    return *this;
  }

  Global(const T& other) {
    JNIEnv* env = GetEnv();
    T::object_ = env->NewGlobalRef(other.get());
  }

  Global& operator=(const T& other) {
    if (T::object_ != other.get()) {
      JNIEnv* env = GetEnv();
      env->DeleteGlobalRef(T::object_);
      T::object_ = env->NewGlobalRef(other.get());
    }
    return *this;
  }

  // Explicitly declare a copy constructor and copy assignment operator because
  // there's an explicitly declared move constructor for this type.
  //
  // Without this, the implicitly-defined copy constructor would be deleted, and
  // during overload resolution the deleted copy constructor would take priority
  // over the looser match above that takes `const T&`.
  Global(const Global& other) : Global(static_cast<const T&>(other)) {}

  Global& operator=(const Global& other) {
    *this = static_cast<const T&>(other);
    return *this;
  }

  Global(Global&& other) noexcept : T(other.release()) {}

  Global& operator=(Global&& other) noexcept {
    if (T::object_ != other.get()) {
      if (T::object_) {
        JNIEnv* env = GetEnv();
        env->DeleteGlobalRef(T::object_);
      }
      T::object_ = other.release();
    }
    return *this;
  }

  Global(Local<T>&& other) noexcept {
    JNIEnv* env = EnsureEnv(other.env());
    T::object_ = env->NewGlobalRef(other.get());
    env->DeleteLocalRef(other.release());
  }

  Global& operator=(Local<T>&& other) noexcept {
    JNIEnv* env = EnsureEnv(other.env());
    env->DeleteGlobalRef(T::object_);
    T::object_ = env->NewGlobalRef(other.get());
    env->DeleteLocalRef(other.release());
    return *this;
  }

  ~Global() {
    if (T::object_) {
      JNIEnv* env = GetEnv();
      env->DeleteGlobalRef(T::object_);
    }
  }

  jni_type get() const override { return static_cast<jni_type>(T::object_); }

  jni_type release() {
    jobject result = T::object_;
    T::object_ = nullptr;
    return static_cast<jni_type>(result);
  }

  void clear() {
    if (T::object_) {
      JNIEnv* env = GetEnv();
      env->DeleteGlobalRef(T::object_);
      T::object_ = nullptr;
    }
  }

 private:
  JNIEnv* EnsureEnv(JNIEnv* other = nullptr) {
    if (other != nullptr) {
      return other;
    } else {
      return GetEnv();
    }
  }
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_OWNERSHIP_H_
