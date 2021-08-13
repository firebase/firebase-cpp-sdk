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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_CLASS_H_
#define FIREBASE_FIRESTORE_SRC_JNI_CLASS_H_

#include <string>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * A wrapper for a JNI `jclass` that adds additional behavior. This is a proxy
 * for a Java Class in the JVM.
 *
 * `Class` merely holds values with `jclass` type, see `Local` and `Global`
 * template subclasses for reference-type-aware wrappers that automatically
 * manage the lifetime of JNI objects.
 */
class Class : public Object {
 public:
  Class() = default;
  explicit Class(jclass clazz) : Object(clazz) {}

  jclass get() const override { return static_cast<jclass>(object_); }

  /**
   * Returns the name of the class, as returned by the Java `Class.name` method.
   */
  std::string GetName(Env& env) const;

  static std::string GetClassName(Env& env, const Object& object);

  bool IsArray(Env& env) const;

 private:
  friend class Loader;

  static void Initialize(Loader& loader);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_CLASS_H_
