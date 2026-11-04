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

#ifndef FIREBASE_APP_SRC_JNI_ENV_H_
#define FIREBASE_APP_SRC_JNI_ENV_H_

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/global.h"
#include "app/src/jni/local.h"
#include "app/src/jni/ownership.h"
#include "app/src/jni/string.h"
#include "app/src/jni/traits.h"

namespace firebase::jni {

class Class : public Object {
 public:
  using JniType = jclass;
  constexpr Class() noexcept = default;
  explicit constexpr Class(jclass clazz) noexcept : Object(clazz) {}
  constexpr jclass get() const noexcept {
    return static_cast<jclass>(object_);
  }
};

namespace internal {

template <typename T, typename = void>
struct ArrayJniType {
  using type = jobjectArray;
};
template <> struct ArrayJniType<bool> { using type = jbooleanArray; };
template <> struct ArrayJniType<int8_t> { using type = jbyteArray; };
template <> struct ArrayJniType<uint8_t> { using type = jbyteArray; };
template <> struct ArrayJniType<int16_t> { using type = jshortArray; };
template <> struct ArrayJniType<uint16_t> { using type = jcharArray; };
template <> struct ArrayJniType<char16_t> { using type = jcharArray; };
template <> struct ArrayJniType<int32_t> { using type = jintArray; };
template <> struct ArrayJniType<int64_t> { using type = jlongArray; };
template <> struct ArrayJniType<float> { using type = jfloatArray; };
template <> struct ArrayJniType<double> { using type = jdoubleArray; };

template <typename T>
struct ArrayElementJniType {
  using type = JniType<T>;
};
template <> struct ArrayElementJniType<bool> { using type = jboolean; };
template <> struct ArrayElementJniType<int8_t> { using type = jbyte; };
template <> struct ArrayElementJniType<uint8_t> { using type = jbyte; };

template <typename DeclaredParam, typename Arg>
inline auto ConvertDeclaredArg(Arg&& arg) {
  using TargetJniType = JniType<DeclaredParam>;
  if constexpr (IsPrimitive<TargetJniType>::value &&
                std::is_convertible_v<Arg, DeclaredParam>) {
    return ToJni(static_cast<DeclaredParam>(std::forward<Arg>(arg)));
  } else {
    auto jni_val = ToJni(std::forward<Arg>(arg));
    static_assert(std::is_convertible_v<decltype(jni_val), TargetJniType>,
                  "Argument is not convertible to declared JNI parameter type");
    return static_cast<TargetJniType>(jni_val);
  }
}

template <typename T>
struct CallTraits {};

#define FIREBASE_JNI_DEFINE_CALL_TRAITS(JType, Name)                      \
  template <>                                                             \
  struct CallTraits<JType> {                                              \
    using ArrayType = JType##Array;                                       \
    static constexpr auto kCall = &JNIEnv::Call##Name##Method;            \
    static constexpr auto kCallStatic = &JNIEnv::CallStatic##Name##Method;\
    static constexpr auto kGetStaticField =                               \
        &JNIEnv::GetStatic##Name##Field;                                  \
    static constexpr auto kNewArray = &JNIEnv::New##Name##Array;          \
    static constexpr auto kGetArrayRegion =                               \
        &JNIEnv::Get##Name##ArrayRegion;                                  \
    static constexpr auto kSetArrayRegion =                               \
        &JNIEnv::Set##Name##ArrayRegion;                                  \
  }

FIREBASE_JNI_DEFINE_CALL_TRAITS(jboolean, Boolean);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jbyte, Byte);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jchar, Char);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jshort, Short);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jint, Int);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jlong, Long);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jfloat, Float);
FIREBASE_JNI_DEFINE_CALL_TRAITS(jdouble, Double);
#undef FIREBASE_JNI_DEFINE_CALL_TRAITS

template <>
struct CallTraits<jobject> {
  using ArrayType = jobjectArray;
  static constexpr auto kCall = &JNIEnv::CallObjectMethod;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticObjectMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticObjectField;
};

template <>
struct CallTraits<void> {
  static constexpr auto kCall = &JNIEnv::CallVoidMethod;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticVoidMethod;
};

}  // namespace internal

template <typename T>
class Array : public Object {
 public:
  using JniType = typename internal::ArrayJniType<T>::type;
  constexpr Array() noexcept = default;
  explicit constexpr Array(JniType array) noexcept : Object(array) {}
  constexpr JniType get() const noexcept {
    return static_cast<JniType>(object_);
  }
  size_t Size(Env& env) const noexcept;
  template <typename U = T>
  EnableForReference<U, Local<U>> Get(Env& env, size_t i) const;
  template <typename U = T, typename V>
  EnableForReference<U, void> Set(Env& env, size_t i, const V& value);
};

