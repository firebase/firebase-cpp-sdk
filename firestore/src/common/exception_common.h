#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_EXCEPTION_COMMON_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_EXCEPTION_COMMON_H_

// Routines in this file are used to throw an exception (or crash, depending on
// platform) in response to API usage errors. Exceptions should only be used
// for programmer errors made by consumers of the SDK, e.g. invalid method
// arguments.
//
// These routines avoid conditional compilation in the caller and avoids lint
// warnings around actually throwing exceptions in source. The implementation
// chooses the best way to surface a logic error to the developer.
//
// For recoverable runtime errors, use util::Status, or in pure Objective-C
// code use an NSError** out-parameter.
//
// For internal programming errors, including internal argument checking, use
// HARD_ASSERT or HARD_FAIL().
//
// TODO(b/163140650): Remove this/unify with the iOS implementation.
// On Android we still support customers building with STLPort, which precludes
// use of Abseil here.

#include <string>

#include "firestore/src/common/macros.h"

#if !defined(__ANDROID__)
#include "Firestore/core/src/util/exception.h"
#endif

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)

// This namespace matches the iOS declarations.
namespace util {

/**
 * An enumeration of logical exception types. Each of these types maps to a
 * common user visible exception we might throw in response do some invalid
 * action in an interaction with the Firestore API.
 */
enum class ExceptionType {
  AssertionFailure,
  IllegalState,
  InvalidArgument,
};

/**
 * Throws an exception of the correct type or crashes, as appropriate for the
 * platform. Implementations of `ThrowHandler` must tolerate null/zero values
 * for `file`, `func`, and `line`.
 */
using ThrowHandler = void (*)(ExceptionType type, const char* file,
                              const char* func, int line,
                              const std::string& message);

/**
 * Overrides the default exception throw handler.
 *
 * The default essentially just calls std::terminate. While reasonable for C++
 * with exceptions disabled, this isn't optimal for platforms that merely use
 * the C++ core as their implementation and would otherwise be expected to throw
 * a platform specific exception.
 *
 * @param handler A function that will handle the exception. This function is
 *     expected not to return. (If it does, std::terminate() will be called
 *     immediately after it does so.)
 * @return A pointer to the previous throw handler.
 */
ThrowHandler SetThrowHandler(ThrowHandler handler);

FIRESTORE_ATTRIBUTE_NORETURN void Throw(ExceptionType exception,
                                        const char* file, const char* func,
                                        int line, const std::string& message);

}  // namespace util

#endif  // defined(__ANDROID__)

/**
 * Throws an exception without any string formatting indicating that the user
 * passed an invalid argument.
 *
 * Invalid argument is interpreted pretty broadly and can mean that the user
 * made an incompatible chained method call while building up a larger
 * structure, like a query.
 */
FIRESTORE_ATTRIBUTE_NORETURN void SimpleThrowInvalidArgument(
    const std::string& message);

/**
 * Throws an exception without any string formatting that indicates the user has
 * attempted to use an API that's in an illegal state, usually by violating a
 * precondition of the API call.
 *
 * Good uses of these are things like using a write batch after committing or
 * trying to use Firestore without initializing FIRApp. Builder-style APIs that
 * haven't done anything yet should likely just stick to
 * SimpleThrowInvalidArgument.
 */
FIRESTORE_ATTRIBUTE_NORETURN void SimpleThrowIllegalState(
    const std::string& message);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_EXCEPTION_COMMON_H_
