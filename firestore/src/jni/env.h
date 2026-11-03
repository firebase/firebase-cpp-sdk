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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ENV_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ENV_H_

#include <jni.h>

#include <limits>
#include <string>
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

namespace firebase {
namespace firestore {
namespace jni {

#define INVOKE(env, method, ...) ((env)->*(method))(__VA_ARGS__)

/**
 * A wrapper around a JNIEnv pointer that makes dealing with JNI simpler in C++,
 * by:
 *
 *   * Automatically converting arguments of C++ types to their JNI equivalents;
 *   * handling C++ strings naturally;
 *   * wrapping JNI references in `Local` RAII wrappers automatically; and
 *   * simplifying error handling related to JNI calls (see below).
 *
 * Normally JNI requires that each call be followed by an explicit check to see
 * if an exception happened. This is tedious and clutters the code. Instead,
 * `Env` automatically checks for a JNI exception and short circuits any further
 * calls. This means that JNI-intensive code can be written straightforwardly
 * with a single, final check for errors. Exceptions can still be handled
 * inline if required.
 */
class Env {
 public:
  /**
   * An unhandled exception handler for Java exceptions that can be registered
   * by calling `SetUnhandledExceptionHandler`. The unhandled exception handler
   * is not invoked immediately after a Java exception is observed via JNI.
   * Instead it is invoked if `Env` starts destruction with a Java exception
   * pending.
   *
   * When calling the `UnhandledExceptionHandler`, `Env` does not automatically
   * clear any pending exceptions. The handler should call `Env::ExceptionClear`
   * if it wishes to clear the pending exception or use `ExceptionClearGuard` to
   * temporarily clear the pending exception.
   */
  using UnhandledExceptionHandler = void (*)(
      jni::Env& env, jni::Local<jni::Throwable>&& exception, void* context);

  Env();

  explicit Env(JNIEnv* env);

  /**
   * Destroys the Env instance. This can throw if an `UnhandledExceptionHandler`
   * has been registered and that function itself throws and there is no
   * exception currently being handled.
   */
  ~Env() noexcept(false);

  Env(const Env&) = delete;
  Env& operator=(const Env&) = delete;

  Env(Env&& other) noexcept
      : env_(other.env_),
        exception_handler_(other.exception_handler_),
        context_(other.context_),
        initial_pending_exceptions_(other.initial_pending_exceptions_) {
    other.exception_handler_ = nullptr;
    other.context_ = nullptr;
  }

  Env& operator=(Env&& other) noexcept {
    if (this != &other) {
      env_ = other.env_;
      exception_handler_ = other.exception_handler_;
      context_ = other.context_;
      initial_pending_exceptions_ = other.initial_pending_exceptions_;
      other.exception_handler_ = nullptr;
      other.context_ = nullptr;
    }
    return *this;
  }

  /** Returns true if the Env is valid and has not encountered an exception. */
  bool ok() const { return env_ != nullptr && !env_->ExceptionCheck(); }

  /** Returns the underlying JNIEnv pointer. */
  JNIEnv* get() const { return env_; }

  // MARK: Class Operations

  /**
   * Finds the java class associated with the given name which should be
   * formatted like "java/lang/Object".
   */
  Local<Class> FindClass(const char* name);

  // MARK: Exceptions

  void Throw(const Throwable& throwable);

  void ThrowNew(const Class& clazz, const char* message);
  void ThrowNew(const Class& clazz, const std::string& message) {
    ThrowNew(clazz, message.c_str());
  }

  /**
   * Returns the last Java exception to occur or an invalid reference. Does not
   * clear the pending exception; call `ExceptionClear` or
   * `ClearExceptionOccurred` to clear it.
   */
  Local<Throwable> ExceptionOccurred();

  /** Clears the last exception. */
  void ExceptionClear();

  /**
   * Returns the last Java exception to occur and clears the pending exception.
   */
  Local<Throwable> ClearExceptionOccurred();

  /**
   * Sets the exception handler to automatically invoke if there's a pending
   * exception when `Env` is being destroyed.
   */
  void SetUnhandledExceptionHandler(UnhandledExceptionHandler handler,
                                    void* context) {
    exception_handler_ = handler;
    context_ = context;
  }

  // MARK: Object Operations