class ScopedThreadAttachment {
 public:
  explicit ScopedThreadAttachment(JavaVM* jvm) noexcept : jvm_(jvm) {
    if (!jvm_) return;
    internal::GlobalRefReaper::SetJavaVM(jvm_);
    jint status = jvm_->GetEnv(internal::JniEnvOut(&env_), JNI_VERSION_1_6);
    if (status == JNI_OK) {
      internal::GlobalRefReaper::Flush(env_);
    } else if (status == JNI_EDETACHED &&
               jvm_->AttachCurrentThread(internal::JniEnvOut(&env_), nullptr) ==
                   JNI_OK &&
               env_ != nullptr) {
      attached_by_us_ = true;
      internal::GlobalRefReaper::Flush(env_);
    } else {
      env_ = nullptr;
    }
  }

  ~ScopedThreadAttachment() noexcept {
    if (attached_by_us_ && jvm_ && env_) {
      if (env_->ExceptionCheck()) env_->ExceptionClear();
      internal::GlobalRefReaper::Flush(env_);
      JNIEnv* check_env = nullptr;
      if (jvm_->GetEnv(internal::JniEnvOut(&check_env), JNI_VERSION_1_6) ==
          JNI_OK) {
        jvm_->DetachCurrentThread();
      }
    }
  }

  ScopedThreadAttachment(const ScopedThreadAttachment&) = delete;
  ScopedThreadAttachment& operator=(const ScopedThreadAttachment&) = delete;

  JNIEnv* env() const noexcept { return env_; }
  bool attached_by_us() const noexcept { return attached_by_us_; }
  explicit operator bool() const noexcept { return env_ != nullptr; }

 private:
  JavaVM* jvm_ = nullptr;
  JNIEnv* env_ = nullptr;
  bool attached_by_us_ = false;
};

class Env {
 public:
  using UnhandledExceptionHandler = void (*)(Env& env,
                                             const Local<Throwable>& exception);

  explicit Env(
      JNIEnv* env,
      ExceptionPolicy policy = ExceptionPolicy::kCaptureAndClear) noexcept
      : env_(env),
        initial_pending_exceptions_(std::uncaught_exceptions()),
        policy_(policy) {
    if (env_) {
      internal::GlobalRefReaper::RecordJavaVM(env_);
      internal::GlobalRefReaper::Flush(env_);
    }
  }

  Env(JNIEnv* env, UnhandledExceptionHandler exception_handler) noexcept
      : env_(env),
        initial_pending_exceptions_(std::uncaught_exceptions()),
        policy_(ExceptionPolicy::kPreservePending),
        exception_handler_(exception_handler) {
    if (env_) {
      internal::GlobalRefReaper::RecordJavaVM(env_);
      internal::GlobalRefReaper::Flush(env_);
    }
  }

  ~Env() noexcept(false) { FinalizePendingExceptions(); }
  Env(const Env&) = delete;
  Env& operator=(const Env&) = delete;

  Env(Env&& other) noexcept
      : env_(other.env_),
        initial_pending_exceptions_(other.initial_pending_exceptions_),
        policy_(other.policy_),
        exception_handler_(other.exception_handler_),
        exceptions_(other.exceptions_),
        saved_exception_(std::move(other.saved_exception_)) {
    other.env_ = nullptr;
    other.exception_handler_ = nullptr;
    other.exceptions_ = 0;
  }

  Env& operator=(Env&& other) noexcept(false) {
    if (this != &other) {
      FinalizePendingExceptions();
      env_ = other.env_;
      initial_pending_exceptions_ = other.initial_pending_exceptions_;
      policy_ = other.policy_;
      exception_handler_ = other.exception_handler_;
      exceptions_ = other.exceptions_;
      saved_exception_ = std::move(other.saved_exception_);
      other.env_ = nullptr;
      other.exception_handler_ = nullptr;
      other.exceptions_ = 0;
    }
    return *this;
  }

  JNIEnv* get() const noexcept { return env_; }
  bool ok() const noexcept { return env_ != nullptr && !ExceptionCheck(); }
  size_t exceptions() const noexcept { return exceptions_; }
  ExceptionPolicy policy() const noexcept { return policy_; }
  void set_policy(ExceptionPolicy policy) noexcept { policy_ = policy; }

