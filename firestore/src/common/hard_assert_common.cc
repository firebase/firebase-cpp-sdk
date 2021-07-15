// Copyright 2021 Google LLC

#include "firestore/src/common/hard_assert_common.h"

#include "firestore/src/common/exception_common.h"

namespace firebase {
namespace firestore {
namespace util {
namespace internal {

#if defined(__ANDROID__)

void FailAssertion(const char* file,
                   const char* func,
                   const int line,
                   const std::string& message) {
  Throw(ExceptionType::AssertionFailure, file, func, line, message);
}

void FailAssertion(const char* file,
                   const char* func,
                   const int line,
                   const std::string& message,
                   const char* condition) {
  std::string failure;
  if (message.empty()) {
    failure = condition;
  } else {
    failure = message + " (expected " + condition + ")";
  }
  Throw(ExceptionType::AssertionFailure, file, func, line, failure);
}

#endif  // defined(__ANDROID__)

}  // namespace internal
}  // namespace util
}  // namespace firestore
}  // namespace firebase
