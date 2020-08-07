#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_

#include <jni.h>

#include <vector>

#include "app/src/util_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

class FieldValueInternal;
class FirestoreInternal;

// This is the generalized wrapper base class which contains a FirestoreInternal
// client instance as well as a jobject, around which this wrapper is.
class Wrapper {
 public:
  // A global reference will be created from obj. The caller is responsible for
  // cleaning up any local references to obj after the constructor returns.
  Wrapper(FirestoreInternal* firestore, jobject obj);

  // Construct a new Java object and wrap around it. clazz is supposed to be the
  // java class of the type of the new Java object and method_id is supposed to
  // be the method id of one of the constructor of the new Java object. The
  // parameters to be used in constructing the new Java object have to be passed
  // through the variadic arguments in JNI-typed format such as jobject or jint.
  // If the constructor accepts no parameter, then leave the variadic arguments
  // empty.
  Wrapper(jclass clazz, jmethodID method_id, ...);

  Wrapper(const Wrapper& wrapper);

  Wrapper(Wrapper&& wrapper) noexcept;

  virtual ~Wrapper();

  // So far, there is no use of assignment. So we do not bother to define our
  // own and delete the default one, which does not copy jobject properly.
  Wrapper& operator=(const Wrapper& wrapper) = delete;
  Wrapper& operator=(Wrapper&& wrapper) = delete;

  FirestoreInternal* firestore_internal() { return firestore_; }
  jobject java_object() const { return obj_; }
  jni::Object ToJava() const { return jni::Object(obj_); }

  // Tests the equality of the wrapped Java Object.
  bool EqualsJavaObject(const Wrapper& other) const;

 protected:
  enum class AllowNullObject { Yes };
  // Default constructor. Subclass is expected to set the obj_ a meaningful
  // value.
  Wrapper();

  // Similar to Wrapper(FirestoreInternal*, jobject) but allowing obj be Null
  // Java object a.k.a. nullptr.
  Wrapper(FirestoreInternal* firestore, jobject obj, AllowNullObject);

  // Similar to a copy constructor, but can handle the case where `rhs` is null.
  explicit Wrapper(Wrapper* rhs);

  jni::Env GetEnv() const;

  // Converts a java list of java type e.g. java.util.List<FirestoreJavaType> to
  // a C++ vector of equivalent type e.g. std::vector<FirestoreType>.
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static void JavaListToStdVector(FirestoreInternal* firestore, jobject from,
                                  std::vector<PublicT>* to) {
    JNIEnv* env = firestore->app()->GetJNIEnv();
    int size =
        env->CallIntMethod(from, util::list::GetMethodId(util::list::kSize));
    CheckAndClearJniExceptions(env);
    to->clear();
    to->reserve(size);
    for (int i = 0; i < size; ++i) {
      jobject element = env->CallObjectMethod(
          from, util::list::GetMethodId(util::list::kGet), i);
      CheckAndClearJniExceptions(env);
      // Cannot call with emplace_back since the constructor is protected.
      to->push_back(PublicT{new InternalT{firestore, element}});
      env->DeleteLocalRef(element);
    }
  }

  // Converts a MapFieldValue to a java Map object that maps String to Object.
  // The caller is responsible for freeing the returned jobject via
  // JNIEnv::DeleteLocalRef().
  jobject MapFieldValueToJavaMap(const MapFieldValue& data);

  // Makes a variadic parameters from C++ MapFieldPathValue iterators up to the
  // given size. The caller is responsible for freeing the returned jobject via
  // JNIENV::DeleteLocalRef(). This helper takes iterators instead of
  // MapFieldPathValue directly because the Android native client API may
  // require passing the first pair explicit and thus the variadic starting from
  // the second pair.
  jobjectArray MapFieldPathValueToJavaArray(
      MapFieldPathValue::const_iterator begin,
      MapFieldPathValue::const_iterator end);

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jobject obj_ = nullptr;

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRAPPER_H_
