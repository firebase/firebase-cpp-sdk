// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_APP_SRC_JNI_TRAITS_H_
#define FIREBASE_APP_SRC_JNI_TRAITS_H_

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

namespace firebase::jni {

template <typename... T>
inline constexpr bool kAlwaysFalse = false;

template <int I>
struct ConverterChoice : ConverterChoice<I + 1> {};
template <>
struct ConverterChoice<5> {};

namespace internal {

struct UnmappedJniType {};

template <typename T>
struct IsPrimitiveImpl : std::false_type {};
template <> struct IsPrimitiveImpl<jboolean> : std::true_type {};
template <> struct IsPrimitiveImpl<jbyte> : std::true_type {};
template <> struct IsPrimitiveImpl<jchar> : std::true_type {};
template <> struct IsPrimitiveImpl<jshort> : std::true_type {};
template <> struct IsPrimitiveImpl<jint> : std::true_type {};
template <> struct IsPrimitiveImpl<jlong> : std::true_type {};
template <> struct IsPrimitiveImpl<jfloat> : std::true_type {};
template <> struct IsPrimitiveImpl<jdouble> : std::true_type {};

template <typename T>
struct IsReferenceImpl : std::false_type {};
template <> struct IsReferenceImpl<jobject> : std::true_type {};
template <> struct IsReferenceImpl<jclass> : std::true_type {};
template <> struct IsReferenceImpl<jstring> : std::true_type {};
template <> struct IsReferenceImpl<jarray> : std::true_type {};
template <> struct IsReferenceImpl<jobjectArray> : std::true_type {};
template <> struct IsReferenceImpl<jbooleanArray> : std::true_type {};
template <> struct IsReferenceImpl<jbyteArray> : std::true_type {};
template <> struct IsReferenceImpl<jcharArray> : std::true_type {};
template <> struct IsReferenceImpl<jshortArray> : std::true_type {};
template <> struct IsReferenceImpl<jintArray> : std::true_type {};
template <> struct IsReferenceImpl<jlongArray> : std::true_type {};
template <> struct IsReferenceImpl<jfloatArray> : std::true_type {};
template <> struct IsReferenceImpl<jdoubleArray> : std::true_type {};
template <> struct IsReferenceImpl<jthrowable> : std::true_type {};

}  // namespace internal

template <typename T>
struct IsPrimitive : internal::IsPrimitiveImpl<std::remove_cv_t<T>> {};
template <typename T>
inline constexpr bool IsPrimitiveV = IsPrimitive<T>::value;

template <typename T>
struct IsReference : internal::IsReferenceImpl<std::remove_cv_t<T>> {};
template <typename T>
inline constexpr bool IsReferenceV = IsReference<T>::value;

namespace internal {

template <typename T, typename = void>
struct HasNestedJniType : std::false_type {};
template <typename T>
struct HasNestedJniType<T, std::void_t<typename T::JniType>> : std::true_type {};

template <typename T, bool HasNested = HasNestedJniType<T>::value,
          bool IsRawJni = IsReference<T>::value || IsPrimitive<T>::value>
struct DefaultJniTypeMap {
  using type = UnmappedJniType;
};
template <typename T, bool IsRawJni>
struct DefaultJniTypeMap<T, true, IsRawJni> {
  using type = typename T::JniType;
};
template <typename T>
struct DefaultJniTypeMap<T, false, true> {
  using type = std::remove_cv_t<T>;
};

}  // namespace internal

template <typename T>
struct JniTypeMap : internal::DefaultJniTypeMap<std::remove_cv_t<T>> {};
template <> struct JniTypeMap<bool> { using type = jboolean; };
template <> struct JniTypeMap<char> { using type = jbyte; };
template <> struct JniTypeMap<signed char> { using type = jbyte; };
template <> struct JniTypeMap<unsigned char> { using type = jboolean; };
template <> struct JniTypeMap<short> { using type = jshort; };  // NOLINT
template <> struct JniTypeMap<unsigned short> { using type = jchar; };  // NOLINT
template <> struct JniTypeMap<char16_t> { using type = jchar; };
template <> struct JniTypeMap<int> { using type = jint; };
template <>
struct JniTypeMap<long> {  // NOLINT
  using type = std::conditional_t<sizeof(long) == sizeof(jint), jint, jlong>;  // NOLINT
};
template <> struct JniTypeMap<long long> { using type = jlong; };  // NOLINT
template <> struct JniTypeMap<size_t> { using type = jsize; };
template <> struct JniTypeMap<float> { using type = jfloat; };
template <> struct JniTypeMap<double> { using type = jdouble; };
template <> struct JniTypeMap<void> { using type = void; };
template <> struct JniTypeMap<jmethodID> { using type = jmethodID; };
template <> struct JniTypeMap<jfieldID> { using type = jfieldID; };

template <typename T>
using JniType = typename JniTypeMap<std::decay_t<T>>::type;

template <typename T, typename U = T>
using EnableForPrimitive = std::enable_if_t<IsPrimitive<JniType<T>>::value, U>;
template <typename T, typename U = T>
using EnableForReference = std::enable_if_t<IsReference<JniType<T>>::value, U>;

inline constexpr jsize SizeToJniSize(size_t value) noexcept {
  constexpr size_t kMaxJSize =
      static_cast<size_t>(std::numeric_limits<jsize>::max());
  return value > kMaxJSize ? std::numeric_limits<jsize>::max()
                           : static_cast<jsize>(value);
}

template <typename T, typename = std::enable_if_t<IsPrimitive<T>::value>>
inline constexpr T RankedToJni(T value, ConverterChoice<0>) noexcept {
  return value;
}

template <typename T, typename = std::enable_if_t<std::is_same_v<T, bool>>>
inline constexpr jboolean RankedToJni(T value, ConverterChoice<1>) noexcept {
  return value ? JNI_TRUE : JNI_FALSE;
}

template <typename T, typename = std::enable_if_t<std::is_same_v<T, size_t>>>
inline constexpr jsize RankedToJni(T value, ConverterChoice<1>) noexcept {
  return SizeToJniSize(value);
}

template <typename T,
          typename = std::enable_if_t<IsPrimitive<JniType<T>>::value &&
                                      !IsPrimitive<T>::value &&
                                      !std::is_same_v<T, bool> &&
                                      !std::is_same_v<T, size_t>>>
inline constexpr JniType<T> RankedToJni(T value, ConverterChoice<1>) noexcept {
  return static_cast<JniType<T>>(value);
}

inline jboolean* RankedToJni(bool* value, ConverterChoice<0>) noexcept {
  static_assert(sizeof(bool) == sizeof(jboolean));
  return reinterpret_cast<jboolean*>(value);
}

inline const jboolean* RankedToJni(const bool* value,
                                   ConverterChoice<0>) noexcept {
  static_assert(sizeof(bool) == sizeof(jboolean));
  return reinterpret_cast<const jboolean*>(value);
}

template <typename T>
inline constexpr bool IsConvertiblePointer() noexcept {
  return IsPrimitive<JniType<T>>::value && !std::is_same_v<T, bool> &&
         !std::is_same_v<T, size_t> && sizeof(T) == sizeof(JniType<T>);
}

template <typename T, typename = std::enable_if_t<IsConvertiblePointer<T>()>>
inline JniType<T>* RankedToJni(T* value, ConverterChoice<2>) noexcept {
  return reinterpret_cast<JniType<T>*>(value);
}

template <typename T, typename = std::enable_if_t<IsConvertiblePointer<T>()>>
inline const JniType<T>* RankedToJni(const T* value,
                                     ConverterChoice<2>) noexcept {
  return reinterpret_cast<const JniType<T>*>(value);
}

inline constexpr jobject RankedToJni(std::nullptr_t,
                                     ConverterChoice<2>) noexcept {
  return nullptr;
}

template <typename T, typename = std::enable_if_t<IsReference<T>::value>>
inline constexpr T RankedToJni(T value, ConverterChoice<3>) noexcept {
  return value;
}

template <typename T>
inline auto RankedToJni(const T& value, ConverterChoice<4>) noexcept
    -> decltype(value.get()) {
  return value.get();
}

template <typename T>
inline constexpr auto ToJni(const T& value) noexcept
    -> decltype(RankedToJni(value, ConverterChoice<0>{})) {
  return RankedToJni(value, ConverterChoice<0>{});
}

template <typename T>
inline constexpr auto ToJniPrimitive(T value) noexcept {
  if constexpr (IsPrimitive<JniType<T>>::value) {
    return ToJni(value);
  } else {
    static_assert(kAlwaysFalse<T>,
                  "Unsupported type for ToJniPrimitive conversion.");
  }
}

namespace internal {

inline constexpr bool ValidateJniFieldDescriptorPrefix(
    std::string_view sig, size_t* consumed) noexcept {
  if (sig.empty()) return false;
  size_t i = 0;
  while (i < sig.size() && sig[i] == '[') ++i;
  if (i >= sig.size()) return false;
  const char c = sig[i];
  if (c == 'B' || c == 'C' || c == 'D' || c == 'F' || c == 'I' || c == 'J' ||
      c == 'S' || c == 'Z') {
    if (consumed) *consumed = i + 1;
    return true;
  }
  if (c == 'L') {
    const size_t start = ++i;
    while (i < sig.size() && sig[i] != ';') {
      if (sig[i] == '.' || sig[i] == '[' || sig[i] == '(' || sig[i] == ')' ||
          sig[i] == '\0') {
        return false;
      }
      ++i;
    }
    if (i >= sig.size() || i == start) return false;
    if (consumed) *consumed = i + 1;
    return true;
  }
  return false;
}

}  // namespace internal

inline constexpr bool ValidateJniFieldDescriptor(
    std::string_view sig, size_t* consumed = nullptr) noexcept {
  size_t step = 0;
  if (!internal::ValidateJniFieldDescriptorPrefix(sig, &step)) return false;
  if (consumed) {
    *consumed = step;
    return true;
  }
  return step == sig.size();
}

inline constexpr bool ValidateJniMethodSignature(
    std::string_view sig) noexcept {
  if (sig.size() < 3 || sig[0] != '(') return false;
  size_t pos = 1;
  while (pos < sig.size() && sig[pos] != ')') {
    size_t step = 0;
    if (!internal::ValidateJniFieldDescriptorPrefix(sig.substr(pos), &step) ||
        step == 0) {
      return false;
    }
    pos += step;
  }
  if (pos >= sig.size() || sig[pos] != ')') return false;
  const std::string_view ret = sig.substr(pos + 1);
  return ret == "V" || ValidateJniFieldDescriptor(ret);
}

inline constexpr bool ValidateJniSignature(std::string_view sig) noexcept {
  if (sig.empty()) return false;
  return sig[0] == '(' ? ValidateJniMethodSignature(sig)
                       : ValidateJniFieldDescriptor(sig);
}

}  // namespace firebase::jni

#endif  // FIREBASE_APP_SRC_JNI_TRAITS_H_
