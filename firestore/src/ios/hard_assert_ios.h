/*
 * Copyright 2019 Google
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

#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_HARD_ASSERT_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_HARD_ASSERT_IOS_H_

#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "Firestore/core/src/util/exception.h"

// TODO(b/147444199): delete this file and use the one that comes from the
// GitHub repo. This file provides simplified versions of `HARD_ASSERT`,
// `HARD_FAIL`, and `ThrowInvalidArgument` that don't support string formatting.

#if defined(_MSC_VER)
#define FIRESTORE_FUNCTION_NAME __FUNCSIG__
#else
#define FIRESTORE_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

/**
 * Invokes the internal Fail function below with all the required contextual
 * information and passes additional arguments.
 *
 * @param message The failure message.
 * @param condition The string form of the expression that failed (optional)
 */
#define INVOKE_INTERNAL_FAIL(...)                     \
  firebase::firestore::util::internal::FailAssertion( \
      __FILE__, FIRESTORE_FUNCTION_NAME, __LINE__, __VA_ARGS__)

/**
 * Fails the current function if the given condition is false.
 *
 * Unlike assert(3) or NSAssert, this macro is never compiled out.
 *
 * @param condition The condition to test.
 * @param message (optional) A message to print.
 */

// Note: this version of `HARD_ASSERT` is deliberately dumbed down to avoid
// using `util::StringFormat`.
#define HARD_ASSERT_IOS(condition, ...)           \
  do {                                            \
    if (!ABSL_PREDICT_TRUE(condition)) {          \
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
#define HARD_FAIL_IOS(...)             \
  do {                                 \
    std::string _failure{__VA_ARGS__}; \
    INVOKE_INTERNAL_FAIL(_failure);    \
  } while (0)

/**
 * Indicates an area of the code that cannot be reached (except possibly due to
 * undefined behaviour or other similar badness). The only reasonable thing to
 * do in these cases is to immediately abort.
 */
#define UNREACHABLE() abort()

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
  (static_cast<void>(ABSL_PREDICT_FALSE((ptr) == nullptr)                  \
                         ? INVOKE_INTERNAL_FAIL("Expected non-null " #ptr) \
                         : static_cast<void>(0)),                          \
   (ptr))  // NOLINT(whitespace/indent)

namespace firebase {
namespace firestore {
namespace util {
namespace internal {

// A no-return helper function. To raise an assertion, use Macro instead.
ABSL_ATTRIBUTE_NORETURN void FailAssertion(const char* file, const char* func,
                                           int line,
                                           const std::string& message);

ABSL_ATTRIBUTE_NORETURN void FailAssertion(const char* file, const char* func,
                                           int line, const std::string& message,
                                           const char* condition);

}  // namespace internal

// This is a workaround for the fact that `util::ThrowInvalidArgument` calls
// `StringFormat` in its implementation, which leads to linkage problems due to
// the use of `absl::string_view` in its signature.
ABSL_ATTRIBUTE_NORETURN void ThrowInvalidArgumentIos(
    const std::string& message);

}  // namespace util
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_HARD_ASSERT_IOS_H_
