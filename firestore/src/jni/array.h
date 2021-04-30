#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ARRAY_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ARRAY_H_

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

template <typename T>
class Array : public Object {
 public:
  using jni_type = JniType<Array<T>>;

  Array() = default;
  explicit Array(jni_type array) : Object(array) {}

  jni_type get() const override { return static_cast<jni_type>(Object::get()); }

  size_t Size(Env& env) const { return env.GetArrayLength(*this); }

  Local<T> Get(Env& env, size_t i) const {
    return env.GetArrayElement<T>(*this, i);
  }

  void Set(Env& env, size_t i, const T& value) {
    env.SetArrayElement<T>(*this, i, value);
  }
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ARRAY_H_
