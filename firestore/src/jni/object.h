// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_OBJECT_H_
#define FIREBASE_FIRESTORE_SRC_JNI_OBJECT_H_

#include <jni.h>

#include <string>

#include "app/src/assert.h"
#include "firestore/src/jni/jni.h"
#include "firestore/src/jni/traits.h"

namespace firebase::firestore::jni {

class Class;
class Env;
class Loader;

class Object {
 public:
  Object() = default;
  constexpr explicit Object(jobject object) : object_(object) {}
  virtual ~Object() = default;
  Object(const Object&) = default;
  Object& operator=(const Object&) = default;

  [[nodiscard]] explicit operator bool() const { return object_ != nullptr; }
  [[nodiscard]] virtual jobject get() const { return object_; }

  static void Initialize(Loader& loader);
  static Class GetClass();

  template <typename U>
  [[nodiscard]] U CastTo() const& {
    return U(static_cast<JniType<U>>(get()));
  }

  std::string ToString(Env& env) const;
  bool Equals(Env& env, const Object& other) const;
  static bool Equals(Env& env, const Object& lhs, const Object& rhs);

 protected:
  jobject object_ = nullptr;
};

[[nodiscard]] inline bool operator==(const Object& lhs, const Object& rhs) {
  if (lhs.get() == rhs.get()) return true;
  if (!lhs || !rhs) return false;
  JNIEnv* env = GetEnv();
  FIREBASE_ASSERT(env != nullptr);
  return env->IsSameObject(lhs.get(), rhs.get());
}

[[nodiscard]] inline bool operator!=(const Object& lhs, const Object& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_OBJECT_H_
