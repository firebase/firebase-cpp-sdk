// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_DECLARATION_H_
#define FIREBASE_FIRESTORE_SRC_JNI_DECLARATION_H_

#include <jni.h>

namespace firebase::firestore::jni {

class Class;
class Env;

class ConstructorBase {
 public:
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

template <typename T>
struct Constructor : ConstructorBase {
  using ConstructorBase::ConstructorBase;
};

class MethodBase {
 public:
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

template <typename T>
struct Method : MethodBase {
  using MethodBase::MethodBase;
};

class StaticFieldBase {
 public:
  constexpr StaticFieldBase(const char* name, const char* sig)
      : name_(name), sig_(sig) {}
  StaticFieldBase(jclass clazz, jfieldID id) : clazz_(clazz), id_(id) {}
  jclass clazz() const { return clazz_; }
  jfieldID id() const { return id_; }

 private:
  friend class Loader;
  const char* name_ = nullptr;
  const char* sig_ = nullptr;
  jclass clazz_ = nullptr;
  jfieldID id_ = nullptr;
};

template <typename T>
struct StaticField : StaticFieldBase {
  using StaticFieldBase::StaticFieldBase;
};

class StaticMethodBase {
 public:
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

template <typename T>
struct StaticMethod : StaticMethodBase {
  using StaticMethodBase::StaticMethodBase;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_DECLARATION_H_