  bool ExceptionCheck() const noexcept {
    return env_ != nullptr &&
           (env_->ExceptionCheck() == JNI_TRUE || saved_exception_);
  }

  void ExceptionClear() noexcept {
    if (env_) env_->ExceptionClear();
    saved_exception_ = {};
  }

  Local<Throwable> ExceptionOccurred() noexcept {
    if (saved_exception_) return Local<Throwable>(saved_exception_);
    if (!env_ || !env_->ExceptionCheck()) return {};
    return Local<Throwable>(env_, env_->ExceptionOccurred(), true);
  }

  Local<Throwable> ClearExceptionOccurred() noexcept {
    Local<Throwable> occurred = ExceptionOccurred();
    if (occurred) ExceptionClear();
    return occurred;
  }

  Local<Class> FindClass(const char* name) {
    return (!ok() || !name) ? Local<Class>{}
                            : MakeResult<Class>(env_->FindClass(name));
  }

  Local<Class> GetObjectClass(const Object& object) {
    return (!ok() || !object)
               ? Local<Class>{}
               : MakeResult<Class>(env_->GetObjectClass(object.get()));
  }

  bool IsInstanceOf(const Object& object, const Class& clazz) {
    return IsInstanceOf(object, clazz.get());
  }
  bool IsInstanceOf(const Object& object, jclass clazz) {
    return ok() && object && clazz &&
           env_->IsInstanceOf(object.get(), clazz) == JNI_TRUE;
  }
  bool IsSameObject(const Object& object1, const Object& object2) {
    return ok() &&
           env_->IsSameObject(object1.get(), object2.get()) == JNI_TRUE;
  }

  jmethodID GetMethodId(const Class& clazz, const char* name, const char* sig) {
    return (!ok() || !clazz || !name || !sig)
               ? nullptr
               : MakeResult<jmethodID>(
                     env_->GetMethodID(clazz.get(), name, sig));
  }
  jfieldID GetStaticFieldId(const Class& clazz, const char* name,
                            const char* sig) {
    return (!ok() || !clazz || !name || !sig)
               ? nullptr
               : MakeResult<jfieldID>(
                     env_->GetStaticFieldID(clazz.get(), name, sig));
  }
  jmethodID GetStaticMethodId(const Class& clazz, const char* name,
                              const char* sig) {
    return (!ok() || !clazz || !name || !sig)
               ? nullptr
               : MakeResult<jmethodID>(
                     env_->GetStaticMethodID(clazz.get(), name, sig));
  }

  void Throw(const Throwable& throwable) {
    if (ok() && throwable &&
        (env_->Throw(throwable.get()) == JNI_OK || !ok())) {
      RecordException();
    }
  }

  void ThrowNew(const Class& clazz, const char* message) {
    if (ok() && clazz &&
        (env_->ThrowNew(clazz.get(), message ? message : "") == JNI_OK ||
         !ok())) {
      RecordException();
    }
  }

  Local<String> NewStringUtf(const char* bytes) {
    return (!ok() || !bytes) ? Local<String>{}
                             : MakeResult<String>(NewRawJString(env_, bytes));
  }
  Local<String> NewStringUtf(const char* bytes, size_t size) {
    if (!ok() || (!bytes && size > 0)) return {};
    return MakeResult<String>(NewRawJString(
        env_, std::string_view(bytes ? bytes : "", bytes ? size : 0)));
  }
  Local<String> NewStringUtf(std::string_view utf8) {
    return !ok() ? Local<String>{}
                 : MakeResult<String>(NewRawJString(env_, utf8));
  }

  std::string ToStringUtf(const String& string) {
    return ToStringUtf(string.get());
  }
  std::string ToStringUtf(jstring string) {
    if (!ok() || !string) return "";
    std::string result = JStringToString(env_, string);
    if (!ok()) {
      RecordException();
      return "";
    }
    return result;
  }

