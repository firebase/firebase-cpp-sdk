#include "firestore/src/tests/util/future_test_util.h"

#include <ostream>

#include "firestore/src/tests/firestore_integration_test.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {

namespace {

void PrintTo(std::ostream* os, FutureStatus future_status, int error,
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
