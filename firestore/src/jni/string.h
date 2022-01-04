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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_STRING_H_
#define FIREBASE_FIRESTORE_SRC_JNI_STRING_H_

#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

class Class;

/**
 * A wrapper for a JNI `jstring` that adds additional behavior. This is a proxy
 * for a Java String in the JVM.
 *
 * `String` merely holds values with `jstring` type, see `Local` and `Global`
 * template subclasses for reference-type-aware wrappers that automatically
 * manage the lifetime of JNI objects.
 */
class String : public Object {
 public:
  String() = default;
  explicit String(jstring string) : Object(string) {}

  /**
   * Creates a new Java String from the given bytes, using the given encoding.
   * This matches the behavior of the Java `String(byte[], String)` constructor.
   *
   * @param bytes A Java array of encoded bytes.
   * @param encoding A Java string naming the encoding of the bytes.
   */
  static Local<String> Create(Env& env,
                              const Array<uint8_t>& bytes,
                              const String& encoding);

  jstring get() const override { return static_cast<jstring>(object_); }

  static void Initialize(Env& env, Loader& loader);
  static void Terminate(Env& env);

  static Class GetClass();

  /** Returns a Java String representing "UTF-8". */
  static String GetUtf8();

  Local<Array<uint8_t>> GetBytes(Env& env, const String& encoding) const;

  /**
   * Converts this Java String to a C++ string encoded in UTF-8.
   *
   * Note that this string is encoded in standard UTF-8, and *not* in the
   * modified UTF-8 customarily used in the JNI API.
   */
  std::string ToString(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_STRING_H_
