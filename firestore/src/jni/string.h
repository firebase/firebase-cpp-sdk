// Copyright 2020 Google LLC

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
