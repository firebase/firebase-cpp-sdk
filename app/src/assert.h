/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_ASSERT_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_ASSERT_H_

#include <cstdlib>

#include "app/src/log.h"

#define FIREBASE_EXPAND_STRINGIFY_(X) #X
#define FIREBASE_EXPAND_STRINGIFY(X) FIREBASE_EXPAND_STRINGIFY_(X)

// Only include file and line number in debug build assert messages.
#if defined(NDEBUG)
#define FIREBASE_ASSERT_MESSAGE_PREFIX
#else
#define FIREBASE_ASSERT_MESSAGE_PREFIX \
  __FILE__ "(" FIREBASE_EXPAND_STRINGIFY(__LINE__) "): "
#endif  // defined(NDEBUG)

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

// FIREBASE_ASSERT_* macros are not compiled out of release builds. They should
// be used for assertions that need to be propagated to end-users of SDKs.
// FIREBASE_DEV_ASSERT_* macros are compiled out of release builds, similar to
// the C assert() macro. They should be used for internal assertions that are
// only shown to SDK developers.

// Assert condition is true, if it's false log an assert with the specified
// expression as a string.
#define FIREBASE_ASSERT_WITH_EXPRESSION(condition, expression)      \
  do {                                                              \
    if (!(condition)) {                                             \
      FIREBASE_NAMESPACE::LogAssert(                                \
          FIREBASE_ASSERT_MESSAGE_PREFIX FIREBASE_EXPAND_STRINGIFY( \
              expression));                                         \
    }                                                               \
  } while (false)

// Assert condition is true, if it's false log an assert with the specified
// expression as a string. Compiled out of release builds.
#if !defined(NDEBUG)
#define FIREBASE_DEV_ASSERT_WITH_EXPRESSION(condition, expression) \
  FIREBASE_ASSERT_WITH_EXPRESSION(condition, expression)
#else
#define FIREBASE_DEV_ASSERT_WITH_EXPRESSION(condition, expression) \
  (void)(condition)
#endif  // !defined(NDEBUG)

// Custom assert() implementation that is not compiled out in release builds.
#define FIREBASE_ASSERT(expression) \
  FIREBASE_ASSERT_WITH_EXPRESSION(expression, expression)

// Custom assert() implementation that is compiled out in release builds.
// Compiled out of release builds.
#define FIREBASE_DEV_ASSERT(expression) \
  FIREBASE_DEV_ASSERT_WITH_EXPRESSION(expression, expression)

// Assert that returns a value if the log operation doesn't abort the program.
#define FIREBASE_ASSERT_RETURN(return_value, expression)    \
  {                                                         \
    bool condition = !(!(expression));                      \
    FIREBASE_ASSERT_WITH_EXPRESSION(condition, expression); \
    if (!condition) return (return_value);                  \
  }

// Assert that returns a value if the log operation doesn't abort the program.
// Compiled out of release builds.
#if !defined(NDEBUG)
#define FIREBASE_DEV_ASSERT_RETURN(return_value, expression) \
  FIREBASE_ASSERT_RETURN(return_value, expression)
#else
#define FIREBASE_DEV_ASSERT_RETURN(return_value, expression) \
  { (void)(expression); }
#endif  // !defined(NDEBUG)

// Assert that returns if the log operation doesn't abort the program.
#define FIREBASE_ASSERT_RETURN_VOID(expression)             \
  {                                                         \
    bool condition = !(!(expression));                      \
    FIREBASE_ASSERT_WITH_EXPRESSION(condition, expression); \
    if (!condition) return;                                 \
  }

// Assert that returns if the log operation doesn't abort the program.
// Compiled out of release builds.
#if !defined(NDEBUG)
#define FIREBASE_DEV_ASSERT_RETURN_VOID(expression) \
  FIREBASE_ASSERT_RETURN_VOID(expression)
#else
#define FIREBASE_DEV_ASSERT_RETURN_VOID(expression) \
  { (void)(expression); }
