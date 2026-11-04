// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_CALL_TRAITS_H_
#define FIREBASE_FIRESTORE_SRC_JNI_CALL_TRAITS_H_

#include <jni.h>

#include "firestore/src/jni/traits.h"

namespace firebase::firestore::jni {

template <typename T>
struct CallTraits {
  static constexpr auto kCall = &JNIEnv::CallObjectMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticObjectField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticObjectMethod;
  static constexpr auto kNewArray = &JNIEnv::NewObjectArray;
};

#define FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jni_type, CallType)        \
  template <>                                                               \
  struct CallTraits<jni_type> {                                             \
    static constexpr auto kCall = &JNIEnv::Call##CallType##Method;          \
    static constexpr auto kGetStaticField =                                 \
        &JNIEnv::GetStatic##CallType##Field;                                \
    static constexpr auto kCallStatic =                                     \
        &JNIEnv::CallStatic##CallType##Method;                              \
    static constexpr auto kNewArray = &JNIEnv::New##CallType##Array;        \
    static constexpr auto kGetArrayRegion =                                 \
        &JNIEnv::Get##CallType##ArrayRegion;                                \
    static constexpr auto kSetArrayRegion =                                 \
        &JNIEnv::Set##CallType##ArrayRegion;                                \
  }

FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jboolean, Boolean);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jbyte, Byte);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jchar, Char);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jshort, Short);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jint, Int);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jlong, Long);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jfloat, Float);
FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE(jdouble, Double);
#undef FIRESTORE_JNI_PRIV_CALL_TRAITS_PRIMITIVE

template <>
struct CallTraits<void> {
  static constexpr auto kCall = &JNIEnv::CallVoidMethod;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticVoidMethod;
};

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

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_CALL_TRAITS_H_