  /**
   * Creates a new Java object of the given class, returning the result in a
   * reference wrapper of type T.
   *
   * @tparam T The C++ type to which the method should be coerced.
   * @tparam Args The C++ types of the arguments to the method.
   * @param clazz The Java class of the resulting object.
   * @param method The constructor method to invoke.
   * @param args The C++ arguments of the constructor. These will be converted
   *     to their JNI equivalent value with a call to ToJni before invocation.
   * @return a local reference to the newly-created object.
   */
  template <typename T = Object, typename... Args>
  Local<T> New(const Class& clazz, jmethodID method, Args&&... args) {
    if (!ok() || !clazz || !method) return {};

    jobject result = env_->NewObject(clazz.get(), method,
                                     ToJni(std::forward<Args>(args))...);
    RecordException();
    return MakeResult<T>(result);
  }

  template <typename T, typename... Args>
  Local<T> New(const Constructor<T>& ctor, Args&&... args) {
    if (!ok() || !ctor.clazz() || !ctor.id()) return {};

    auto result = env_->NewObject(ctor.clazz(), ctor.id(),
                                  ToJni(std::forward<Args>(args))...);
    RecordException();
    return MakeResult<T>(result);
  }

  Local<Class> GetObjectClass(const Object& object);

  bool IsInstanceOf(const Object& object, const Class& clazz);
  bool IsInstanceOf(const Object& object, jclass clazz) {
    return IsInstanceOf(object, Class(clazz));
  }

  bool IsSameObject(const Object& object1, const Object& object2);

  // MARK: Calling Instance Methods

  /**
   * Finds the method on the given class that's associated with the method name
   * and signature.
   */
  jmethodID GetMethodId(const Class& clazz, const char* name, const char* sig);

  /**
   * Invokes the JNI instance method using the `Call*Method` appropriate to the
   * return type T.
   *
   * @tparam T The C++ return type to which the method should be coerced.
   * @param object The object to use as `this` for the invocation.
   * @param method The method to invoke.
   * @param args The C++ arguments of the method. These will be converted to
   *     their JNI equivalent value with a call to ToJni before invocation.
   * @return The primitive result if T is a primitive, nothing if T is `void`,
   *     or a local reference to the returned object.
   */
  template <typename T, typename... Args>
  ResultType<T> Call(const Object& object, jmethodID method, Args&&... args) {
    auto env_method = CallTraits<JniType<T>>::kCall;
    return CallHelper<T>(env_method, object.get(), method,
                         ToJni(std::forward<Args>(args))...);
  }

  template <typename T, typename... Args>
  ResultType<T> Call(const Object& object,
                     const Method<T>& method,
                     Args&&... args) {
    auto env_method = CallTraits<JniType<T>>::kCall;
    return CallHelper<T>(env_method, object.get(), method.id(),
                         ToJni(std::forward<Args>(args))...);
  }

  template <typename T, typename... Args>
  ResultType<T> Call(const ArenaRef& object,
                     const Method<T>& method,
                     Args&&... args) {
    if (!ok()) return ResultType<T>();
    return Call(object.get(*this), method, std::forward<Args>(args)...);
  }

  // MARK: Accessing Static Fields

  jfieldID GetStaticFieldId(const Class& clazz,
                            const char* name,
                            const char* sig);

  /**
   * Returns the value of the given field using the GetStatic*Field method
   * appropriate to the field type T.
   *
   * @tparam T The C++ type to which the field value should be coerced.
   * @param clazz The class to use as the container for the field.
   * @param field The field to get.
   * @return a local reference to the field value.
   */
  template <typename T = Object>
  ResultType<T> GetStaticField(const Class& clazz, jfieldID field) {
    auto env_method = CallTraits<JniType<T>>::kGetStaticField;
    return CallHelper<T>(env_method, clazz.get(), field);
  }

  template <typename T>
  ResultType<T> Get(const StaticField<T>& field) {
    auto env_method = CallTraits<JniType<T>>::kGetStaticField;
    return CallHelper<T>(env_method, field.clazz(), field.id());
  }

  // MARK: Calling Static Methods

  /**
   * Finds the method on the given class that's associated with the method name
   * and signature.
   */
  jmethodID GetStaticMethodId(const Class& clazz,
                              const char* name,
                              const char* sig);

