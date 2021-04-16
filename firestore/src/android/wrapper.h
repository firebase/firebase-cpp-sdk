#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {

class FieldValue;
class FirestoreInternal;

// This is the generalized wrapper base class which contains a FirestoreInternal
// client instance as well as a jobject, around which this is a wrapper.
class Wrapper {
 public:
  Wrapper(FirestoreInternal* firestore, const jni::Object& obj);

  Wrapper(const Wrapper& wrapper) = default;
  Wrapper(Wrapper&& wrapper) noexcept = default;

  virtual ~Wrapper();

  // So far, there is no use of assignment. So we do not bother to define our
  // own and delete the default one, which does not copy jobject properly.
  Wrapper& operator=(const Wrapper& wrapper) = delete;
  Wrapper& operator=(Wrapper&& wrapper) = delete;

  FirestoreInternal* firestore_internal() { return firestore_; }
  const jni::Object& ToJava() const { return obj_; }

  static jni::Object ToJava(const FieldValue& value);

 protected:
  // Default constructor. Subclass is expected to set the obj_ a meaningful
  // value.
  Wrapper();

  // Similar to a copy constructor, but can handle the case where `rhs` is null.
  explicit Wrapper(Wrapper* rhs);

  jni::Env GetEnv() const;

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jni::Global<jni::Object> obj_;

 private:
  friend class FirestoreInternal;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_
