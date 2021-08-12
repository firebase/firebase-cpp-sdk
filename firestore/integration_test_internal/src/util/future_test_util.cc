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

#include "util/future_test_util.h"

#include <ostream>

#include "firebase/firestore/firestore_errors.h"
#include "firestore_integration_test.h"

namespace firebase {

namespace {

void PrintTo(std::ostream* os,
             FutureStatus future_status,
             int error,
             const char* error_message = nullptr) {
  *os << "Future<void>{status=" << ToEnumeratorName(future_status)
      << " error=" << firestore::ToFirestoreErrorCodeName(error);
  if (error_message != nullptr) {
    *os << " error_message=" << error_message;
  }
  *os << "}";
}

class FutureSucceedsImpl
    : public testing::MatcherInterface<const Future<void>&> {
 public:
  void DescribeTo(std::ostream* os) const override {
    PrintTo(os, FutureStatus::kFutureStatusComplete,
            firestore::Error::kErrorOk);
  }

  bool MatchAndExplain(const Future<void>& future,
                       testing::MatchResultListener*) const override {
    firestore::WaitFor(future);
    return future.status() == FutureStatus::kFutureStatusComplete &&
           future.error() == firestore::Error::kErrorOk;
  }
};

}  // namespace

std::string ToEnumeratorName(FutureStatus status) {
  switch (status) {
    case kFutureStatusComplete:
      return "kFutureStatusComplete";
    case kFutureStatusPending:
      return "kFutureStatusPending";
    case kFutureStatusInvalid:
      return "kFutureStatusInvalid";
    default:
      return "[invalid FutureStatus]";
  }
}

void PrintTo(const Future<void>& future, std::ostream* os) {
  PrintTo(os, future.status(), future.error(), future.error_message());
}

testing::Matcher<const Future<void>&> FutureSucceeds() {
  return testing::Matcher<const Future<void>&>(new FutureSucceedsImpl());
}

}  // namespace firebase
