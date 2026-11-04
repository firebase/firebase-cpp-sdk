// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ENV_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ENV_H_

#include <jni.h>

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/call_traits.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/jni/throwable.h"
#include "firestore/src/jni/traits.h"

namespace firebase::firestore::jni {

#define INVOKE(env, method, ...) ((env)->*(method))(__VA_ARGS__)

class Env {
 public:
  using UnhandledExceptionHandler = void (*)(
      jni::Env& env, jni::Local<jni::Throwable>&& exception, void* context);

  Env();
  explicit Env(JNIEnv* env);
  ~Env() noexcept(false);

  Env(const Env&) = delete;
  Env& operator=(const Env&) = delete;

  Env(Env&& other) noexcept
      : env_(other.env_),
        exception_handler_(std::exchange(other.exception_handler_, nullptr)),
        context_(std::exchange(other.context_, nullptr)),
        initial_pending_exceptions_(other.initial_pending_exceptions_) {}

  Env& operator=(Env&& other) noexcept {
    if (this != &other) {
      env_ = other.env_;
      exception_handler_ = std::exchange(other.exception_handler_, nullptr);
      context_ = std::exchange(other.context_, nullptr);
      initial_pending_exceptions_ = other.initial_pending_exceptions_;
    }
    return *this;
  }

  bool ok() const { return env_ != nullptr && !env_->ExceptionCheck(); }
  JNIEnv* get() const { return env_; }

  Local<Class> FindClass(const char* name);
  void Throw(const Throwable& throwable);
  void ThrowNew(const Class& clazz, const char* message);
  void ThrowNew(const Class& clazz, const std::string& message) {
    ThrowNew(clazz, message.c_str());
  }

  Local<Throwable> ExceptionOccurred();
  void ExceptionClear();
  Local<Throwable> ClearExceptionOccurred();

  void SetUnhandledExceptionHandler(UnhandledExceptionHandler handler,
                                    void* context) {
    exception_handler_ = handler;
    context_ = context;
  }

  template <typename T = Object, typename... Args>
  Local<T> New(const Class& clazz, jmethodID method, Args&&... args) {
    if (!ok() || !clazz || !method) return {};
    return MakeResult<T>(env_->NewObject(clazz.get(), method,
                                         ToJni(std::forward<Args>(args))...));
  }

  template <typename T, typename... Args>
  Local<T> New(const Constructor<T>& ctor, Args&&... args) {
    if (!ok() || !ctor.clazz() || !ctor.id()) return {};
    return MakeResult<T>(env_->NewObject(ctor.clazz(), ctor.id(),
                                         ToJni(std::forward<Args>(args))...));
  }

  Local<Class> GetObjectClass(const Object& object);
  bool IsInstanceOf(const Object& object, const Class& clazz);
  bool IsInstanceOf(const Object& object, jclass clazz) {
    return IsInstanceOf(object, Class(clazz));
  }
  bool IsSameObject(const Object& object1, const Object& object2);

  jmethodID GetMethodId(const Class& clazz, const char* name, const char* sig);

  template <typename T, typename... Args>
  ResultType<T> Call(const Object& object, jmethodID method, Args&&... args) {
    return CallHelper<T>(CallTraits<JniType<T>>::kCall, object.get(), method,
                         ToJni(std::forward<Args>(args))...);
  }

  template <typename T, typename... Args>
  ResultType<T> Call(const Object& object, const Method<T>& method,
                     Args&&... args) {
    return CallHelper<T>(CallTraits<JniType<T>>::kCall, object.get(),
                         method.id(), ToJni(std::forward<Args>(args))...);
  }

  template <typename T, typename... Args>
  ResultType<T> Call(const ArenaRef& object, const Method<T>& method,
                     Args&&... args) {
    return ok() ? Call(object.get(*this), method, std::forward<Args>(args)...)
                : ResultType<T>();
  }

  jfieldID GetStaticFieldId(const Class& clazz, const char* name,
                            const char* sig);

