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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_OBJECT_H_
#define FIREBASE_FIRESTORE_SRC_JNI_OBJECT_H_

#include <jni.h>

#include <string>

#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

class Class;
class Env;
class Loader;

/**
 * A wrapper for a JNI `jobject` that adds additional behavior.
 *
 * `Object` merely holds values with `jobject` types, see `Local` and `Global`
 * template subclasses for reference-type-aware wrappers that automatically
 * manage the lifetime of JNI objects.
 */
class Object {
 public:
  Object() = default;
  constexpr explicit Object(jobject object) : object_(object) {}
  virtual ~Object() = default;

  explicit operator bool() const { return object_ != nullptr; }

  virtual jobject get() const { return object_; }

  static void Initialize(Loader& loader);

  static Class GetClass();

  /** Returns a non-owning proxy of type `U` that points to this object. */
  template <typename U>
  U CastTo() const& {
    return U(static_cast<JniType<U>>(get()));
  }

  /**
   * Converts this object to a C++ String encoded in UTF-8 by calling the Java
   * `toString` method on it.
   *
   * Note that this string is encoded in standard UTF-8, and *not* in the
   * modified UTF-8 customarily used in the JNI API.
   */
  std::string ToString(Env& env) const;

  bool Equals(Env& env, const Object& other) const;
  static bool Equals(Env& env, const Object& lhs, const Object& rhs);

 protected:
  jobject object_ = nullptr;
};

inline bool operator==(const Object& lhs, const Object& rhs) {
  return lhs.get() == rhs.get();
}

inline bool operator!=(const Object& lhs, const Object& rhs) {
  return !(lhs == rhs);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_OBJECT_H_
