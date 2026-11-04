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
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/ownership.h"

namespace firebase {
namespace util {

#ifndef FIREBASE_UTIL_CLASS_REQUIREMENT_DEFINED
#define FIREBASE_UTIL_CLASS_REQUIREMENT_DEFINED
enum ClassRequirement {
  kClassRequired,
  kClassOptional,
};
#endif  // FIREBASE_UTIL_CLASS_REQUIREMENT_DEFINED

jclass FindClassGlobal(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>* embedded_files,
    const char* class_name, ClassRequirement optional);

const std::vector<internal::EmbeddedFile>& CacheEmbeddedFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files);

}  // namespace util

namespace jni {

inline const std::vector<::firebase::internal::EmbeddedFile>&
CacheEmbeddedFiles(
    JNIEnv* env, jobject activity,
    const std::vector<::firebase::internal::EmbeddedFile>& files) {
  return ::firebase::util::CacheEmbeddedFiles(env, activity, files);
}

constexpr const char* SkipProguardPrefix(const char* class_name) noexcept {
  return (class_name && class_name[0] == '%' && class_name[1] == 'P' &&
          class_name[2] == 'G' && class_name[3] == '%')
             ? (class_name + 4)
             : class_name;
}

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

namespace internal {

template <typename IdType>
class MemberBase {
 public:
  constexpr MemberBase(const char* name, const char* sig) noexcept
      : name_(name ? name : ""), sig_(sig ? sig : "") {}
  explicit constexpr MemberBase(IdType id) noexcept
      : name_(""), sig_(""), id_(id) {}
  constexpr const char* name() const noexcept { return name_; }
  constexpr const char* sig() const noexcept { return sig_; }
  IdType id() const noexcept { return id_.load(std::memory_order_acquire); }
  explicit operator bool() const noexcept { return id() != nullptr; }
  void Set(IdType id) noexcept { id_.store(id, std::memory_order_release); }
  void Reset() noexcept { id_.store(nullptr, std::memory_order_release); }

 private:
  const char* name_ = "";
  const char* sig_ = "";
  std::atomic<IdType> id_{nullptr};
};

template <typename IdType>
class ClassMemberBase {
 protected:
  struct State {
    jclass clazz;
    IdType id;
  };

 public:
  constexpr ClassMemberBase(const char* name, const char* sig) noexcept
      : name_(name ? name : ""), sig_(sig ? sig : "") {}
  explicit constexpr ClassMemberBase(jclass global_clazz, IdType id) noexcept
      : name_(""), sig_(""), clazz_(global_clazz), id_(id) {}
  constexpr const char* name() const noexcept { return name_; }
  constexpr const char* sig() const noexcept { return sig_; }

  State Get() const noexcept {
    IdType id = id_.load(std::memory_order_acquire);
    jclass clazz = clazz_.load(std::memory_order_acquire);
    return State{clazz, id};
  }
  explicit operator bool() const noexcept {
    State s = Get();
    return s.clazz != nullptr && s.id != nullptr;
  }
  void Set(jclass global_clazz, IdType id) noexcept {
    clazz_.store(global_clazz, std::memory_order_release);
    id_.store(id, std::memory_order_release);
  }
  void Set(JNIEnv*, jclass global_clazz, IdType id) noexcept {
    Set(global_clazz, id);
  }
  void Reset(JNIEnv* = nullptr) noexcept {
    id_.store(nullptr, std::memory_order_release);
    clazz_.store(nullptr, std::memory_order_release);
  }