  /**
   * Invokes the JNI static method using the CallStatic*Method appropriate to
   * the return type T.
   *
   * @tparam T The C++ return type to which the method should be coerced.
   * @tparam Args The C++ types of the arguments to the method.
   * @param clazz The class against which to invoke the static method.
   * @param method The method to invoke.
   * @param args The C++ arguments of the method. These will be converted to
   *     their JNI equivalent value with a call to ToJni before invocation.
   * @return The primitive result if T is a primitive, nothing if T is `void`,
   *     or a local reference to the returned object.
   */
  template <typename T, typename... Args>
  ResultType<T> CallStatic(const Class& clazz,
                           jmethodID method,
                           Args&&... args) {
    auto env_method = CallTraits<JniType<T>>::kCallStatic;
    return CallHelper<T>(env_method, clazz.get(), method,
                         ToJni(std::forward<Args>(args))...);
  }

  template <typename T, typename... Args>
  ResultType<T> Call(const StaticMethod<T>& method, Args&&... args) {
    auto env_method = CallTraits<JniType<T>>::kCallStatic;
    return CallHelper<T>(env_method, method.clazz(), method.id(),
                         ToJni(std::forward<Args>(args))...);
  }

  // MARK: String Operations

  /**
   * Creates a new proxy for a Java String from a sequence of UTF-8 bytes.
   *
   * Note that unlike the underlying JNI method, these bytes should be encoded
   * in standard UTF-8, and *not* in the modified UTF-8 customarily used in the
   * JNI API.
   *
   * @param bytes The UTF-8 bytes from which to create a Java String.
   * @param size The number of bytes in the UTF-8 sequence.
   */
  Local<String> NewStringUtf(const char* bytes);
  Local<String> NewStringUtf(const char* bytes, size_t size);
  Local<String> NewStringUtf(const std::string& bytes) {
    return NewStringUtf(bytes.c_str(), bytes.size());
  }

  /**
   * Converts the given Java String to a C++ string encoded in UTF-8.
   *
   * Note that this string is encoded in standard UTF-8, and *not* in the
   * modified UTF-8 customarily used in the JNI API.
   */
  std::string ToStringUtf(const String& string);

  // MARK: Array Operations

  /**
   * Returns the length of the given Java Array.
   */
  template <typename T>
  size_t GetArrayLength(const Array<T>& array) {
    if (!ok() || !array) return 0;

    jsize result = env_->GetArrayLength(array.get());
    RecordException();
    return result > 0 ? static_cast<size_t>(result) : 0u;
  }

  /**
   * Creates a new object array where `element_class` is the required type for
   * each element.
   *
   * @tparam T The type of the C++ proxy for elements of the array.
   * @param size The fixed size of the array.
   * @param element_class The required class of each element, equivalent to the
   *     literal Java type before the square brackets. That is, to create a Java
   *     `String[]`, pass `String::GetClass()` for `element_class`.
   */
  template <typename T = Object>
  EnableForReference<T, Local<Array<T>>> NewArray(size_t size,
                                                  jclass element_class) {
    if (!ok() || !element_class ||
        size > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return {};
    }

    jobjectArray result = env_->NewObjectArray(static_cast<jsize>(size),
                                               element_class, nullptr);
    RecordException();
    return MakeResult<Array<T>>(result);
  }

  template <typename T = Object>
  EnableForReference<T, Local<Array<T>>> NewArray(size_t size,
                                                  const Class& element_class) {
    return NewArray<T>(size, element_class.get());
  }

  /**
   * Creates a new primitive array where the element type is derived from the
   * JNI type of `T`.
   *
   * @tparam T A C++ primitive type (like `uint8_t`) that maps onto a Java
   *     primitive type (like `byte`).
   * @param size The fixed size of the array.
   */
  template <typename T>
  EnableForPrimitive<T, Local<Array<T>>> NewArray(size_t size) {
    if (!ok() ||
        size > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return {};
    }

    auto env_method = CallTraits<JniType<T>>::kNewArray;
    auto result = INVOKE(env_, env_method, static_cast<jsize>(size));
    RecordException();
    return MakeResult<Array<T>>(result);
  }

  /**
   * Returns a reference to the element at the given index in the Java object
   * array.
   */
  template <typename T = Object>
  EnableForReference<T, Local<T>> GetArrayElement(const Array<T>& array,
                                                  size_t index) {
    if (!ok() || !array ||
        index > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return {};
    }

    jobject result =
        env_->GetObjectArrayElement(ToJni(array), static_cast<jsize>(index));
    RecordException();
    return MakeResult<T>(result);
  }

