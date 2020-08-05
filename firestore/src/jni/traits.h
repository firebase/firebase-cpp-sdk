#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_TRAITS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_TRAITS_H_

#include <jni.h>

#include <cstddef>

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
template <> struct IsReference<jarray> : public true_type {};
template <> struct IsReference<jbyteArray> : public true_type {};
template <> struct IsReference<jobjectArray> : public true_type {};


// MARK: Type mapping

// A compile-time map from C++ types to their JNI equivalents.
template <typename T> struct JniTypeMap { using type = jobject; };

template <> struct JniTypeMap<bool> { using type = jboolean; };
template <> struct JniTypeMap<uint8_t> { using type = jbyte; };
template <> struct JniTypeMap<uint16_t> { using type = jchar; };
template <> struct JniTypeMap<int16_t> { using type = jshort; };
template <> struct JniTypeMap<int32_t> { using type = jint; };
template <> struct JniTypeMap<int64_t> { using type = jlong; };
template <> struct JniTypeMap<float> { using type = jfloat; };
template <> struct JniTypeMap<double> { using type = jdouble; };

template <> struct JniTypeMap<size_t> { using type = jsize; };
template <> struct JniTypeMap<void> { using type = void; };

template <> struct JniTypeMap<Class> { using type = jclass; };
template <> struct JniTypeMap<Object> { using type = jobject; };
template <> struct JniTypeMap<String> { using type = jstring; };
template <> struct JniTypeMap<Throwable> { using type = jthrowable; };

template <typename T> struct JniTypeMap<Array<T>> {
  using type = jobjectArray;
};
template <> struct JniTypeMap<Array<bool>> { using type = jbooleanArray; };
template <> struct JniTypeMap<Array<uint8_t>> { using type = jbyteArray; };
template <> struct JniTypeMap<Array<uint16_t>> { using type = jcharArray; };
template <> struct JniTypeMap<Array<int16_t>> { using type = jshortArray; };
template <> struct JniTypeMap<Array<int32_t>> { using type = jintArray; };
template <> struct JniTypeMap<Array<int64_t>> { using type = jlongArray; };
template <> struct JniTypeMap<Array<float>> { using type = jfloatArray; };
template <> struct JniTypeMap<Array<double>> { using type = jdoubleArray; };

template <typename T> struct JniTypeMap<Local<T>> {
  using type = typename JniTypeMap<T>::type;
};
template <typename T> struct JniTypeMap<Global<T>> {
  using type = typename JniTypeMap<T>::type;
};

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

namespace internal {

/**
 * An explicit ordering for overload resolution of JNI conversions. This allows
 * SFINAE without needing to make all the enable_if cases mutually exclusive.
 *
 * When finding a JNI converter, we try the following, in order:
 *   * pass through, for JNI primitive types;
 *   * static casts, for C++ primitive types;
 *   * reinterpret casts, for C++ pointers to primitive types (only where
 *     pointed-to sizes match);
 *   * pass through, for JNI reference types like jobject;
 *   * unwrapping, for JNI reference wrapper types like `Object` or
 *     `Local<String>`.
 *
 * `ConverterChoice` is a recursive type, defined such that `ConverterChoice<0>`
 * is the most derived type, `ConverterChoice<1>` and beyond are progressively
 * less derived. This causes the compiler to prioritize the overloads with
 * lower-numbered `ConverterChoice`s first, allowing compilation to succeed even
 * if multiple unqualified overloads would match, and would otherwise fail due
 * to an ambiguity.
 */
template <int I>
struct ConverterChoice : public ConverterChoice<I + 1> {};

template <>
struct ConverterChoice<4> {};

/**
 * Converts JNI primitive types to themselves.
 */
template <typename T,
          typename = typename enable_if<IsPrimitive<T>::value>::type>
T RankedToJni(T value, ConverterChoice<0>) {
  return value;
}

/**
 * Converts C++ primitive types to their equivalent JNI primitive types by
 * casting.
 */
template <typename T,
          typename = typename enable_if<IsPrimitive<JniType<T>>::value>::type>
JniType<T> RankedToJni(T value, ConverterChoice<1>) {
  return static_cast<JniType<T>>(value);
}

/**
 * Returns true if a reinterpret cast from a pointer to a C++ primitive type to
 * a pointer to the equivalent JNI type is well-defined. Reinterpreting certain
 * C++ types does not work:
 *
 *   * `bool` doesn't have a fully specified representation so any
 *     reinterpret_cast of such a value is invalid in portable code.
 *   * `size_t` does not have a fixed size, so an array of such values cannot
 *     be portably be reinterpreted as an array of jsize.
 */
template <typename T>
constexpr bool IsConvertiblePointer() {
  return IsPrimitive<JniType<T>>::value && !is_same<T, bool>::value &&
         !is_same<T, size_t>::value && sizeof(T) == sizeof(JniType<T>);
}

/**
 * Converts pointers to C++ primitive types to their equivalent JNI pointers to
 * primitive types by casting. This matches all potential primitive pointer
 * types and will fail to compile if the type is ineligible for automatic
 * conversion.
 */
template <typename T,
          typename = typename enable_if<IsPrimitive<JniType<T>>::value>::type>
const JniType<T>* RankedToJni(const T* value, ConverterChoice<2>) {
  static_assert(IsConvertiblePointer<T>(), "conversion must be well defined");
  return reinterpret_cast<const JniType<T>*>(value);
}
template <typename T,
          typename = typename enable_if<IsPrimitive<JniType<T>>::value>::type>
JniType<T>* RankedToJni(T* value, ConverterChoice<2>) {
  static_assert(IsConvertiblePointer<T>(), "conversion must be well defined");
  return reinterpret_cast<JniType<T>*>(value);
}

/**
 * Converts direct use of a JNI reference types to themselves.
 */
template <typename T,
          typename = typename enable_if<IsReference<T>::value>::type>
T RankedToJni(T value, ConverterChoice<3>) {
  return value;
}

#if defined(_STLPORT_VERSION)
using nullptr_t = decltype(nullptr);
#else
using nullptr_t = std::nullptr_t;
#endif

inline jobject RankedToJni(nullptr_t, ConverterChoice<2>) { return nullptr; }

/**
 * Converts wrapper types to JNI references by unwrapping them.
 */
template <typename T, typename J = JniType<T>>
J RankedToJni(const T& value, ConverterChoice<4>) {
  return value.get();
}

}  // namespace internal

template <typename T>
auto ToJni(const T& value)
    -> decltype(internal::RankedToJni(value, internal::ConverterChoice<0>{})) {
  return internal::RankedToJni(value, internal::ConverterChoice<0>{});
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_TRAITS_H_