  template <typename T = Object>
  ResultType<T> GetStaticField(const Class& clazz, jfieldID field) {
    return CallHelper<T>(CallTraits<JniType<T>>::kGetStaticField, clazz.get(),
                         field);
  }

  template <typename T>
  ResultType<T> Get(const StaticField<T>& field) {
    return CallHelper<T>(CallTraits<JniType<T>>::kGetStaticField, field.clazz(),
                         field.id());
  }

  jmethodID GetStaticMethodId(const Class& clazz, const char* name,
                              const char* sig);

  template <typename T, typename... Args>
  ResultType<T> CallStatic(const Class& clazz, jmethodID method,
                           Args&&... args) {
    return CallHelper<T>(CallTraits<JniType<T>>::kCallStatic, clazz.get(),
                         method, ToJni(std::forward<Args>(args))...);
  }

  template <typename T, typename... Args>
  ResultType<T> Call(const StaticMethod<T>& method, Args&&... args) {
    return CallHelper<T>(CallTraits<JniType<T>>::kCallStatic, method.clazz(),
                         method.id(), ToJni(std::forward<Args>(args))...);
  }

  Local<String> NewStringUtf(const char* bytes);
  Local<String> NewStringUtf(const char* bytes, size_t size);
  Local<String> NewStringUtf(const std::string& bytes) {
    return NewStringUtf(bytes.c_str(), bytes.size());
  }
  std::string ToStringUtf(const String& string);

  template <typename T>
  size_t GetArrayLength(const Array<T>& array) {
    if (!ok() || !array) return 0;
    jsize result = env_->GetArrayLength(array.get());
    return result > 0 ? static_cast<size_t>(result) : 0u;
  }

  template <typename T = Object>
  EnableForReference<T, Local<Array<T>>> NewArray(size_t size,
                                                  jclass element_class) {
    if (!ok() || !element_class || !ValidSize(size)) return {};
    return MakeResult<Array<T>>(
        env_->NewObjectArray(static_cast<jsize>(size), element_class, nullptr));
  }

  template <typename T = Object>
  EnableForReference<T, Local<Array<T>>> NewArray(size_t size,
                                                  const Class& element_class) {
    return NewArray<T>(size, element_class.get());
  }

  template <typename T>
  EnableForPrimitive<T, Local<Array<T>>> NewArray(size_t size) {
    if (!ok() || !ValidSize(size)) return {};
    return MakeResult<Array<T>>(INVOKE(env_, CallTraits<JniType<T>>::kNewArray,
                                       static_cast<jsize>(size)));
  }

  template <typename T = Object>
  EnableForReference<T, Local<T>> GetArrayElement(const Array<T>& array,
                                                  size_t index) {
    if (!ok() || !array || !ValidSize(index)) return {};
    return MakeResult<T>(
        env_->GetObjectArrayElement(ToJni(array), static_cast<jsize>(index)));
  }

  template <typename T = Object>
  EnableForReference<T, void> SetArrayElement(Array<T>& array, size_t index,
                                              const Object& value) {
    if (!ok() || !array || !ValidSize(index)) return;
    env_->SetObjectArrayElement(ToJni(array), static_cast<jsize>(index),
                                ToJni(value));
  }

  template <typename T>
  EnableForPrimitive<T, void> GetArrayRegion(const Array<T>& array,
                                             size_t start, size_t len,
                                             T* buffer) {
    if (!ok() || !array || len == 0 || !buffer || !ValidRange(start, len)) {
      return;
    }
    if constexpr (std::is_same_v<T, bool>) {
      std::vector<jboolean> temp(len);
      env_->GetBooleanArrayRegion(ToJni(array), static_cast<jsize>(start),
                                  static_cast<jsize>(len), temp.data());
      if (ok()) {
        for (size_t i = 0; i < len; ++i) buffer[i] = temp[i] != JNI_FALSE;
      }
    } else {
      INVOKE(env_, CallTraits<JniType<T>>::kGetArrayRegion, ToJni(array),
             static_cast<jsize>(start), static_cast<jsize>(len), ToJni(buffer));
    }
  }

