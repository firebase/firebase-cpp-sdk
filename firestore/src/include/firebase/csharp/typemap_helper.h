#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TYPEMAP_HELPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TYPEMAP_HELPER_H_

#include <string>

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

struct TransactionGetResult {
  DocumentSnapshot snapshot;
  Error error_code = Error::kUnknown;
  std::string error_message;
};

TransactionGetResult TransactionGet(Transaction* transaction,
                                    const DocumentReference& document) {
  TransactionGetResult result;
  result.snapshot =
      transaction->Get(document, &result.error_code, &result.error_message);
  return result;
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_TYPEMAP_HELPER_H_
