#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_MACROS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_MACROS_H_

#include <cstdlib>

// A collection of macros that really should come from elsewhere but are
// required for compatibility with STLPort.
//
// TODO(b/163140650): Remove these and just use the Abseil equivalents.

#if !defined(__ANDROID__)
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#endif

// Matching declarations from absl/base/config.h

// FIRESTORE_HAVE_EXCEPTIONS
//
// Checks whether the compiler both supports and enables exceptions. Many
// compilers support a "no exceptions" mode that disables exceptions.
//
// Discover if exceptions are enabled and define them as needed.
//
// TODO(b/163140650): Consider ABSL_HAVE_EXCEPTIONS after dropping STLPort.
// This will require re-validating the results of `GetFullCompilerInfo()` on all
// supported platforms.
#if defined(__clang__)
#if defined(__EXCEPTIONS) && __has_feature(cxx_exceptions)
#define FIRESTORE_HAVE_EXCEPTIONS 1
#endif  // defined(__EXCEPTIONS) && __has_feature(cxx_exceptions)

#elif defined(_MSC_VER)
#if defined(_CPPUNWIND)
#define FIRESTORE_HAVE_EXCEPTIONS 1
#endif  // defined(_CPPUNWIND)

#elif defined(__GNUC__)
#if (__GNUC__ < 5) && defined(__EXCEPTIONS)
#define FIRESTORE_HAVE_EXCEPTIONS 1
#elif (__GNUC__ >= 5) && defined(__cpp_exceptions)
#define FIRESTORE_HAVE_EXCEPTIONS 1
#endif  // (__GNUC__ >= 5) && defined(__cpp_exceptions)

#elif defined(__cpp_exceptions)
// This should work in increasingly more and more compilers.
// https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations
#define FIRESTORE_HAVE_EXCEPTIONS 1
#endif  // FIRESTORE_HAVE_EXCEPTIONS

// Matching declarations from absl/base/attributes.h

// ABSL_ATTRIBUTE_NORETURN
//
// Tells the compiler that a given function never returns.
#if defined(__ANDROID__)
#define FIRESTORE_ATTRIBUTE_NORETURN __attribute__((noreturn))
#else
#define FIRESTORE_ATTRIBUTE_NORETURN ABSL_ATTRIBUTE_NORETURN
#endif  // defined(__ANDROID__)

// Matching declarations from absl/base/optimization.h

// FIRESTORE_PREDICT_TRUE, FIRESTORE_PREDICT_FALSE
//
// Enables the compiler to prioritize compilation using static analysis for
// likely paths within a boolean branch.
//
// See Abseil equivalents for examples and recommendations.
#if defined(__ANDROID__)
#define FIRESTORE_PREDICT_FALSE(x) (__builtin_expect(false || (x), false))
#define FIRESTORE_PREDICT_TRUE(x) (__builtin_expect(false || (x), true))
#else
#define FIRESTORE_PREDICT_FALSE(x) ABSL_PREDICT_FALSE(x)
#define FIRESTORE_PREDICT_TRUE(x) ABSL_PREDICT_TRUE(x)
#endif

// Other macros.

/**
 * Indicates a statement that cannot be reached. Should the program actually
 * reach this statement, the behavior is undefined, and the compiler may
 * optimize accordingly.
 *
 * Use this to suppress errors about not all paths returning a value after an
 * exhaustive switch statement or after calling a function that does not return
 * but isn't annotated as `noreturn`.
 */
#if defined(__GNUC__) || defined(__clang__)
#define FIRESTORE_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define FIRESTORE_UNREACHABLE() __assume(false)
#else
#define FIRESTORE_UNREACHABLE()  // nothing
#endif  // defined(__GNUC__) || defined(__clang__)

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_MACROS_H_
