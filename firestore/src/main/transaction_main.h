// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_TRANSACTION_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_TRANSACTION_MAIN_H_

#include <memory>
#include <string>

#include "Firestore/core/src/core/transaction.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/transaction.h"
#include "firestore/src/main/firestore_main.h"
#include "firestore/src/main/user_data_converter_main.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;

class TransactionInternal {
 public:
  TransactionInternal(std::shared_ptr<core::Transaction> transaction,
                      FirestoreInternal* firestore_internal);

  Firestore* firestore();
  FirestoreInternal* firestore_internal();

  void Set(const DocumentReference& document,
           const MapFieldValue& data,
           const SetOptions& options);

  void Update(const DocumentReference& document, const MapFieldValue& data);
  void Update(const DocumentReference& document, const MapFieldPathValue& data);

  void Delete(const DocumentReference& document);

  DocumentSnapshot Get(const DocumentReference& document,
                       Error* error_code,
                       std::string* error_message);

  void MarkPermanentlyFailed();

 private:
  void ValidateReference(const DocumentReference& document);

  std::shared_ptr<core::Transaction> transaction_;
  FirestoreInternal* firestore_internal_ = nullptr;
  UserDataConverter user_data_converter_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_TRANSACTION_MAIN_H_
