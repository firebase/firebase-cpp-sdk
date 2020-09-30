#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_STRING_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_STRING_H_

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

  jstring get() const override { return static_cast<jstring>(object_); }

  static Class GetClass();

  /** Converts this Java String to a C++ string. */
  std::string ToString(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_STRING_H_