  template <typename T = Object, typename... Args>
  Local<T> New(const Class& clazz, jmethodID method, Args&&... args) {
    return New<T>(clazz.get(), method, std::forward<Args>(args)...);
  }
  template <typename T = Object, typename... Args>
  Local<T> New(jclass clazz, jmethodID method, Args&&... args) {
    if (!ok() || !clazz || !method) return {};
    return MakeResult<T>(
        env_->NewObject(clazz, method, ToJni(std::forward<Args>(args))...));
  }
  template <typename T, typename... Args,
            typename = std::enable_if_t<!Constructor<T>::kHasTypedArgs>>
  Local<T> New(const Constructor<T>& ctor, Args&&... args) {
    auto s = ctor.Get();
    return New<T>(s.clazz, s.id, std::forward<Args>(args)...);
  }
  template <typename T, typename... ParamTypes, typename... Args>
  Local<T> New(const Constructor<T(ParamTypes...)>& ctor, Args&&... args) {
    static_assert(sizeof...(ParamTypes) == sizeof...(Args));
    auto s = ctor.Get();
    if (!ok() || !s.clazz || !s.id) return {};
    return MakeResult<T>(env_->NewObject(
        s.clazz, s.id,
        internal::ConvertDeclaredArg<ParamTypes>(std::forward<Args>(args))...));
  }

  template <typename R, typename... Args>
  auto Call(const Object& object, jmethodID method, Args&&... args) {
    return CallHelper<R>(internal::CallTraits<CallType<R>>::kCall, object.get(),
                         method, ToJni(std::forward<Args>(args))...);
  }
  template <typename R, typename... Args,
            typename = std::enable_if_t<!Method<R>::kHasTypedArgs>>
  auto Call(const Object& object, const Method<R>& method, Args&&... args) {
    return CallHelper<R>(internal::CallTraits<CallType<R>>::kCall, object.get(),
                         method.id(), ToJni(std::forward<Args>(args))...);
  }
  template <typename R, typename... ParamTypes, typename... Args>
  auto Call(const Object& object, const Method<R(ParamTypes...)>& method,
            Args&&... args) {
    static_assert(sizeof...(ParamTypes) == sizeof...(Args));
    return CallHelper<R>(
        internal::CallTraits<CallType<R>>::kCall, object.get(), method.id(),
        internal::ConvertDeclaredArg<ParamTypes>(std::forward<Args>(args))...);
  }

  template <typename R, typename... Args>
  auto CallStatic(const Class& clazz, jmethodID method, Args&&... args) {
    return CallStatic<R>(clazz.get(), method, std::forward<Args>(args)...);
  }
  template <typename R, typename... Args>
  auto CallStatic(jclass clazz, jmethodID method, Args&&... args) {
    return CallHelper<R>(internal::CallTraits<CallType<R>>::kCallStatic, clazz,
                         method, ToJni(std::forward<Args>(args))...);
  }
  template <typename R, typename... Args,
            typename = std::enable_if_t<!StaticMethod<R>::kHasTypedArgs>>
  auto Call(const StaticMethod<R>& method, Args&&... args) {
    auto s = method.Get();
    return CallHelper<R>(internal::CallTraits<CallType<R>>::kCallStatic,
                         s.clazz, s.id, ToJni(std::forward<Args>(args))...);
  }
  template <typename R, typename... ParamTypes, typename... Args>
  auto Call(const StaticMethod<R(ParamTypes...)>& method, Args&&... args) {
    static_assert(sizeof...(ParamTypes) == sizeof...(Args));
    auto s = method.Get();
    return CallHelper<R>(
        internal::CallTraits<CallType<R>>::kCallStatic, s.clazz, s.id,
        internal::ConvertDeclaredArg<ParamTypes>(std::forward<Args>(args))...);
  }

  template <typename R>
  auto GetStaticField(const Class& clazz, jfieldID field) {
    return GetStaticField<R>(clazz.get(), field);
  }
  template <typename R>
  auto GetStaticField(jclass clazz, jfieldID field) {
    using Ret = std::conditional_t<IsReference<JniType<R>>::value, Local<R>, R>;
    if (!ok() || !clazz || !field) return Ret{};
    constexpr auto kGetField =
        internal::CallTraits<CallType<R>>::kGetStaticField;
    return MakeResult<R>((env_->*kGetField)(clazz, field));
  }
  template <typename T>
  auto Get(const StaticField<T>& field) {
    auto s = field.Get();
    return GetStaticField<T>(s.clazz, s.id);
  }

  template <typename T>
  size_t GetArrayLength(const Array<T>& array) noexcept {
    if (!ok() || !array) return 0;
    jsize len = env_->GetArrayLength(array.get());
    return len > 0 ? static_cast<size_t>(len) : 0;
  }

