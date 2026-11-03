/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_FIRESTORE_SRC_JNI_OWNERSHIP_H_
#define FIREBASE_FIRESTORE_SRC_JNI_OWNERSHIP_H_

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
    if (other.get() != nullptr) {
      EnsureEnv(other.env());
      if (env_ != nullptr) {
        T::object_ = env_->NewLocalRef(other.get());
      }
    } else if (other.env() != nullptr) {
      env_ = other.env();
    }
  }

  Local& operator=(const Local& other) {
    if (this != &other) {
      clear();
      if (other.get() != nullptr) {
        EnsureEnv(other.env());
        if (env_ != nullptr) {
          T::object_ = env_->NewLocalRef(other.get());
        }
      } else if (env_ == nullptr && other.env() != nullptr) {
        env_ = other.env();
      }
    }
    return *this;
  }

  Local(Local&& other) noexcept : T(other.release()), env_(other.env_) {}

  Local& operator=(Local&& other) noexcept {
    if (this != &other) {
      if (other.env_ != nullptr) {
        EnsureEnv(other.env_);
      }
      clear();
      T::object_ = other.release();
    }
    return *this;
  }

  Local(const Global<T>& other) {
    if (other.get() != nullptr) {
      EnsureEnv();
      if (env_ != nullptr) {
        T::object_ = env_->NewLocalRef(other.get());
      }
    }
  }

  Local& operator=(const Global<T>& other) {
    clear();
    if (other.get() != nullptr) {
      EnsureEnv();
      if (env_ != nullptr) {
        T::object_ = env_->NewLocalRef(other.get());
      }
    }
    return *this;
  }

  Local(Global<T>&& other) noexcept {
    if (other.get() != nullptr) {
      EnsureEnv();
      if (env_ != nullptr) {
        T::object_ = env_->NewLocalRef(other.get());
        env_->DeleteGlobalRef(other.release());
      }
    }
  }

  Local& operator=(Global<T>&& other) noexcept {
    clear();
    if (other.get() != nullptr) {
      EnsureEnv();
      if (env_ != nullptr) {
        T::object_ = env_->NewLocalRef(other.get());
        env_->DeleteGlobalRef(other.release());
      }
    }
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
    return U(static_cast<JniType<U>>(get()));
  }

  /**
   * Converts this instance to a new local proxy of `U` that points to this
   * object. Equivalent to passing the result of `release()` to a new `Local<U>`
   * instance.
   */
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
      if (env_) {
        env_->DeleteLocalRef(T::object_);
      }
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
    if (other.get() != nullptr) {
      JNIEnv* env = EnsureEnv(other.env());
      if (env != nullptr) {
        T::object_ = env->NewGlobalRef(other.get());
      }
    }
  }

  Global& operator=(const Local<T>& other) {
    JNIEnv* env = nullptr;
    if (T::object_ != nullptr || other.get() != nullptr) {
      env = EnsureEnv(other.env());
    }
    if (T::object_ != nullptr) {
      if (env != nullptr) env->DeleteGlobalRef(T::object_);
      T::object_ = nullptr;
    }
    if (other.get() != nullptr && env != nullptr) {
      T::object_ = env->NewGlobalRef(other.get());
    }
    return *this;
  }

  Global(const T& other) {
    if (other.get() != nullptr) {
      JNIEnv* env = GetEnv();
      if (env != nullptr) {
        T::object_ = env->NewGlobalRef(other.get());
      }
    }
  }

  Global& operator=(const T& other) {
    if (this != &other) {
      JNIEnv* env = nullptr;
      if (T::object_ != nullptr || other.get() != nullptr) {
        env = GetEnv();
      }
      jobject new_ref = nullptr;
      if (other.get() != nullptr && env != nullptr) {
        new_ref = env->NewGlobalRef(other.get());
      }
      if (T::object_ != nullptr && env != nullptr) {
        env->DeleteGlobalRef(T::object_);
      }
      T::object_ = new_ref;
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
    if (this != &other) {
      clear();
      T::object_ = other.release();
    }
    return *this;
  }

  Global(Local<T>&& other) noexcept {
    if (other.get() != nullptr) {
      JNIEnv* env = EnsureEnv(other.env());
      if (env != nullptr) {
        T::object_ = env->NewGlobalRef(other.get());
        env->DeleteLocalRef(other.release());
      }
    }
  }

  Global& operator=(Local<T>&& other) noexcept {
    JNIEnv* env = nullptr;
    if (T::object_ != nullptr || other.get() != nullptr) {
      env = EnsureEnv(other.env());
    }
    if (T::object_ != nullptr) {
      if (env != nullptr) env->DeleteGlobalRef(T::object_);
      T::object_ = nullptr;
    }
    if (other.get() != nullptr && env != nullptr) {
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

  void clear() {
    if (T::object_) {
      JNIEnv* env = GetEnv();
      if (env) {
        env->DeleteGlobalRef(T::object_);
      }
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

#endif  // FIREBASE_FIRESTORE_SRC_JNI_OWNERSHIP_H_
