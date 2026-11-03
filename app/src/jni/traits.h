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

namespace firebase {
namespace jni {

// Always-false helper for dependent static_asserts in uninstantiated templates.
template <typename... T>
inline constexpr bool kAlwaysFalse = false;

// Priority tag hierarchy for overload resolution without ambiguous enable_if.
template <int I>
struct ConverterChoice : ConverterChoice<I + 1> {};

template <>
struct ConverterChoice<5> {};

namespace internal {

// Sentinel type returned by DefaultJniTypeMap for unmapped C++ types so that
// unmapped types never satisfy IsPrimitive or IsReference.
struct UnmappedJniType {};

template <typename T>
struct IsPrimitiveImpl : std::false_type {};

template <>
struct IsPrimitiveImpl<jboolean> : std::true_type {};
template <>
struct IsPrimitiveImpl<jbyte> : std::true_type {};
template <>
struct IsPrimitiveImpl<jchar> : std::true_type {};
template <>
struct IsPrimitiveImpl<jshort> : std::true_type {};
template <>
struct IsPrimitiveImpl<jint> : std::true_type {};
template <>
struct IsPrimitiveImpl<jlong> : std::true_type {};
template <>
struct IsPrimitiveImpl<jfloat> : std::true_type {};
template <>
struct IsPrimitiveImpl<jdouble> : std::true_type {};

template <typename T>
struct IsReferenceImpl : std::false_type {};

template <>
struct IsReferenceImpl<jobject> : std::true_type {};
template <>
struct IsReferenceImpl<jclass> : std::true_type {};
template <>
struct IsReferenceImpl<jstring> : std::true_type {};
template <>
struct IsReferenceImpl<jarray> : std::true_type {};
template <>
struct IsReferenceImpl<jobjectArray> : std::true_type {};
template <>
struct IsReferenceImpl<jbooleanArray> : std::true_type {};
template <>
struct IsReferenceImpl<jbyteArray> : std::true_type {};
template <>
struct IsReferenceImpl<jcharArray> : std::true_type {};
template <>
struct IsReferenceImpl<jshortArray> : std::true_type {};
template <>
struct IsReferenceImpl<jintArray> : std::true_type {};
template <>
struct IsReferenceImpl<jlongArray> : std::true_type {};
template <>
struct IsReferenceImpl<jfloatArray> : std::true_type {};
template <>
struct IsReferenceImpl<jdoubleArray> : std::true_type {};
template <>
struct IsReferenceImpl<jthrowable> : std::true_type {};

}  // namespace internal

// Trait identifying raw JNI primitive types (cv-insensitive).
template <typename T>
struct IsPrimitive : internal::IsPrimitiveImpl<std::remove_cv_t<T>> {};

template <typename T>
inline constexpr bool IsPrimitiveV = IsPrimitive<T>::value;

// Trait identifying raw JNI reference types, including all 8 primitive array
// types (fixes BUG-0167, cv-insensitive).
template <typename T>
struct IsReference : internal::IsReferenceImpl<std::remove_cv_t<T>> {};

template <typename T>
inline constexpr bool IsReferenceV = IsReference<T>::value;

namespace internal {

template <typename T, typename = void>
struct HasNestedJniType : std::false_type {};

template <typename T>
struct HasNestedJniType<T, std::void_t<typename T::JniType>> : std::true_type {
};

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

// Maps C++ types or JNI wrapper types to their underlying raw JNI type.
// Note: In <jni.h>, `jboolean` is `unsigned char` (`uint8_t`) and `jbyte` is
// `signed char` (`int8_t`). Keeping `unsigned char` mapped to `jboolean`
// preserves `JniType<jboolean> == jboolean` while `Array<uint8_t>` specializes
// its element/array traits in `env.h` to `jbyteArray`.
template <typename T>
struct JniTypeMap : internal::DefaultJniTypeMap<std::remove_cv_t<T>> {};

template <>
struct JniTypeMap<bool> {
  using type = jboolean;
};
template <>
struct JniTypeMap<char> {
  using type = jbyte;
};
template <>
struct JniTypeMap<signed char> {
  using type = jbyte;
};
template <>
struct JniTypeMap<unsigned char> {
  using type = jboolean;
};
template <>
struct JniTypeMap<short> {  // NOLINT(runtime/int)
  using type = jshort;
};
template <>
struct JniTypeMap<unsigned short> {  // NOLINT(runtime/int)
  using type = jchar;
};
template <>
struct JniTypeMap<char16_t> {
  using type = jchar;
};
template <>
struct JniTypeMap<int> {
  using type = jint;
};
template <>
struct JniTypeMap<long> {  // NOLINT(runtime/int)
  using type = std::conditional_t<sizeof(long) == sizeof(jint), jint, jlong>;  // NOLINT(runtime/int)
};
template <>
struct JniTypeMap<long long> {  // NOLINT(runtime/int)
  using type = jlong;
};
template <>
struct JniTypeMap<size_t> {
  using type = jsize;
};
template <>
struct JniTypeMap<float> {
  using type = jfloat;
};
template <>
struct JniTypeMap<double> {
  using type = jdouble;
};
template <>
struct JniTypeMap<void> {
  using type = void;
};
template <>
struct JniTypeMap<jmethodID> {
  using type = jmethodID;
};
template <>
struct JniTypeMap<jfieldID> {
  using type = jfieldID;
};

template <typename T>
using JniType = typename JniTypeMap<std::decay_t<T>>::type;

template <typename T, typename U = T>
using EnableForPrimitive =
    std::enable_if_t<IsPrimitive<JniType<T>>::value, U>;

template <typename T, typename U = T>
using EnableForReference =
    std::enable_if_t<IsReference<JniType<T>>::value, U>;

// Converts a size_t to jsize with saturation at INT32_MAX to prevent negative
// wraparound on 64-bit platforms.
inline constexpr jsize SizeToJniSize(size_t value) noexcept {
  constexpr size_t kMaxJSize =
      static_cast<size_t>(std::numeric_limits<jsize>::max());
  return value > kMaxJSize ? std::numeric_limits<jsize>::max()
                           : static_cast<jsize>(value);
}

// 1. Raw JNI primitive types pass through unchanged (including jboolean).
template <typename T, typename = std::enable_if_t<IsPrimitive<T>::value>>
inline constexpr T RankedToJni(T value, ConverterChoice<0>) noexcept {
  return value;
}

// 2. C++ bool converts to jboolean (JNI_TRUE / JNI_FALSE).
template <typename T, typename = std::enable_if_t<std::is_same_v<T, bool>>>
inline constexpr jboolean RankedToJni(T value, ConverterChoice<1>) noexcept {
  return value ? JNI_TRUE : JNI_FALSE;
}

// 3. C++ size_t saturates to jsize (INT32_MAX).
template <typename T, typename = std::enable_if_t<std::is_same_v<T, size_t>>>
inline constexpr jsize RankedToJni(T value, ConverterChoice<1>) noexcept {
  return SizeToJniSize(value);
}

// 4. Other C++ primitive types (e.g., char16_t, long) cast to JniType<T>.
template <typename T,
          typename = std::enable_if_t<IsPrimitive<JniType<T>>::value &&
                                      !IsPrimitive<T>::value &&
                                      !std::is_same_v<T, bool> &&
                                      !std::is_same_v<T, size_t>>>
inline constexpr JniType<T> RankedToJni(T value, ConverterChoice<1>) noexcept {
  return static_cast<JniType<T>>(value);
}

// 5. Pointer conversions for array region operations.
inline jboolean* RankedToJni(bool* value, ConverterChoice<0>) noexcept {
  static_assert(sizeof(bool) == sizeof(jboolean),
                "bool and jboolean must have identical size");
  return reinterpret_cast<jboolean*>(value);
}

inline const jboolean* RankedToJni(const bool* value,
                                   ConverterChoice<0>) noexcept {
  static_assert(sizeof(bool) == sizeof(jboolean),
                "bool and jboolean must have identical size");
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

// 6. Raw JNI reference types and nullptr pass through.
inline constexpr jobject RankedToJni(std::nullptr_t,
                                     ConverterChoice<2>) noexcept {
  return nullptr;
}

template <typename T, typename = std::enable_if_t<IsReference<T>::value>>
inline constexpr T RankedToJni(T value, ConverterChoice<3>) noexcept {
  return value;
}

// 7. Wrapper types (Object, Local<T>, Global<T>, etc.) unwrap via .get().
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

// Validates a single JNI field/type descriptor prefix at the start of `sig`
// and sets `*consumed` to the number of bytes consumed.
inline constexpr bool ValidateJniFieldDescriptorPrefix(
    std::string_view sig, size_t* consumed) noexcept {
  if (sig.empty()) return false;
  size_t i = 0;
  while (i < sig.size() && sig[i] == '[') {
    ++i;
  }
  if (i >= sig.size()) return false;
  const char c = sig[i];
  if (c == 'B' || c == 'C' || c == 'D' || c == 'F' || c == 'I' || c == 'J' ||
      c == 'S' || c == 'Z') {
    ++i;
    if (consumed) *consumed = i;
    return true;
  }
  if (c == 'L') {
    ++i;
    const size_t start = i;
    while (i < sig.size() && sig[i] != ';') {
      // Reject illegal characters in internal JVM class descriptors (notably
      // '.' which must be '/' in JNI descriptors).
      if (sig[i] == '.' || sig[i] == '[' || sig[i] == '(' || sig[i] == ')' ||
          sig[i] == '\0') {
        return false;
      }
      ++i;
    }
    if (i >= sig.size() || i == start) return false;
    ++i;  // consume ';'
    if (consumed) *consumed = i;
    return true;
  }
  return false;
}

}  // namespace internal

// Compile-time validator for a JNI field descriptor (e.g. "I",
// "Ljava/lang/String;", "[B"). When `consumed == nullptr`, requires that the
// entire `sig` string is a single valid field descriptor with no trailing
// characters.
inline constexpr bool ValidateJniFieldDescriptor(
    std::string_view sig, size_t* consumed = nullptr) noexcept {
  size_t step = 0;
  if (!internal::ValidateJniFieldDescriptorPrefix(sig, &step)) {
    return false;
  }
  if (consumed) {
    *consumed = step;
    return true;
  }
  return step == sig.size();
}

// Compile-time validator for a JNI method signature (e.g.
// "(Ljava/lang/String;I)V").
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
  ++pos;  // consume ')'
  const std::string_view ret = sig.substr(pos);
  if (ret == "V") return true;
  return ValidateJniFieldDescriptor(ret);
}

inline constexpr bool ValidateJniSignature(std::string_view sig) noexcept {
  if (sig.empty()) return false;
  if (sig[0] == '(') return ValidateJniMethodSignature(sig);
  return ValidateJniFieldDescriptor(sig);
}

}  // namespace jni
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_JNI_TRAITS_H_
