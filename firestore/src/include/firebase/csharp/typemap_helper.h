#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TYPEMAP_HELPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TYPEMAP_HELPER_H_

#include <string>
#include <utility>

#include "firebase/firestore/document_reference.h"
#include "firebase/firestore/document_snapshot.h"
#include "firebase/firestore/transaction.h"
#include "firebase/firestore/firestore_errors.h"

// We add helpers to do the type-mapping in C++ code instead of doing this in
// SWIG. We want to minimize the SWIG code, which is hard to understand and
// debug.

namespace firebase {
namespace firestore {
namespace csharp {

class TransactionGetResult {
 public:
  TransactionGetResult(DocumentSnapshot snapshot, Error error_code,
                       std::string error_message)
      : snapshot_(std::move(snapshot)),
        error_code_(error_code),
        error_message_(std::move(error_message)) {}

  DocumentSnapshot TakeSnapshot() { return std::move(snapshot_); }

  Error error_code() const { return error_code_; }

  const std::string& error_message() const { return error_message_; }

 private:
  DocumentSnapshot snapshot_;
  Error error_code_ = Error::kErrorUnknown;
  std::string error_message_;
};

TransactionGetResult TransactionGet(Transaction* transaction,
                                    const DocumentReference& document) {
  Error error_code = Error::kErrorUnknown;
  std::string error_message;
  DocumentSnapshot snapshot =
      transaction->Get(document, &error_code, &error_message);
  return TransactionGetResult(std::move(snapshot), error_code,
                              std::move(error_message));
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TYPEMAP_HELPER_H_