  /**
   * Sets the value at the given index in the Java object array.
   */
  template <typename T = Object>
  EnableForReference<T, void> SetArrayElement(Array<T>& array,
                                              size_t index,
                                              const Object& value) {
    if (!ok() || !array ||
        index > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return;
    }

    env_->SetObjectArrayElement(ToJni(array), static_cast<jsize>(index),
                                ToJni(value));
    RecordException();
  }

  /**
   * Copies elements in the given range from the Java array to the C++ buffer.
   * The caller must ensure that the buffer is large enough to copy `len`
   * elements.
   */
  template <typename T>
  typename enable_if<IsPrimitive<JniType<T>>::value && !is_same<T, bool>::value,
                     void>::type
  GetArrayRegion(const Array<T>& array, size_t start, size_t len, T* buffer) {
    if (!ok() || !array || len == 0) return;
    if (!buffer ||
        start > static_cast<size_t>(std::numeric_limits<jsize>::max()) ||
        len > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return;
    }

    auto env_method = CallTraits<JniType<T>>::kGetArrayRegion;
    INVOKE(env_, env_method, ToJni(array), static_cast<jsize>(start),
           static_cast<jsize>(len), ToJni(buffer));
    RecordException();
  }

  template <typename T>
  typename enable_if<is_same<T, bool>::value, void>::type
  GetArrayRegion(const Array<T>& array, size_t start, size_t len, T* buffer) {
    if (!ok() || !array || len == 0) return;
    if (!buffer ||
        start > static_cast<size_t>(std::numeric_limits<jsize>::max()) ||
        len > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return;
    }

    std::vector<jboolean> temp(len);
    env_->GetBooleanArrayRegion(ToJni(array), static_cast<jsize>(start),
                                static_cast<jsize>(len), temp.data());
    RecordException();
    if (!ok()) return;
    for (size_t i = 0; i < len; ++i) {
      buffer[i] = temp[i] != JNI_FALSE;
    }
  }

  /**
   * Copies elements in the given range from the Java array to a new C++ vector.
   */
  template <typename T>
  typename enable_if<IsPrimitive<JniType<T>>::value && !is_same<T, bool>::value,
                     std::vector<T>>::type
  GetArrayRegion(const Array<T>& array, size_t start, size_t len) {
    if (!ok() || !array || len == 0) return {};
    std::vector<T> result(len);
    GetArrayRegion(array, start, len, result.data());
    if (!ok()) return {};
    return result;
  }

  template <typename T>
  typename enable_if<is_same<T, bool>::value, std::vector<T>>::type
  GetArrayRegion(const Array<T>& array, size_t start, size_t len) {
    if (!ok() || !array || len == 0) return {};
    if (start > static_cast<size_t>(std::numeric_limits<jsize>::max()) ||
        len > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return {};
    }
    std::vector<jboolean> temp(len);
    env_->GetBooleanArrayRegion(ToJni(array), static_cast<jsize>(start),
                                static_cast<jsize>(len), temp.data());
    RecordException();
    if (!ok()) return {};
    std::vector<T> result(len);
    for (size_t i = 0; i < len; ++i) {
      result[i] = temp[i] != JNI_FALSE;
    }
    return result;
  }

  /**
   * Copies elements from the C++ buffer into the given range of the Java array.
   * The caller must ensure that the array is large enough to copy `len`
   * elements.
   */
  template <typename T>
  typename enable_if<IsPrimitive<JniType<T>>::value && !is_same<T, bool>::value,
                     void>::type
  SetArrayRegion(Array<T>& array, size_t start, size_t len, const T* buffer) {
    if (!ok() || !array || len == 0) return;
    if (!buffer ||
        start > static_cast<size_t>(std::numeric_limits<jsize>::max()) ||
        len > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return;
    }

    auto env_method = CallTraits<JniType<T>>::kSetArrayRegion;
    INVOKE(env_, env_method, ToJni(array), static_cast<jsize>(start),
           static_cast<jsize>(len), ToJni(buffer));
    RecordException();
  }

