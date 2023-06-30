/*
 * Copyright 2023 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_

#include <jni.h>

#include <memory>

#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * An RAII wrapper for a global JNI reference, that automatically deletes the
 * JNI global reference when it goes out of scope.
 *
 * This class is mostly a drop-in replacement for the `Global` template class;
 * however, `ArenaRef` has the added benefit that it does _not_ consume a JNI
 * global reference from Android's limitied global reference pool. In contrast,
 * each `Global` instance consumes one JNI global reference.
 *
 * Instead, `ArenaRef` just stores a `long` unique ID, which is used as a key
 * into a Java HashMap. When the referenced object is needed then `ArenaRef`
 * retrieves the object from the hash table by its ID.
 *
 * This class supports move and copy semantics. Moves and copies are *very*
 * efficient: they have the same cost as the corresponding operation on a
 * std::shared_ptr<jlong> (which is very small compared to a JNI call).
 *
 * This class is not thread safe; concurrent use of an instance of this class
 * without external synchronization is undefined behavior. However, distinct
 * instances can be created concurrently in multiple threads as access to the
 * backing HashMap _is_ synchronized.
 */
class ArenaRef final {
 public:
  /**
   * Creates an `ArenaRef` that does not refer to any object.
   */
  ArenaRef() = default;

  ~ArenaRef() = default;

  /**
   * Creates an `ArenaRef` that does not refer to the given object.
   *
   * The given `Env` is used to perform the JNI call to insert the key/value
   * pair into the backing Java `HashMap`. The given `jobject` may be null, in
   * which case retrieving the object will return a null value.
   *
   * If the JNI call to insert the key/value pair into the backing Java HashMap
   * fails then this object will be in the same state as a default-constructed
   * instance.
   */
  ArenaRef(Env&, jobject);

  // Copy and move construction and assignment is supported.
  ArenaRef(const ArenaRef&) = default;
  ArenaRef(ArenaRef&&) = default;
  ArenaRef& operator=(const ArenaRef&) = default;
  ArenaRef& operator=(ArenaRef&&) = default;

  /**
   * Returns the Java object referenced by this `ArenaRef`.
   *
   * This function returns a Java "null" object in the following scenarios:
   * 1. This object was created using the default constructor.
   * 2. The object given to the constructor was a Java "null" object.
   * 3. The JNI call to retrieve the object from the backing Java HashMap fails,
   *   such as if there is a pending Java exception.
   */
  Local<Object> get(Env&) const;

  /**
   * Changes this object's referenced Java object to the given Java object.
   *
   * Subsequent invocations of `get()` will return the given object. The given
   * object may be a Java "null" object.
   *
   * If invoked with a pending Java exception then this `ArenaRef` is set to a
   * `null` Java object reference.
   */
  void reset(Env& env, const Object&);

  /**
   * Performs one-time initialization of the `ArenaRef` class.
   *
   * This function _must_ be called before any instances of `ArenaRef` are
   * created. Violating this requirement results in undefined behavior.
   *
   * It is safe to call this function multiple times: subsequent invocations
   * have no effect.
   *
   * This function is _not_ thread-safe; calling concurrently from multiple
   * threads results in undefined behavior.
   */
  static void Initialize(Loader&);

 private:
  class ObjectArenaEntry;

  void reset(Env& env, jobject);

  std::shared_ptr<ObjectArenaEntry> entry_;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