#endif  // !defined(NDEBUG)

// Assert condition is true otherwise display the specified expression,
// message and abort.
#define FIREBASE_ASSERT_MESSAGE_WITH_EXPRESSION(condition, expression, ...) \
  do {                                                                      \
    if (!(condition)) {                                                     \
      FIREBASE_NAMESPACE::LogError(                                         \
          FIREBASE_ASSERT_MESSAGE_PREFIX FIREBASE_EXPAND_STRINGIFY(         \
              expression));                                                 \
      FIREBASE_NAMESPACE::LogAssert(__VA_ARGS__);                           \
    }                                                                       \
  } while (false)

// Assert condition is true otherwise display the specified expression,
// message and abort. Compiled out of release builds.
#if !defined(NDEBUG)
#define FIREBASE_DEV_ASSERT_MESSAGE_WITH_EXPRESSION(condition, expression, \
                                                    ...)                   \
  FIREBASE_ASSERT_MESSAGE_WITH_EXPRESSION(condition, expression, __VA_ARGS__)
#else
#define FIREBASE_DEV_ASSERT_MESSAGE_WITH_EXPRESSION(condition, expression, \
                                                    ...)                   \
  { (void)(condition); }
#endif  // !defined(NDEBUG)

// Assert expression is true otherwise display the specified message and
// abort.
#define FIREBASE_ASSERT_MESSAGE(expression, ...) \
  FIREBASE_ASSERT_MESSAGE_WITH_EXPRESSION(expression, expression, __VA_ARGS__)

// Assert expression is true otherwise display the specified message and
// abort. Compiled out of release builds.
#define FIREBASE_DEV_ASSERT_MESSAGE(expression, ...)                  \
  FIREBASE_DEV_ASSERT_MESSAGE_WITH_EXPRESSION(expression, expression, \
                                              __VA_ARGS__)

// Assert expression is true otherwise display the specified message and
// abort or return a value if the log operation doesn't abort.
#define FIREBASE_ASSERT_MESSAGE_RETURN(return_value, expression, ...) \
  {                                                                   \
    bool condition = !(!(expression));                                \
    FIREBASE_ASSERT_MESSAGE_WITH_EXPRESSION(condition, expression,    \
                                            __VA_ARGS__);             \
    if (!condition) return (return_value);                            \
  }

// Assert expression is true otherwise display the specified message and
// abort or return a value if the log operation doesn't abort. Compiled out of
// release builds.
#if !defined(NDEBUG)
#define FIREBASE_DEV_ASSERT_MESSAGE_RETURN(return_value, expression, ...) \
  FIREBASE_ASSERT_MESSAGE_RETURN(return_value, expression, __VA_ARGS__)
#else
#define FIREBASE_DEV_ASSERT_MESSAGE_RETURN(return_value, expression, ...) \
  { (void)(expression); }
#endif  // !defined(NDEBUG)

// Assert expression is true otherwise display the specified message and
// abort or return if the log operation doesn't abort.
#define FIREBASE_ASSERT_MESSAGE_RETURN_VOID(expression, ...)       \
  {                                                                \
    bool condition = !(!(expression));                             \
    FIREBASE_ASSERT_MESSAGE_WITH_EXPRESSION(condition, expression, \
                                            __VA_ARGS__);          \
    if (!condition) return;                                        \
  }

// Assert expression is true otherwise display the specified message and
// abort or return if the log operation doesn't abort. Compiled out of release
// builds.
#if !defined(NDEBUG)
#define FIREBASE_DEV_ASSERT_MESSAGE_RETURN_VOID(expression, ...) \
  FIREBASE_ASSERT_MESSAGE_RETURN_VOID(expression, __VA_ARGS__)
#else
#define FIREBASE_DEV_ASSERT_MESSAGE_RETURN_VOID(expression, ...) \
  { (void)(expression); }
#endif  // !defined(NDEBUG)

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_ASSERT_H_
