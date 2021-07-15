/*
 * Copyright 2019 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_HARD_ASSERT_COMMON_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_HARD_ASSERT_COMMON_H_

// TODO(b/163140650): Remove this/unify with the iOS implementation.
// On Android we still support customers building with STLPort, which precludes
// use of Abseil here.

#include <string>
#include <utility>

#include "firestore/src/common/macros.h"

#if !defined(__ANDROID__)
#include "Firestore/core/src/util/hard_assert.h"
#endif  // !defined(__ANDROID__)

#if defined(__ANDROID__)

/**
 * Invokes the internal Fail function below with all the required contextual
 * information and passes additional arguments.
 *
 * @param message The failure message.
 * @param condition The string form of the expression that failed (optional)
 */
#define INVOKE_INTERNAL_FAIL(...)                     \
  firebase::firestore::util::internal::FailAssertion( \
      __FILE__, __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)

#endif  // !defined(__ANDROID__)

/**
 * Fails the current function if the given condition is false.
 *
 * Unlike assert(3) or NSAssert, this macro is never compiled out.
 *
 * Note: this version of `HARD_ASSERT` is deliberately simplified to avoid
 * using `util::StringFormat`.
 *
 * @param condition The condition to test.
 * @param message (optional) A message to print.
 */
#define SIMPLE_HARD_ASSERT(condition, ...)        \
  do {                                            \
    if (!FIRESTORE_PREDICT_TRUE(condition)) {     \
      std::string _message{__VA_ARGS__};          \
      INVOKE_INTERNAL_FAIL(_message, #condition); \
    }                                             \
  } while (0)

/**
 * Unconditionally fails the current function.
 *
 * Unlike assert(3) or NSAssert, this macro is never compiled out.
 *
 * @param message (optional) A message to print.
 */
#define SIMPLE_HARD_FAIL(...)          \
  do {                                 \
    std::string _failure{__VA_ARGS__}; \
    INVOKE_INTERNAL_FAIL(_failure);    \
  } while (0)

#if defined(__ANDROID__)

/**
 * Returns the given `ptr` if it is non-null; otherwise, results in a failed
 * assertion, similar to `HARD_ASSERT`. This macro deliberately expands to an
 * expression, so that it can be used in initialization and assignment:
 *
 *   my_ptr_ = NOT_NULL(suspicious_ptr);
 *   my_smart_ptr_ = std::move(NOT_NULL(suspicious_smart_ptr));
 *
 *   MyClass() : my_ptr_{NOT_NULL(suspicious_ptr)} {}
 *
 * @param ptr The pointer to check and return. Can be a smart pointer.
 */
#define NOT_NULL(ptr)                                                      \
  (static_cast<void>(FIRESTORE_PREDICT_FALSE((ptr) == nullptr)             \
                         ? INVOKE_INTERNAL_FAIL("Expected non-null " #ptr) \
                         : static_cast<void>(0)),                          \
   (ptr))  // NOLINT(whitespace/indent)

namespace firebase {
namespace firestore {
namespace util {
namespace internal {

// A no-return helper function. To raise an assertion, use Macro instead.
// These symbols are in the util::internal namespace to match their iOS
// equivalents.
FIRESTORE_ATTRIBUTE_NORETURN void FailAssertion(const char* file,
                                                const char* func,
                                                int line,
                                                const std::string& message);

FIRESTORE_ATTRIBUTE_NORETURN void FailAssertion(const char* file,
                                                const char* func,
                                                int line,
                                                const std::string& message,
                                                const char* condition);

}  // namespace internal
}  // namespace util
}  // namespace firestore
}  // namespace firebase

#endif  // defined(__ANDROID__)

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_HARD_ASSERT_COMMON_H_
