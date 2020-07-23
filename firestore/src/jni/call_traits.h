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
};

template <>
struct CallTraits<jboolean> {
  static constexpr auto kCall = &JNIEnv::CallBooleanMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticBooleanField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticBooleanMethod;
};

template <>
struct CallTraits<jbyte> {
  static constexpr auto kCall = &JNIEnv::CallByteMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticByteField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticByteMethod;
};

template <>
struct CallTraits<jchar> {
  static constexpr auto kCall = &JNIEnv::CallCharMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticCharField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticCharMethod;
};

template <>
struct CallTraits<jshort> {
  static constexpr auto kCall = &JNIEnv::CallShortMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticShortField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticShortMethod;
};

template <>
struct CallTraits<jint> {
  static constexpr auto kCall = &JNIEnv::CallIntMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticIntField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticIntMethod;
};

template <>
struct CallTraits<jlong> {
  static constexpr auto kCall = &JNIEnv::CallLongMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticLongField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticLongMethod;
};

template <>
struct CallTraits<jfloat> {
  static constexpr auto kCall = &JNIEnv::CallFloatMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticFloatField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticFloatMethod;
};

template <>
struct CallTraits<jdouble> {
  static constexpr auto kCall = &JNIEnv::CallDoubleMethod;
  static constexpr auto kGetStaticField = &JNIEnv::GetStaticDoubleField;
  static constexpr auto kCallStatic = &JNIEnv::CallStaticDoubleMethod;
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