  template <typename T>
  EnableForReference<T, Local<Array<T>>> NewArray(size_t size,
                                                  jclass element_class) {
    if (!ok() || !element_class || size > std::numeric_limits<jsize>::max())
      return {};
    return MakeResult<Array<T>>(
        env_->NewObjectArray(ToJni(size), element_class, nullptr));
  }
  template <typename T>
  EnableForReference<T, Local<Array<T>>> NewArray(size_t size,
                                                  const Class& element_class) {
    return NewArray<T>(size, element_class.get());
  }
  template <typename T>
  EnableForPrimitive<T, Local<Array<T>>> NewArray(size_t size) {
    if (!ok() || size > std::numeric_limits<jsize>::max()) return {};
    using ElementJniType = typename internal::ArrayElementJniType<T>::type;
    constexpr auto kNewArray = internal::CallTraits<ElementJniType>::kNewArray;
    return MakeResult<Array<T>>((env_->*kNewArray)(ToJni(size)));
  }

  template <typename T>
  EnableForReference<T, Local<T>> GetArrayElement(const Array<T>& array,
                                                  size_t index) {
    if (!ok() || !array || index > std::numeric_limits<jsize>::max()) return {};
    return MakeResult<T>(env_->GetObjectArrayElement(array.get(), ToJni(index)));
  }

  template <typename T, typename U>
  EnableForReference<T, void> SetArrayElement(const Array<T>& array,
                                              size_t index, const U& value) {
    if (!ok() || !array || index > std::numeric_limits<jsize>::max()) return;
    env_->SetObjectArrayElement(array.get(), ToJni(index), ToJni(value));
    if (!ok()) RecordException();
  }

  template <typename T>
  EnableForPrimitive<T, void> GetArrayRegion(const Array<T>& array,
                                             size_t start, size_t len,
                                             T* buffer) {
    constexpr size_t kMax = std::numeric_limits<jsize>::max();
    if (!ok() || !array || len == 0 || !buffer || start > kMax || len > kMax ||
        start + len > kMax) {
      return;
    }
    if constexpr (std::is_same_v<T, bool>) {
      std::vector<jboolean> raw(len, JNI_FALSE);
      env_->GetBooleanArrayRegion(ToJni(array), ToJni(start), ToJni(len),
                                  raw.data());
      if (!ok()) {
        RecordException();
        return;
      }
      for (size_t i = 0; i < len; ++i) buffer[i] = (raw[i] != JNI_FALSE);
    } else {
      using ElementJniType = typename internal::ArrayElementJniType<T>::type;
      static_assert(sizeof(T) == sizeof(ElementJniType));
      constexpr auto kGetRegion =
          internal::CallTraits<ElementJniType>::kGetArrayRegion;
      (env_->*kGetRegion)(ToJni(array), ToJni(start), ToJni(len),
                          reinterpret_cast<ElementJniType*>(buffer));
      if (!ok()) RecordException();
    }
  }

  template <typename T>
  EnableForPrimitive<T, std::vector<T>> GetArrayRegion(const Array<T>& array,
                                                       size_t start,
                                                       size_t len) {
    constexpr size_t kMax = std::numeric_limits<jsize>::max();
    if (!ok() || !array || len == 0 || start > kMax || len > kMax ||
        start + len > kMax) {
      return {};
    }
    if constexpr (std::is_same_v<T, bool>) {
      std::vector<jboolean> raw(len, JNI_FALSE);
      env_->GetBooleanArrayRegion(ToJni(array), ToJni(start), ToJni(len),
                                  raw.data());
      if (!ok()) {
        RecordException();
        return {};
      }
      std::vector<bool> result(len, false);
      for (size_t i = 0; i < len; ++i) result[i] = (raw[i] != JNI_FALSE);
      return result;
    } else {
      std::vector<T> result(len);
      GetArrayRegion(array, start, len, result.data());
      return ok() ? result : std::vector<T>{};
    }
  }

  template <typename T>
  EnableForPrimitive<T, void> SetArrayRegion(Array<T>& array, size_t start,
                                             size_t len, const T* buffer) {
    constexpr size_t kMax = std::numeric_limits<jsize>::max();
    if (!ok() || !array || len == 0 || !buffer || start > kMax || len > kMax ||
        start + len > kMax) {
      return;
    }
    if constexpr (std::is_same_v<T, bool>) {
      std::vector<jboolean> raw(len, JNI_FALSE);
      for (size_t i = 0; i < len; ++i)
        raw[i] = buffer[i] ? JNI_TRUE : JNI_FALSE;
      env_->SetBooleanArrayRegion(ToJni(array), ToJni(start), ToJni(len),
                                  raw.data());
      if (!ok()) RecordException();
    } else {
      using ElementJniType = typename internal::ArrayElementJniType<T>::type;
      static_assert(sizeof(T) == sizeof(ElementJniType));
      constexpr auto kSetRegion =
          internal::CallTraits<ElementJniType>::kSetArrayRegion;
      (env_->*kSetRegion)(ToJni(array), ToJni(start), ToJni(len),
                          reinterpret_cast<const ElementJniType*>(buffer));
      if (!ok()) RecordException();
    }
  }

