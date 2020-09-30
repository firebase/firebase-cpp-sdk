#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_CALL_TRAITS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_CALL_TRAITS_H_

#include <jni.h>

#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * Traits describing how to invoke JNI methods uniformly for various JNI return
 * types.
 *
 * By default, uses Object variants (e.g. `CallObjectMethod`), since most types
 * will use this form. Only primitives need special forms.
 */
template <typename T>
struct CallTraits {
  static constexpr auto kCall = &JNIEnv::CallObjectMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticObjectField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticObjectMethod;
  static constexpr auto kNewArray = &JNIEnv::NewObjectArray;
};

template <>
struct CallTraits<jboolean> {
  static constexpr auto kCall = &JNIEnv::CallBooleanMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticBooleanField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticBooleanMethod;
  static constexpr auto kNewArray = &JNIEnv::NewBooleanArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetBooleanArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetBooleanArrayRegion;
};

template <>
struct CallTraits<jbyte> {
  static constexpr auto kCall = &JNIEnv::CallByteMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticByteField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticByteMethod;
  static constexpr auto kNewArray = &JNIEnv::NewByteArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetByteArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetByteArrayRegion;
};

template <>
struct CallTraits<jchar> {
  static constexpr auto kCall = &JNIEnv::CallCharMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticCharField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticCharMethod;
  static constexpr auto kNewArray = &JNIEnv::NewCharArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetCharArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetCharArrayRegion;
};

template <>
struct CallTraits<jshort> {
  static constexpr auto kCall = &JNIEnv::CallShortMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticShortField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticShortMethod;
  static constexpr auto kNewArray = &JNIEnv::NewShortArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetShortArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetShortArrayRegion;
};

template <>
struct CallTraits<jint> {
  static constexpr auto kCall = &JNIEnv::CallIntMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticIntField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticIntMethod;
  static constexpr auto kNewArray = &JNIEnv::NewIntArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetIntArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetIntArrayRegion;
};

template <>
struct CallTraits<jlong> {
  static constexpr auto kCall = &JNIEnv::CallLongMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticLongField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticLongMethod;
  static constexpr auto kNewArray = &JNIEnv::NewLongArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetLongArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetLongArrayRegion;
};

template <>
struct CallTraits<jfloat> {
  static constexpr auto kCall = &JNIEnv::CallFloatMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticFloatField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticFloatMethod;
  static constexpr auto kNewArray = &JNIEnv::NewFloatArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetFloatArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetFloatArrayRegion;
};

template <>
struct CallTraits<jdouble> {
  static constexpr auto kCall = &JNIEnv::CallDoubleMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticDoubleField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticDoubleMethod;
  static constexpr auto kNewArray = &JNIEnv::NewDoubleArray;
  static constexpr auto kGetArrayRegion = &JNIEnv::GetDoubleArrayRegion;
  static constexpr auto kSetArrayRegion = &JNIEnv::SetDoubleArrayRegion;
};

template <>
struct CallTraits<void> {
  static constexpr auto kCall = &JNIEnv::CallVoidMethod;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticVoidMethod;
};

/**
 * The type of the result of a JNI function. For reference types, it's always
 * a `Local` wrapper of the type. For primitive types, it's just the type
 * itself.
 */
template <typename T, bool = IsPrimitive<JniType<T>>::value>
struct ResultTypeMap {
  using type = T;
};

template <typename T>
struct ResultTypeMap<T, false> {
  using type = Local<T>;
};

template <>
struct ResultTypeMap<void, false> {
  using type = void;
};

template <typename T>
using ResultType = typename ResultTypeMap<T>::type;

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_CALL_TRAITS_H_
