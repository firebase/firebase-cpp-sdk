/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/common/exception_common.h"

#include <sstream>

#if FIRESTORE_HAVE_EXCEPTIONS
#include <stdexcept>
#endif  // FIRESTORE_HAVE_EXCEPTIONS

#include "app/src/log.h"
#if !defined(__ANDROID__)
#include "Firestore/core/src/util/firestore_exceptions.h"
#else
#include "firestore/src/android/firestore_exceptions_android.h"
#endif

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)

namespace util {
namespace {

const char* ExceptionName(ExceptionType exception) {
  switch (exception) {
    case ExceptionType::AssertionFailure:
      return "FIRESTORE INTERNAL ASSERTION FAILED";
    case ExceptionType::IllegalState:
      return "Illegal state";
    case ExceptionType::InvalidArgument:
      return "Invalid argument";
  }
  FIRESTORE_UNREACHABLE();
}

FIRESTORE_ATTRIBUTE_NORETURN void DefaultThrowHandler(
    ExceptionType type,
    const char* file,
    const char* func,
    int line,
    const std::string& message) {
  std::ostringstream what;
  what << ExceptionName(type) << ": ";
  if (file && func) {
    what << file << "(" << line << ") " << func << ": ";
  }
  what << message;

#if FIRESTORE_HAVE_EXCEPTIONS
  // TODO(b/149105903): Remove this once SWIG maps C++ exceptions to C#.
  //
  // This is a temporary workaround for Unity: if the throw handler is
  // triggered, Unity log would end up containing the stack trace but not the
  // exception message. Until we implement better error handling in Unity, just
  // log the message as a quick-and-dirty solution.
  std::string what_str = what.str();
  LogError("%s", what_str.c_str());

  switch (type) {
    case ExceptionType::AssertionFailure:
      throw FirestoreInternalError(what_str);
    case ExceptionType::IllegalState:
      // Omit descriptive text since the type already encodes the kind of error.
      throw std::logic_error(message);
    case ExceptionType::InvalidArgument:
      // Omit descriptive text since the type already encodes the kind of error.
      throw std::invalid_argument(message);
  }
#else
  LogError("%s", what.str().c_str());
  std::terminate();
#endif  // FIRESTORE_HAVE_EXCEPTIONS

  FIRESTORE_UNREACHABLE();
}

ThrowHandler throw_handler = DefaultThrowHandler;

}  // namespace

ThrowHandler SetThrowHandler(ThrowHandler handler) {
  ThrowHandler previous = throw_handler;
  throw_handler = handler;
  return previous;
}

FIRESTORE_ATTRIBUTE_NORETURN void Throw(ExceptionType exception,
                                        const char* file,
                                        const char* func,
                                        int line,
                                        const std::string& message) {
  throw_handler(exception, file, func, line, message);

  // It's expected that the throw handler above does not return. If it does,
  // just terminate.
  std::terminate();
}

}  // namespace util

#endif  // defined(__ANDROID__)

using ::firebase::firestore::util::ExceptionType;
using ::firebase::firestore::util::Throw;

FIRESTORE_ATTRIBUTE_NORETURN void SimpleThrowInvalidArgument(
    const std::string& message) {
  Throw(ExceptionType::InvalidArgument, nullptr, nullptr, 0, message);
}

FIRESTORE_ATTRIBUTE_NORETURN void SimpleThrowIllegalState(
    const std::string& message) {
  Throw(ExceptionType::IllegalState, nullptr, nullptr, 0, message);
}

}  // namespace firestore
}  // namespace firebase
