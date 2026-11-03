/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_TYPE_TRAITS_IOS_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_TYPE_TRAITS_IOS_H_

#include <type_traits>

#if defined(__OBJC__)
#import <objc/objc.h>
#endif  // defined(__OBJC__)

namespace firebase {
namespace internal {

namespace detail {

template <typename T>
struct IsObjCBlockImpl : std::false_type {};

#if defined(__BLOCKS__)
#define FIREBASE_HAS_BLOCKS 1
#elif defined(__has_feature)
#if __has_feature(blocks)
#define FIREBASE_HAS_BLOCKS 1
#endif
#endif

#if defined(FIREBASE_HAS_BLOCKS)
template <typename Ret, typename... Args>
struct IsObjCBlockImpl<Ret (^)(Args...)> : std::true_type {};

template <typename Ret, typename... Args>
struct IsObjCBlockImpl<Ret (^)(Args...) noexcept> : std::true_type {};

template <typename Ret, typename... Args>
struct IsObjCBlockImpl<Ret (^)(Args..., ...)> : std::true_type {};

template <typename Ret, typename... Args>
struct IsObjCBlockImpl<Ret (^)(Args..., ...) noexcept> : std::true_type {};
#endif

}  // namespace detail

// Detects Objective-C block types (e.g. `void (^)(NSError*)`), stripping
// top-level cv-qualifiers so `IsObjCBlock<T>::value == IsObjCBlockV<T>`.
template <typename T>
struct IsObjCBlock : detail::IsObjCBlockImpl<std::remove_cv_t<T>> {};

template <typename T>
inline constexpr bool IsObjCBlockV = IsObjCBlock<T>::value;

#if defined(__OBJC__)

// Detects Objective-C object pointer types (`id`, `NSObject*`, `id<Protocol>`,
// etc.) while excluding Objective-C blocks, C++ references, `std::nullptr_t`,
// raw C/C++ pointers (`void*`, `int*`), and C++ classes with user-defined
// conversion operators to `id`.
template <typename T>
struct IsObjCPointer
    : std::bool_constant<!std::is_reference_v<T> &&
                         std::is_pointer_v<T> &&
                         std::is_convertible_v<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>*, id>> {};

#else  // !defined(__OBJC__)

template <typename T>
struct IsObjCPointer : std::false_type {};

#endif  // defined(__OBJC__)

template <typename T>
inline constexpr bool IsObjCPointerV = IsObjCPointer<T>::value;

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_TYPE_TRAITS_IOS_H_
