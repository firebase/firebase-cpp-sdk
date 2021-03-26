#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_DECLARATION_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_DECLARATION_H_

#include <jni.h>

namespace firebase {
namespace firestore {
namespace jni {

class Class;
class Env;

/**
 * A base class containing the details of a Java constructor. See
 * Constructor<T>.
 */
class ConstructorBase {
 public:
  /**
   * Creates a new method descriptor from an argument signature. The argument
   * should be a string literal--this class does not take ownership of nor copy
   * the string. The internal method ID should be populated later with a call to
   * `Loader::Load`.
   */
  constexpr explicit ConstructorBase(const char* sig) : sig_(sig) {}

  ConstructorBase(jclass clazz, jmethodID id) : clazz_(clazz), id_(id) {}

  jclass clazz() const { return clazz_; }
  jmethodID id() const { return id_; }

 private:
  friend class Loader;

  const char* sig_ = nullptr;

  jclass clazz_ = nullptr;
  jmethodID id_ = nullptr;
};

/**
 * A declaration of a Java constructor. This is intended to be used as a global
 * variable and loaded once the JavaVM is available.
 *
 * @tparam T The C++ type of the new object.
 */
template <typename T>
class Constructor : public ConstructorBase {
 public:
  using ConstructorBase::ConstructorBase;
};

/**
 * A base class containing the details of a Java method. See Method<T>.
 */
class MethodBase {
 public:
  /**
   * Creates a new method descriptor from a name and signature. These arguments
   * should be string literals--this class does not take ownership of nor copy
   * these strings. The internal method ID should be populated later with a call
   * to `Loader::Load`.
   */
  constexpr MethodBase(const char* name, const char* sig)
      : name_(name), sig_(sig) {}

  explicit MethodBase(jmethodID id) : id_(id) {}

  jmethodID id() const { return id_; }

 private:
  friend class Loader;

  const char* name_ = nullptr;
  const char* sig_ = nullptr;

  jmethodID id_ = nullptr;
};

/**
 * A declaration of a Java method. This is intended to be used as a global
 * variable and loaded once the JavaVM is available.
 *
 * @tparam T The C++ return type of the method.
 */
template <typename T>
class Method : public MethodBase {
 public:
  using MethodBase::MethodBase;
};

/**
 * A base class containing the details of a Java static field. See
 * StaticField<T>.
 */
class StaticFieldBase {
 public:
  /**
   * Creates a new static field descriptor from a name and signature. These
   * arguments should be string literals--this class does not take ownership of
   * these strings. The internal class and field ID should be populated later
   * with a call to `Loader::Load`.
   */
  constexpr StaticFieldBase(const char* name, const char* sig)
      : name_(name), sig_(sig) {}

  explicit StaticFieldBase(jfieldID id) : id_(id) {}

  jclass clazz() const { return clazz_; }

  jfieldID id() const { return id_; }

 private:
  friend class Loader;

  const char* name_ = nullptr;
  const char* sig_ = nullptr;

  jclass clazz_ = nullptr;
  jfieldID id_ = nullptr;
};

/**
 * A declaration of a Java static field. This is intended to be used as a global
 * variable and loaded once the JavaVM is available.
 *
 * @tparam T The C++ type of the field.
 */
template <typename T>
class StaticField : public StaticFieldBase {
 public:
  using StaticFieldBase::StaticFieldBase;
};

/**
 * A base class containing the details of a Java static method. See
 * StaticMethod<T>.
 */
class StaticMethodBase {
 public:
  /**
   * Creates a new static method descriptor from a name and signature. These
   * arguments should be string literals--this class does not take ownership of
   * nor copy these strings. The internal method ID should be populated later
   * with a call to `Loader::Load`.
   */
  constexpr StaticMethodBase(const char* name, const char* sig)
      : name_(name), sig_(sig) {}

  StaticMethodBase(jclass clazz, jmethodID id) : clazz_(clazz), id_(id) {}

  jclass clazz() const { return clazz_; }
  jmethodID id() const { return id_; }

 private:
  friend class Loader;

  const char* name_ = nullptr;
  const char* sig_ = nullptr;

  jclass clazz_ = nullptr;
  jmethodID id_ = nullptr;
};

/**
 * A declaration of a Java static method. This is intended to be used as a
 * global variable and loaded once the JavaVM is available.
 *
 * @tparam T The C++ return type of the method.
 */
template <typename T>
class StaticMethod : public StaticMethodBase {
 public:
  using StaticMethodBase::StaticMethodBase;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_DECLARATION_H_