 private:
  template <typename R>
  using CallType =
      std::conditional_t<IsReference<JniType<R>>::value, jobject, JniType<R>>;

  void FinalizePendingExceptions() noexcept(false) {
    if (!env_ || !ExceptionCheck()) return;
    if (exception_handler_ != nullptr) {
      if (std::uncaught_exceptions() == initial_pending_exceptions_) {
        Local<Throwable> exception = ClearExceptionOccurred();
        (*exception_handler_)(*this, exception);
        if (policy_ == ExceptionPolicy::kCaptureAndClear && ExceptionCheck()) {
          ExceptionClear();
        }
      } else {
        ExceptionClear();
      }
    } else if (policy_ == ExceptionPolicy::kCaptureAndClear) {
      ExceptionClear();
    }
  }

  template <typename R, typename MethodPtr, typename Target, typename... JniArgs>
  std::enable_if_t<std::is_void_v<R>, void> CallHelper(MethodPtr method,
                                                       Target target,
                                                       jmethodID id,
                                                       JniArgs... args) {
    if (!ok() || !target || !id) return;
    (env_->*method)(target, id, args...);
    if (!ok()) RecordException();
  }

  template <typename R, typename MethodPtr, typename Target, typename... JniArgs>
  std::enable_if_t<!std::is_void_v<R>,
                   std::conditional_t<IsReference<JniType<R>>::value, Local<R>, R>>
  CallHelper(MethodPtr method, Target target, jmethodID id, JniArgs... args) {
    if (!ok() || !target || !id) return {};
    return MakeResult<R>((env_->*method)(target, id, args...));
  }

  template <typename R>
  std::enable_if_t<!IsReference<JniType<R>>::value, R> MakeResult(
      JniType<R> result) {
    if (ok()) return R(result);
    RecordException();
    return R{};
  }

  template <typename R>
  EnableForReference<R, Local<R>> MakeResult(jobject result) {
    if (ok()) return Local<R>(env_, static_cast<JniType<R>>(result), true);
    RecordException();
    if (result) env_->DeleteLocalRef(result);
    return {};
  }

  void RecordException() noexcept {
    ++exceptions_;
    if (policy_ == ExceptionPolicy::kCaptureAndClear && env_ &&
        env_->ExceptionCheck()) {
      jthrowable ex = env_->ExceptionOccurred();
      env_->ExceptionClear();
      if (!saved_exception_) {
        saved_exception_ = Local<Throwable>(env_, ex, true);
      } else if (ex) {
        env_->DeleteLocalRef(ex);
      }
    }
  }

  JNIEnv* env_ = nullptr;
  int initial_pending_exceptions_ = 0;
  ExceptionPolicy policy_ = ExceptionPolicy::kCaptureAndClear;
  UnhandledExceptionHandler exception_handler_ = nullptr;
  size_t exceptions_ = 0;
  Local<Throwable> saved_exception_;
};

using JniEnv = Env;

inline ExceptionClearGuard::ExceptionClearGuard(Env& env) noexcept
    : ExceptionClearGuard(env.get()) {}
inline std::string String::ToString(Env& env) const {
  return env.ToStringUtf(*this);
}
inline std::string Throwable::GetMessage(Env& env) const {
  return (!env.get() || !*this) ? "" : ExtractExceptionMessage(env.get(), get());
}

template <typename T>
inline size_t Array<T>::Size(Env& env) const noexcept {
  return env.GetArrayLength(*this);
}
template <typename T>
template <typename U>
inline EnableForReference<U, Local<U>> Array<T>::Get(Env& env, size_t i) const {
  return env.GetArrayElement(*this, i);
}
template <typename T>
template <typename U, typename V>
inline EnableForReference<U, void> Array<T>::Set(Env& env, size_t i,
                                                 const V& value) {
  env.SetArrayElement(*this, i, value);
}

}  // namespace firebase::jni

namespace firebase::util {
JNIEnv* GetThreadsafeJNIEnv(JavaVM* java_vm);
}  // namespace firebase::util

#endif  // FIREBASE_APP_SRC_JNI_ENV_H_
