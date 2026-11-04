// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_STRING_H_
#define FIREBASE_FIRESTORE_SRC_JNI_STRING_H_

#include <cstdint>
#include <string>

#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase::firestore::jni {

class String : public Object {
 public:
  String() = default;
  explicit String(jstring string) : Object(string) {}

  static Local<String> Create(Env& env, const Array<uint8_t>& bytes,
                              const String& encoding);

  jstring get() const override { return static_cast<jstring>(object_); }

  static void Initialize(Env& env, Loader& loader);
  static void Terminate(Env& env);
  static Class GetClass();
  static String GetUtf8();

  Local<Array<uint8_t>> GetBytes(Env& env, const String& encoding) const;

  using Object::ToString;
  std::string ToString(Env& env) const;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_STRING_H_
