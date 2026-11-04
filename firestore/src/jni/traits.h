// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_TRAITS_H_
#define FIREBASE_FIRESTORE_SRC_JNI_TRAITS_H_

#include <jni.h>

#include <cstddef>
#include <cstdint>

#include "app/src/include/firebase/internal/type_traits.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase::firestore::jni {

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
template <> struct IsReference<jbooleanArray> : public true_type {};
template <> struct IsReference<jbyteArray> : public true_type {};
template <> struct IsReference<jcharArray> : public true_type {};
template <> struct IsReference<jshortArray> : public true_type {};
template <> struct IsReference<jintArray> : public true_type {};
template <> struct IsReference<jlongArray> : public true_type {};
template <> struct IsReference<jfloatArray> : public true_type {};
template <> struct IsReference<jdoubleArray> : public true_type {};
template <> struct IsReference<jobjectArray> : public true_type {};

template <typename T> struct JniTypeMap { using type = jobject; };
template <> struct JniTypeMap<bool> { using type = jboolean; };
template <> struct JniTypeMap<int8_t> { using type = jbyte; };
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

template <typename T> struct JniTypeMap<Array<T>> { using type = jobjectArray; };
template <> struct JniTypeMap<Array<bool>> { using type = jbooleanArray; };
template <> struct JniTypeMap<Array<int8_t>> { using type = jbyteArray; };
template <> struct JniTypeMap<Array<uint8_t>> { using type = jbyteArray; };
template <> struct JniTypeMap<Array<uint16_t>> { using type = jcharArray; };
template <> struct JniTypeMap<Array<int16_t>> { using type = jshortArray; };
template <> struct JniTypeMap<Array<int32_t>> { using type = jintArray; };
template <> struct JniTypeMap<Array<int64_t>> { using type = jlongArray; };
template <> struct JniTypeMap<Array<float>> { using type = jfloatArray; };
template <> struct JniTypeMap<Array<double>> { using type = jdoubleArray; };

template <typename T> struct JniTypeMap<Local<T>> { using type = typename JniTypeMap<T>::type; };
template <typename T> struct JniTypeMap<Global<T>> { using type = typename JniTypeMap<T>::type; };

template <typename T> using JniType = typename JniTypeMap<decay_t<T>>::type;

template <typename T, typename R = T>
using EnableForPrimitive = typename enable_if<IsPrimitive<JniType<T>>::value, R>::type;

template <typename T, typename R = T>
using EnableForReference = typename enable_if<IsReference<JniType<T>>::value, R>::type;

namespace internal {

template <int I> struct ConverterChoice : public ConverterChoice<I + 1> {};
template <> struct ConverterChoice<4> {};

template <typename T, typename = typename enable_if<IsPrimitive<T>::value>::type>
T RankedToJni(T value, ConverterChoice<0>) { return value; }

template <typename T>
typename enable_if<is_same<T, bool>::value, jboolean>::type RankedToJni(
    T value, ConverterChoice<1>) {
  return value ? JNI_TRUE : JNI_FALSE;
}

template <typename T>
typename enable_if<IsPrimitive<JniType<T>>::value && !is_same<T, bool>::value, JniType<T>>::type
RankedToJni(T value, ConverterChoice<1>) {
  return static_cast<JniType<T>>(value);
}

template <typename T>
constexpr bool IsConvertiblePointer() {
  return IsPrimitive<JniType<T>>::value && !is_same<T, bool>::value &&
         !is_same<T, size_t>::value && sizeof(T) == sizeof(JniType<T>);
}

template <typename T, typename = typename enable_if<IsPrimitive<JniType<T>>::value>::type>
const JniType<T>* RankedToJni(const T* value, ConverterChoice<2>) {
  static_assert(IsConvertiblePointer<T>(), "conversion must be well defined");
  return reinterpret_cast<const JniType<T>*>(value);
}

template <typename T, typename = typename enable_if<IsPrimitive<JniType<T>>::value>::type>
JniType<T>* RankedToJni(T* value, ConverterChoice<2>) {
  static_assert(IsConvertiblePointer<T>(), "conversion must be well defined");
  return reinterpret_cast<JniType<T>*>(value);
}

template <typename T, typename = typename enable_if<IsReference<T>::value>::type>
T RankedToJni(T value, ConverterChoice<3>) { return value; }

inline jobject RankedToJni(std::nullptr_t, ConverterChoice<2>) { return nullptr; }

template <typename T, typename J = JniType<T>>
J RankedToJni(const T& value, ConverterChoice<4>) { return value.get(); }

}  // namespace internal

template <typename T>
auto ToJni(const T& value)
    -> decltype(internal::RankedToJni(value, internal::ConverterChoice<0>{})) {
  return internal::RankedToJni(value, internal::ConverterChoice<0>{});
}

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_TRAITS_H_
