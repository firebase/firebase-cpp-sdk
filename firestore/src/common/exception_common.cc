// Copyright 2021 Google LLC
#include "firestore/src/common/exception_common.h"

#include <atomic>
#include <sstream>
#if FIRESTORE_HAVE_EXCEPTIONS
#include <stdexcept>
#endif
#include "app/src/log.h"
#if !defined(__ANDROID__)
#include "Firestore/core/src/util/firestore_exceptions.h"
#else
#include "firestore/src/android/firestore_exceptions_android.h"
#endif

namespace firebase::firestore {

#if defined(__ANDROID__)
namespace util {
namespace {

const char* ExceptionName(ExceptionType exception) {
  switch (exception) {
    case ExceptionType::AssertionFailure: return "FIRESTORE INTERNAL ASSERTION FAILED";
    case ExceptionType::IllegalState: return "Illegal state";
    case ExceptionType::InvalidArgument: return "Invalid argument";
  }
  FIRESTORE_UNREACHABLE();
}

FIRESTORE_ATTRIBUTE_NORETURN void DefaultThrowHandler(
    ExceptionType type, const char* file, const char* func, int line,
    const std::string& message) {
  std::ostringstream what;
  what << ExceptionName(type) << ": ";
  if (file && func) what << file << "(" << line << ") " << func << ": ";
  what << message;
  std::string what_str = what.str();
  LogError("%s", what_str.c_str());
#if FIRESTORE_HAVE_EXCEPTIONS
  switch (type) {
    case ExceptionType::AssertionFailure: throw FirestoreInternalError(what_str);
    case ExceptionType::IllegalState: throw std::logic_error(message);
    case ExceptionType::InvalidArgument: throw std::invalid_argument(message);
  }
#else
  std::terminate();
#endif
  FIRESTORE_UNREACHABLE();
}

std::atomic<ThrowHandler> throw_handler{DefaultThrowHandler};

}  // namespace

ThrowHandler SetThrowHandler(ThrowHandler handler) {
  return throw_handler.exchange(handler ? handler : DefaultThrowHandler);
}

FIRESTORE_ATTRIBUTE_NORETURN void Throw(ExceptionType exception, const char* file,
                                        const char* func, int line,
                                        const std::string& message) {
  if (ThrowHandler handler = throw_handler.load()) {
    handler(exception, file, func, line, message);
  }
  std::terminate();
}

}  // namespace util
#endif

using ::firebase::firestore::util::ExceptionType;
using ::firebase::firestore::util::Throw;

FIRESTORE_ATTRIBUTE_NORETURN void SimpleThrowInvalidArgument(const std::string& message) {
  Throw(ExceptionType::InvalidArgument, nullptr, nullptr, 0, message);
}

FIRESTORE_ATTRIBUTE_NORETURN void SimpleThrowIllegalState(const std::string& message) {
  Throw(ExceptionType::IllegalState, nullptr, nullptr, 0, message);
}

}  // namespace firebase::firestore