  template <typename T>
  typename enable_if<is_same<T, bool>::value, void>::type
  SetArrayRegion(Array<T>& array, size_t start, size_t len, const T* buffer) {
    if (!ok() || !array || len == 0) return;
    if (!buffer ||
        start > static_cast<size_t>(std::numeric_limits<jsize>::max()) ||
        len > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
      return;
    }

    std::vector<jboolean> temp(len);
    for (size_t i = 0; i < len; ++i) {
      temp[i] = buffer[i] ? JNI_TRUE : JNI_FALSE;
    }
    env_->SetBooleanArrayRegion(ToJni(array), static_cast<jsize>(start),
                                static_cast<jsize>(len), temp.data());
    RecordException();
  }

 private:
  /**
   * Invokes the JNI instance method using the given method reference on JNIEnv.
   *
   * @tparam T The non-void C++ return type to which the method's result should
   *     be coerced.
   * @param env_method A method reference from JNIEnv, appropriate for the
   *     return type T, and the kind of method being invoked (instance or
   *     static). Use `CallTraits<JniType<T>>::kCall` or `kCallStatic` to find
   *     the right method.
   * @param target The target class or object handle.
   * @param id The method or field ID.
   * @param args Any arguments to pass to the JNI method.
   * @return The primitive result if T is a primitive or a local reference to
   *     the returned object.
   */
  template <typename T, typename M, typename Target, typename Id, typename... Args>
  typename enable_if<!is_same<T, void>::value, ResultType<T>>::type CallHelper(
      M&& env_method, Target target, Id id, Args&&... args) {
    if (!ok() || !target || !id) return {};

    auto result =
        INVOKE(env_, env_method, target, id, std::forward<Args>(args)...);
    RecordException();
    return MakeResult<T>(result);
  }

  /**
   * Invokes a JNI call method if the return type is `void`.
   *
   * If `T` is anything but `void`, the overload is disabled.
   */
  template <typename T, typename M, typename Target, typename Id, typename... Args>
  typename enable_if<is_same<T, void>::value, void>::type CallHelper(
      M&& env_method, Target target, Id id, Args&&... args) {
    if (!ok() || !target || !id) return;

    INVOKE(env_, env_method, target, id, std::forward<Args>(args)...);
    RecordException();
  }

  void RecordException();
  std::string ErrorDescription(const Object& object);
  const char* ErrorName(jint error);

  template <typename T>
  EnableForPrimitive<T, T> MakeResult(JniType<T> value) {
    return static_cast<T>(value);
  }

  template <typename T>
  EnableForReference<T, Local<T>> MakeResult(jobject object) {
    // JNI object method results are always jobject, even when the actual type
    // is jstring or jclass. Cast to the correct type here so that Local<T>
    // doesn't have to account for this.
    auto typed_object = static_cast<JniType<T>>(object);
    return Local<T>(env_, typed_object);
  }

  JNIEnv* env_ = nullptr;

  UnhandledExceptionHandler exception_handler_ = nullptr;
  void* context_ = nullptr;
  int initial_pending_exceptions_ = 0;
};

#undef INVOKE

/**
 * Temporarily clears any pending exception state in the environment by calling
 * `JNIEnv::ExceptionClear`. If there was an exception pending when
 * `ExceptionClearGuard` is constructed, the guard restores that exception when
 * it is destructed. This is useful for executing cleanup code that needs to run
 * even if an exception is pending, similar to the way a `finally` block works
 * in Java.
 *
 * Like a Java `finally` block, if an exception is thrown before the
 * `ExceptionClearGuard` is destructed, that exception takes precedence and any
 * original exception is lost. Exceptions thrown during the lifetime of an
 * `ExceptionClearGuard` are not suppressed, so if a multi-step cleanup action
 * can throw, multiple `ExceptionClearGuard`s may be required.
 */
class ExceptionClearGuard {
 public:
  explicit ExceptionClearGuard(Env& env)
      : env_(env), exception_(env.ClearExceptionOccurred()) {}

  ExceptionClearGuard(const ExceptionClearGuard&) = delete;
  ExceptionClearGuard& operator=(const ExceptionClearGuard&) = delete;
  ExceptionClearGuard(ExceptionClearGuard&&) = delete;
  ExceptionClearGuard& operator=(ExceptionClearGuard&&) = delete;

  ~ExceptionClearGuard() {
    if (exception_) {
      if (!env_.ok()) {
        env_.ExceptionClear();
      }
      env_.Throw(exception_);
    }
  }

 private:
  Env& env_;
  Local<Throwable> exception_;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ENV_H_