  template <typename T>
  EnableForPrimitive<T, std::vector<T>> GetArrayRegion(const Array<T>& array,
                                                       size_t start,
                                                       size_t len) {
    if (!ok() || !array || len == 0 || !ValidRange(start, len)) return {};
    if constexpr (std::is_same_v<T, bool>) {
      std::vector<jboolean> temp(len);
      env_->GetBooleanArrayRegion(ToJni(array), static_cast<jsize>(start),
                                  static_cast<jsize>(len), temp.data());
      if (!ok()) return {};
      std::vector<T> result(len);
      for (size_t i = 0; i < len; ++i) result[i] = temp[i] != JNI_FALSE;
      return result;
    } else {
      std::vector<T> result(len);
      GetArrayRegion(array, start, len, result.data());
      return ok() ? result : std::vector<T>();
    }
  }

  template <typename T>
  EnableForPrimitive<T, void> SetArrayRegion(Array<T>& array, size_t start,
                                             size_t len, const T* buffer) {
    if (!ok() || !array || len == 0 || !buffer || !ValidRange(start, len)) {
      return;
    }
    if constexpr (std::is_same_v<T, bool>) {
      std::vector<jboolean> temp(len);
      for (size_t i = 0; i < len; ++i) {
        temp[i] = buffer[i] ? JNI_TRUE : JNI_FALSE;
      }
      env_->SetBooleanArrayRegion(ToJni(array), static_cast<jsize>(start),
                                  static_cast<jsize>(len), temp.data());
    } else {
      INVOKE(env_, CallTraits<JniType<T>>::kSetArrayRegion, ToJni(array),
             static_cast<jsize>(start), static_cast<jsize>(len), ToJni(buffer));
    }
  }

 private:
  static constexpr bool ValidSize(size_t n) {
    return n <= static_cast<size_t>(std::numeric_limits<jsize>::max());
  }
  static constexpr bool ValidRange(size_t start, size_t len) {
    return ValidSize(start) &&
           len <= static_cast<size_t>(std::numeric_limits<jsize>::max()) - start;
  }

  template <typename T, typename M, typename Target, typename Id,
            typename... Args>
  ResultType<T> CallHelper(M&& env_method, Target target, Id id,
                           Args&&... args) {
    if constexpr (std::is_same_v<T, void>) {
      if (ok() && target && id) {
        INVOKE(env_, env_method, target, id, std::forward<Args>(args)...);
      }
    } else {
      if (!ok() || !target || !id) return {};
      return MakeResult<T>(
          INVOKE(env_, env_method, target, id, std::forward<Args>(args)...));
    }
  }

  std::string ErrorDescription(const Object& object);
  const char* ErrorName(jint error);

  template <typename T>
  EnableForPrimitive<T, T> MakeResult(JniType<T> value) {
    return static_cast<T>(value);
  }
  template <typename T>
  EnableForReference<T, Local<T>> MakeResult(jobject object) {
    return Local<T>(env_, static_cast<JniType<T>>(object));
  }

  JNIEnv* env_ = nullptr;
  UnhandledExceptionHandler exception_handler_ = nullptr;
  void* context_ = nullptr;
  int initial_pending_exceptions_ = 0;
};

#undef INVOKE

class ExceptionClearGuard {
 public:
  explicit ExceptionClearGuard(Env& env)
      : env_(env), exception_(env.ClearExceptionOccurred()) {}
  ExceptionClearGuard(const ExceptionClearGuard&) = delete;
  ExceptionClearGuard& operator=(const ExceptionClearGuard&) = delete;
  ~ExceptionClearGuard() {
    if (exception_) env_.Throw(exception_);
  }

 private:
  Env& env_;
  Local<Throwable> exception_;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ENV_H_
