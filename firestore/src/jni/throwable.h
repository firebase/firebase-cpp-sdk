#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_THROWABLE_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_THROWABLE_H_

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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_THROWABLE_H_
