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

#include <map>
#include <vector>
#include <string>

#include "app/src/embedded_file.h"
namespace firebase {
namespace jni {
  inline std::vector<::firebase::internal::EmbeddedFile> CacheEmbeddedFiles(
      JNIEnv* env, jobject activity, const std::vector<::firebase::internal::EmbeddedFile>& files) {
    return files;
  }
}
}



namespace firebase {
namespace jni {
  enum MethodType {
    kMethodTypeInstance = 0,
    kMethodTypeStatic = 1,
    kMethodOptional = 2,
    kMethodTypeStaticOptional = 3,
  };
  enum FieldType {
    kFieldTypeInstance = 0,
    kFieldTypeStatic = 1,
    kFieldOptional = 2,
    kFieldTypeStaticOptional = 3,
  };
}
}

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


// -------------------------------------------------------------------------
// Boilerplate reduction macro for JNI class/method lookup

#define METHOD_LOOKUP_NONE(X)
#define FIELD_LOOKUP_NONE(X)

#define JNI_METHOD_LOOKUP_DECLARATION(NAMESPACE, METHODS_MACRO, FIELDS_MACRO, CLASS_NAME) \
  namespace NAMESPACE { \
    enum Method { \
      METHODS_MACRO(JNI_METHOD_ENUM) \
      kMethodCount \
    }; \
    enum Field { \
      FIELDS_MACRO(JNI_FIELD_ENUM) \
      kFieldCount \
    }; \
    inline jclass GetClass(); \
    inline jmethodID GetMethodId(Method method); \
    inline jfieldID GetFieldId(Field field); \
    bool CacheMethodIds(JNIEnv* env, jobject activity = nullptr); \
    bool CacheFieldIds(JNIEnv* env, jobject activity = nullptr); \
    inline void ReleaseClass(JNIEnv* env); \
    jclass CacheClassFromFiles(JNIEnv* env, jobject activity = nullptr, const void* embedded_files = nullptr); \
    inline bool RegisterNatives(JNIEnv* env, const JNINativeMethod* methods, size_t num_methods); \
  } \
  namespace NAMESPACE { \
    inline ::firebase::jni::Global<jclass> g_class; \
    inline jmethodID g_method_ids[kMethodCount > 0 ? kMethodCount : 1]; \
    inline jfieldID g_field_ids[kFieldCount > 0 ? kFieldCount : 1]; \
    inline jclass GetClass() { return g_class.get(); } \
    inline jmethodID GetMethodId(Method method) { return g_method_ids[method]; } \
    inline jfieldID GetFieldId(Field field) { return g_field_ids[field]; } \
    inline bool CacheMethodIds(JNIEnv* env, jobject activity) { \
      if (g_class) return true; \
      jclass local_clazz = env->FindClass(CLASS_NAME); \
      if (!local_clazz) { \
        (void)::firebase::jni::CheckAndClearException(env); \
        return false; \
      } \
      g_class = ::firebase::jni::Global<jclass>(env, local_clazz); \
      env->DeleteLocalRef(local_clazz); \
      struct MethodData { int id; const char* name; const char* sig; int type; }; \
      static const MethodData kMethods[] = { \
        METHODS_MACRO(JNI_METHOD_DATA) \
        {-1, nullptr, nullptr, 0} \
      }; \
      for (const auto& m : kMethods) { \
        if (m.id == -1) break; \
        if (m.type == 1 || m.type == 3) { \
          g_method_ids[m.id] = env->GetStaticMethodID(g_class.get(), m.name, m.sig); \
        } else { \
          g_method_ids[m.id] = env->GetMethodID(g_class.get(), m.name, m.sig); \
        } \
        if (!g_method_ids[m.id]) { \
          if (m.type == 2 || m.type == 3) { \
            (void)::firebase::jni::CheckAndClearException(env); \
          } else { \
            return false; \
          } \
        } \
      } \
      return CacheFieldIds(env, activity); \
    } \
    inline bool CacheFieldIds(JNIEnv* env, jobject activity) { \
      if (!g_class) return false; \
      struct FieldData { int id; const char* name; const char* sig; int type; }; \
      static const FieldData kFields[] = { \
        FIELDS_MACRO(JNI_FIELD_DATA) \
        {-1, nullptr, nullptr, 0} \
      }; \
      for (const auto& f : kFields) { \
        if (f.id == -1) break; \
        if (f.type == 1 || f.type == 3) { \
          g_field_ids[f.id] = env->GetStaticFieldID(g_class.get(), f.name, f.sig); \
        } else { \
          g_field_ids[f.id] = env->GetFieldID(g_class.get(), f.name, f.sig); \
        } \
        if (!g_field_ids[f.id]) { \
          if (f.type == 2 || f.type == 3) { \
            (void)::firebase::jni::CheckAndClearException(env); \
          } else { \
            return false; \
          } \
        } \
      } \
      return true; \
    } \
    inline void ReleaseClass(JNIEnv* env) { \
      g_class.reset(env); \
    } \
    inline jclass CacheClassFromFiles(JNIEnv* env, jobject activity, const void* embedded_files) { \
      if (g_class) return g_class.get(); \
      jclass local_clazz = env->FindClass(CLASS_NAME); \
      if (!local_clazz) { \
        (void)::firebase::jni::CheckAndClearException(env); \
        return nullptr; \
      } \
      g_class = ::firebase::jni::Global<jclass>(env, local_clazz); \
      env->DeleteLocalRef(local_clazz); \
      return g_class.get(); \
    } \
    inline bool RegisterNatives(JNIEnv* env, const JNINativeMethod* methods, size_t num_methods) { \
      if (!g_class) return false; \
      if (env->RegisterNatives(g_class.get(), methods, num_methods) != 0) { \
        (void)::firebase::jni::CheckAndClearException(env); \
        return false; \
      } \
      return true; \
    } \
  }

#define JNI_METHOD_ENUM(Name, ...) k##Name,
#define JNI_FIELD_ENUM(Name, ...) k##Name,

#define JNI_METHOD_DATA_IMPL(Name, StringName, Signature, MethodType, ...) \
  {k##Name, StringName, Signature, MethodType},

#define JNI_METHOD_DATA_EXPAND(...) JNI_METHOD_DATA_IMPL(__VA_ARGS__)
#define JNI_METHOD_DATA(Name, StringName, Signature, ...) \
  JNI_METHOD_DATA_EXPAND(Name, StringName, Signature, ##__VA_ARGS__, 0, 0)

#define JNI_FIELD_DATA_IMPL(Name, StringName, Signature, FieldType, ...) \
  {k##Name, StringName, Signature, FieldType},

#define JNI_FIELD_DATA_EXPAND(...) JNI_FIELD_DATA_IMPL(__VA_ARGS__)
#define JNI_FIELD_DATA(Name, StringName, Signature, ...) \
  JNI_FIELD_DATA_EXPAND(Name, StringName, Signature, ##__VA_ARGS__, 0, 0)

#endif  // FIREBASE_APP_SRC_JNI_DECLARATION_H_
