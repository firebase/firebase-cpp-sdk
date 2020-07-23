#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ENV_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ENV_H_

#include <jni.h>

#include <string>

#include "app/meta/move.h"
#include "firestore/src/jni/call_traits.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

// Since we're targeting STLPort, `std::invoke` is not available.
#define INVOKE(env, method, ...) ((env)->*(method))(__VA_ARGS__);

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
  Env() : env_(GetEnv()) {}

  explicit Env(JNIEnv* env) : env_(env) {}

  /** Returns true if the Env has not encountered an exception. */
  bool ok() const { return last_exception_ == nullptr; }

  /** Returns the underlying JNIEnv pointer. */
  JNIEnv* get() const { return env_; }

  // MARK: Class Operations

  /**
   * Finds the java class associated with the given name which should be
   * formatted like "java/lang/Object".
   */
  Local<Class> FindClass(const char* name);

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
    if (!ok()) return {};

    jobject result =
        env_->NewObject(clazz.get(), method, ToJni(Forward<Args>(args))...);
    RecordException();
    return MakeResult<T>(result);
  }

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
                         ToJni(Forward<Args>(args))...);
  }

  // MARK: Accessing Static Fields

  jfieldID GetStaticFieldId(const Class& clazz, const char* name,
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
    if (!ok()) return {};

    auto env_method = CallTraits<JniType<T>>::kGetStaticField;
    auto result = INVOKE(env_, env_method, clazz.get(), field);
    RecordException();
    return MakeResult<T>(result);
  }

  // MARK: Calling Static Methods

  /**
   * Finds the method on the given class that's associated with the method name
   * and signature.
   */
  jmethodID GetStaticMethodId(const Class& clazz, const char* name,
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
  ResultType<T> CallStatic(const Class& clazz, jmethodID method,
                           Args&&... args) {
    auto env_method = CallTraits<JniType<T>>::kCallStatic;
    return CallHelper<T>(env_method, clazz.get(), method,
                         ToJni(Forward<Args>(args))...);
  }

  // MARK: String Operations

  /**
   * Creates a new proxy for a Java String from a sequences of modified UTF-8
   * bytes.
   */
  Local<String> NewStringUtf(const char* bytes);
  Local<String> NewStringUtf(const std::string& bytes) {
    return NewStringUtf(bytes.c_str());
  }

  /** Returns the length of the string in modified UTF-8 bytes. */
  size_t GetStringUtfLength(const String& string) {
    if (!ok()) return 0;

    jsize result = env_->GetStringUTFLength(string.get());
    RecordException();
    return static_cast<size_t>(result);
  }

  /**
   * Copies the contents of a region of a Java string to a C++ string. The
   * resulting string has a modified UTF-8 encoding.
   */
  std::string GetStringUtfRegion(const String& string, size_t start,
                                 size_t len);

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
   * @param args The method and JNI arguments of the JNI method, including the
   *     class or object, jmethodID, and any arguments to pass.
   * @return The primitive result if T is a primitive or a local reference to
   *     the returned object.
   */
  template <typename T, typename M, typename... Args>
  typename enable_if<!is_same<T, void>::value, ResultType<T>>::type CallHelper(
      M&& env_method, Args&&... args) {
    if (!ok()) return {};

    auto result = INVOKE(env_, env_method, Forward<Args>(args)...);
    RecordException();
    return MakeResult<T>(result);
  }

  /**
   * Invokes a JNI call method if the return type is `void`.
   *
   * If `T` is anything but `void`, the overload is disabled.
   */
  template <typename T, typename M, typename... Args>
  typename enable_if<is_same<T, void>::value, void>::type CallHelper(
      M&& env_method, Args&&... args) {
    if (!ok()) return;

    INVOKE(env_, env_method, Forward<Args>(args)...);
    RecordException();
  }

  void RecordException();

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
  jthrowable last_exception_ = nullptr;
};

#undef INVOKE

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ENV_H_
