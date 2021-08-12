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