 private:
  const char* name_ = "";
  const char* sig_ = "";
  std::atomic<jclass> clazz_{nullptr};
  std::atomic<IdType> id_{nullptr};
};

}  // namespace internal

class ConstructorBase : public internal::ClassMemberBase<jmethodID> {
 public:
  constexpr explicit ConstructorBase(const char* sig) noexcept
      : ClassMemberBase("<init>", sig) {}
  explicit constexpr ConstructorBase(jclass global_clazz,
                                     jmethodID id) noexcept
      : ClassMemberBase(global_clazz, id) {}
  constexpr const char* name() const noexcept { return "<init>"; }
};

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

using MethodBase = internal::MemberBase<jmethodID>;

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

using FieldBase = internal::MemberBase<jfieldID>;

template <typename T>
class Field : public FieldBase {
 public:
  using ResultType = T;
  using FieldBase::FieldBase;
};

using StaticFieldBase = internal::ClassMemberBase<jfieldID>;

template <typename T>
class StaticField : public StaticFieldBase {
 public:
  using ResultType = T;
  using StaticFieldBase::StaticFieldBase;
};

using StaticMethodBase = internal::ClassMemberBase<jmethodID>;

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

#ifndef METHOD_LOOKUP_NONE
#define METHOD_LOOKUP_NONE(X)
#endif
#ifndef FIELD_LOOKUP_NONE
#define FIELD_LOOKUP_NONE(X)
#endif

#define JNI_METHOD_LOOKUP_DECLARATION(NAMESPACE, METHODS_MACRO, FIELDS_MACRO, \
                                      CLASS_NAME)                             \
  namespace NAMESPACE {                                                       \
  enum Method { METHODS_MACRO(JNI_METHOD_ENUM) kMethodCount };                \
  enum Field { FIELDS_MACRO(JNI_FIELD_ENUM) kFieldCount };                    \
  inline ::firebase::Mutex g_mutex;                                           \
  inline jclass g_class = nullptr;                                            \
  inline bool g_methods_cached = false;                                       \
  inline bool g_registered_natives = false;                                   \
  inline jmethodID g_method_ids[kMethodCount > 0 ? kMethodCount : 1];         \
  inline jfieldID g_field_ids[kFieldCount > 0 ? kFieldCount : 1];             \
  inline jclass GetClass() {                                                  \
    ::firebase::MutexLock lock(g_mutex);                                      \
    return g_class;                                                           \
  }                                                                           \
  inline jmethodID GetMethodId(Method method) { return g_method_ids[method]; }\
  inline jfieldID GetFieldId(Field field) { return g_field_ids[field]; }      \
  inline void ReleaseClassLocked(JNIEnv* env) {                               \
    for (size_t i = 0; i < static_cast<size_t>(kMethodCount); ++i)            \
      g_method_ids[i] = nullptr;                                              \
    for (size_t i = 0; i < static_cast<size_t>(kFieldCount); ++i)             \
      g_field_ids[i] = nullptr;                                               \
    if (g_class) {                                                            \
      if (env && g_registered_natives) {                                      \
        (void)::firebase::jni::CheckAndClearException(env);                   \
        env->UnregisterNatives(g_class);                                      \
        (void)::firebase::jni::CheckAndClearException(env);                   \
      }                                                                       \
      ::firebase::jni::internal::GlobalRefReaper::DeleteOrEnqueue(g_class,    \
                                                                  env);       \
      g_class = nullptr;                                                      \
    }                                                                         \
    g_methods_cached = false;                                                 \
    g_registered_natives = false;                                             \
  }                                                                           \
  inline bool CacheFieldIdsLocked(JNIEnv* env) {                              \
    if (!g_class) return false;                                               \
    struct FieldData {                                                        \
      int id;                                                                 \
      const char* name;                                                       \
      const char* sig;                                                        \
      int type;                                                               \
      int optional;                                                           \
    };                                                                        \
    static const FieldData kFields[] = {FIELDS_MACRO(JNI_FIELD_DATA){         \
        -1, nullptr, nullptr, 0, 0}};                                         \
    for (const auto& f : kFields) {                                           \
      if (f.id == -1) break;                                                  \
      if (!f.name || !f.sig) continue;                                        \
      g_field_ids[f.id] =                                                     \
          (f.type == 1 || f.type == 3)                                        \
              ? env->GetStaticFieldID(g_class, f.name, f.sig)                 \
              : env->GetFieldID(g_class, f.name, f.sig);                      \
      if (::firebase::jni::CheckAndClearException(env) ||                     \
          !g_field_ids[f.id]) {                                               \
        g_field_ids[f.id] = nullptr;                                          \
        if (!(f.type == 2 || f.type == 3 || f.optional != 0)) return false;   \
      }                                                                       \
    }                                                                         \
    return true;                                                              \
  }                                                                           \
  inline bool CacheFieldIds(JNIEnv* env, jobject activity = nullptr) {        \
    ::firebase::MutexLock lock(g_mutex);                                      \
    if (!g_class) {                                                           \
      g_class = ::firebase::util::FindClassGlobal(                            \
          env, activity, nullptr,                                             \
          ::firebase::jni::SkipProguardPrefix(CLASS_NAME),                    \
          ::firebase::util::kClassRequired);                                  \
      if (!g_class) return false;                                             \
    }                                                                         \
    if (!CacheFieldIdsLocked(env)) {                                          \
      ReleaseClassLocked(env);                                                \
      return false;                                                           \
    }                                                                         \
    return true;                                                              \
  }                                                                           \
  inline bool CacheMethodIds(JNIEnv* env, jobject activity = nullptr) {       \
    ::firebase::MutexLock lock(g_mutex);                                      \
    if (g_methods_cached && g_class) return true;                             \
    if (!g_class) {                                                           \
      g_class = ::firebase::util::FindClassGlobal(                            \
          env, activity, nullptr,                                             \
          ::firebase::jni::SkipProguardPrefix(CLASS_NAME),                    \
          ::firebase::util::kClassRequired);                                  \
      if (!g_class) return false;                                             \
    }                                                                         \
    struct MethodData {                                                       \
      int id;                                                                 \
      const char* name;                                                       \
      const char* sig;                                                        \
      int type;                                                               \
      int optional;                                                           \
    };                                                                        \
    static const MethodData kMethods[] = {METHODS_MACRO(JNI_METHOD_DATA){     \
        -1, nullptr, nullptr, 0, 0}};                                         \
    for (const auto& m : kMethods) {                                          \
      if (m.id == -1) break;                                                  \
      if (!m.name || !m.sig) continue;                                        \
      g_method_ids[m.id] =                                                    \
          (m.type == 1 || m.type == 3)                                        \
              ? env->GetStaticMethodID(g_class, m.name, m.sig)                \
              : env->GetMethodID(g_class, m.name, m.sig);                     \
      if (::firebase::jni::CheckAndClearException(env) ||                     \
          !g_method_ids[m.id]) {                                              \
        g_method_ids[m.id] = nullptr;                                         \
        if (!(m.type == 2 || m.type == 3 || m.optional != 0)) {               \
          ReleaseClassLocked(env);                                            \
          return false;                                                       \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    if (!CacheFieldIdsLocked(env)) {                                          \
      ReleaseClassLocked(env);                                                \
      return false;                                                           \
    }                                                                         \
    g_methods_cached = true;                                                  \
    return true;                                                              \
  }                                                                           \
  inline void ReleaseClass(JNIEnv* env) {                                     \
    ::firebase::MutexLock lock(g_mutex);                                      \
    ReleaseClassLocked(env);                                                  \
  }                                                                           \
  inline jclass CacheClassFromFiles(                                          \
      JNIEnv* env, jobject activity = nullptr,                                \
      const std::vector<::firebase::internal::EmbeddedFile>* embedded_files = \
          nullptr,                                                            \
      ::firebase::util::ClassRequirement optional =                           \
          ::firebase::util::kClassRequired) {                                 \
    ::firebase::MutexLock lock(g_mutex);                                      \
    if (!g_class) {                                                           \
      g_class = ::firebase::util::FindClassGlobal(                            \
          env, activity, embedded_files,                                      \
          ::firebase::jni::SkipProguardPrefix(CLASS_NAME), optional);         \
    }                                                                         \
    return g_class;                                                           \
  }                                                                           \
  inline bool RegisterNatives(JNIEnv* env, const JNINativeMethod* methods,    \
                              size_t num_methods) {                           \
    ::firebase::MutexLock lock(g_mutex);                                      \
    if (g_registered_natives || !env || !g_class || !methods ||               \
        num_methods == 0)                                                     \
      return false;                                                           \
    (void)::firebase::jni::CheckAndClearException(env);                       \
    const jint status = env->RegisterNatives(                                 \
        g_class, methods, static_cast<jint>(num_methods));                    \
    const bool had_exception =                                                \
        ::firebase::jni::CheckAndClearException(env);                         \
    g_registered_natives = (status == JNI_OK && !had_exception);              \
    return g_registered_natives;                                              \
  }                                                                           \
  }

#define JNI_METHOD_ENUM(Name, ...) k##Name,
#define JNI_FIELD_ENUM(Name, ...) k##Name,

#define JNI_METHOD_DATA_IMPL(Name, StringName, Signature, MethodType, \
                             Optional, ...)                           \
  {k##Name, StringName, Signature, static_cast<int>(MethodType),      \
   static_cast<int>(Optional)},
#define JNI_METHOD_DATA_EXPAND(...) JNI_METHOD_DATA_IMPL(__VA_ARGS__)
#define JNI_METHOD_DATA(Name, StringName, Signature, ...) \
  JNI_METHOD_DATA_EXPAND(Name, StringName, Signature, ##__VA_ARGS__, 0, 0, 0)

#define JNI_FIELD_DATA_IMPL(Name, StringName, Signature, FieldType, Optional, \
                            ...)                                              \
  {k##Name, StringName, Signature, static_cast<int>(FieldType),               \
   static_cast<int>(Optional)},
#define JNI_FIELD_DATA_EXPAND(...) JNI_FIELD_DATA_IMPL(__VA_ARGS__)
#define JNI_FIELD_DATA(Name, StringName, Signature, ...) \
  JNI_FIELD_DATA_EXPAND(Name, StringName, Signature, ##__VA_ARGS__, 0, 0, 0)

#endif  // FIREBASE_APP_SRC_JNI_DECLARATION_H_
