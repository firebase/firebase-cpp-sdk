#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_TRAITS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_TRAITS_H_

#include <jni.h>

#include "app/src/include/firebase/internal/type_traits.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {
namespace jni {

// clang-format off

// MARK: Primitive and Reference Type traits

template <typename T> struct IsPrimitive : public false_type {};
template <> struct IsPrimitive<jboolean> : public true_type {};
template <> struct IsPrimitive<jbyte> : public true_type {};
template <> struct IsPrimitive<jchar> : public true_type {};
template <> struct IsPrimitive<jshort> : public true_type {};
template <> struct IsPrimitive<jint> : public true_type {};
template <> struct IsPrimitive<jlong> : public true_type {};
template <> struct IsPrimitive<jfloat> : public true_type {};
template <> struct IsPrimitive<jdouble> : public true_type {};

template <typename T> struct IsReference : public false_type {};
template <> struct IsReference<jclass> : public true_type {};
template <> struct IsReference<jobject> : public true_type {};
template <> struct IsReference<jstring> : public true_type {};
template <> struct IsReference<jthrowable> : public true_type {};
template <> struct IsReference<jbyteArray> : public true_type {};
template <> struct IsReference<jobjectArray> : public true_type {};


// MARK: Type mapping

// A compile-time map from C++ types to their JNI equivalents.
template <typename T> struct JniTypeMap {};
template <> struct JniTypeMap<bool> { using type = jboolean; };
template <> struct JniTypeMap<uint8_t> { using type = jbyte; };
template <> struct JniTypeMap<uint16_t> { using type = jchar; };
template <> struct JniTypeMap<int16_t> { using type = jshort; };
template <> struct JniTypeMap<int32_t> { using type = jint; };
template <> struct JniTypeMap<int64_t> { using type = jlong; };
template <> struct JniTypeMap<float> { using type = jfloat; };
template <> struct JniTypeMap<double> { using type = jdouble; };
template <> struct JniTypeMap<size_t> { using type = jsize; };

template <> struct JniTypeMap<jobject> { using type = jobject; };

template <> struct JniTypeMap<Object> { using type = jobject; };

template <typename T>
using JniType = typename JniTypeMap<decay_t<T>>::type;

// clang-format on

// MARK: Enable If Helpers

template <typename T, typename R = T>
using EnableForPrimitive =
    typename enable_if<IsPrimitive<JniType<T>>::value, R>::type;

template <typename T, typename R = T>
using EnableForReference =
    typename enable_if<IsReference<JniType<T>>::value, R>::type;

// MARK: Type converters

// Converts C++ primitives to their equivalent JNI primitive types by casting.
template <typename T>
EnableForPrimitive<T, JniType<T>> ToJni(const T& value) {
  return static_cast<JniType<T>>(value);
}

// Converts JNI wrapper reference types (like `const Object&`) and any ownership
// wrappers of those types to their underlying `jobject`-derived reference.
template <typename T>
EnableForReference<T, JniType<T>> ToJni(const T& value) {
  return value.get();
}
template <typename T, typename J = typename T::jni_type>
J ToJni(const T& value) {
  return value.get();
}

// Preexisting JNI types can be passed directly. This makes incremental
// migration possible. Ideally this could eventually be removed.
inline jobject ToJni(jobject value) { return value; }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_TRAITS_H_
