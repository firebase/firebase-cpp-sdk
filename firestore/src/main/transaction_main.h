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

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

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
