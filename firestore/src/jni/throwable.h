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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_THROWABLE_H_
#define FIREBASE_FIRESTORE_SRC_JNI_THROWABLE_H_

#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * A wrapper for a JNI `jthrowable` that adds additional behavior. This is a
 * proxy for a Java Throwable in the JVM.
 *
 * `Throwable` merely holds values with `jthrowable` type, see `Local` and
 * `Global` template subclasses for reference-type-aware wrappers that
 * automatically manage the lifetime of JNI objects.
 */
class Throwable : public Object {
 public:
  Throwable() = default;
  explicit Throwable(jthrowable throwable) : Object(throwable) {}

  jthrowable get() const override { return static_cast<jthrowable>(object_); }

  /**
   * Gets the message associated with this throwable.
   *
   * This method can be run even when an exception is pending.
   */
  std::string GetMessage(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_THROWABLE_H_
