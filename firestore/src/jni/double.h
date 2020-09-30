#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_DOUBLE_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_DOUBLE_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `Double`. */
class Double : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  static Class GetClass();

  static Local<Double> Create(Env& env, double value);

  double DoubleValue(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_DOUBLE_H_
