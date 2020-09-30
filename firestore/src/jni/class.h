#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_CLASS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_CLASS_H_

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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_CLASS_H_
