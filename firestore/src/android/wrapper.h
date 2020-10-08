#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_

#include <vector>

#include "app/src/util_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/list.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {

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
  jobject java_object() const { return obj_.get(); }
  const jni::Object& ToJava() const { return obj_; }

  static jni::Object ToJava(const FieldValue& value);

 protected:
  // Default constructor. Subclass is expected to set the obj_ a meaningful
  // value.
  Wrapper();

  // Similar to a copy constructor, but can handle the case where `rhs` is null.
  explicit Wrapper(Wrapper* rhs);

  jni::Env GetEnv() const { return firestore_->GetEnv(); }

  // Converts a java list of java type e.g. java.util.List<FirestoreJavaType> to
  // a C++ vector of equivalent type e.g. std::vector<FirestoreType>.
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  std::vector<PublicT> MakeVector(jni::Env& env, const jni::List& from) const {
    size_t size = from.Size(env);
    std::vector<PublicT> result;
    result.reserve(size);

    for (int i = 0; i < size; ++i) {
      jni::Local<jni::Object> element = from.Get(env, i);

      // Avoid creating a partially valid public object on failure.
      // TODO(b/163140650): Use `return {}`
      // Clang 5 with STLPort gives a "chosen constructor is explicit in
      // copy-initialization" error because the default constructor is explicit.
      if (!env.ok()) return std::vector<PublicT>();

      // Use push_back because emplace_back requires a public constructor.
      result.push_back(PublicT{new InternalT{firestore_, element}});
    }
    return result;
  }

  // Converts a MapFieldValue to a Java Map object that maps String to Object.
  jni::Local<jni::HashMap> MakeJavaMap(jni::Env& env,
                                       const MapFieldValue& data) const;

  /**
   * The result of parsing a `MapFieldPathValue` object into its equivalent
   * arguments, prepared for calling a Firestore Java `update` method. `update`
   * takes its first two arguments separate from a varargs array.
   *
   * An `UpdateFieldPathArgs` object is only valid as long as the
   * `MapFieldPathValue` object from which it is created is valid because these
   * are not new references. This is reflected in the fact that `first_value`
   * is not an owning reference to its `jni::Object`.
   */
  struct UpdateFieldPathArgs {
    jni::Local<jni::Object> first_field;
    jni::Object first_value;
    jni::Local<jni::Array<jni::Object>> varargs;
  };

  // Creates the variadic parameters for a call to Java `update` from a C++
  // MapFieldPathValue. The result separates the first field and value because
  // Android Java API requires passing the first pair separately. The caller
  // is responsible for verifying that `data` has at least one element.
  UpdateFieldPathArgs MakeUpdateFieldPathArgs(
      jni::Env& env, const MapFieldPathValue& data) const;

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jni::Global<jni::Object> obj_;

 private:
  friend class FirestoreInternal;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_
