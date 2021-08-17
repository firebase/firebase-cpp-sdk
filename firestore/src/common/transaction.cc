/*
 * Copyright 2020 Google LLC
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

#include "firestore/src/include/firebase/firestore/transaction.h"

#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#if defined(__ANDROID__)
#include "firestore/src/android/transaction_android.h"
#else
#include "firestore/src/main/transaction_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

namespace {

using CleanupFnTransaction = CleanupFn<Transaction>;

void ValidateReference(const DocumentReference& document) {
  if (!document.is_valid()) {
    SimpleThrowInvalidArgument("Invalid document reference provided.");
  }
}

}  // namespace

Transaction::Transaction(TransactionInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnTransaction::Register(this, internal_);
}

Transaction::~Transaction() {
  CleanupFnTransaction::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

void Transaction::Set(const DocumentReference& document,
                      const MapFieldValue& data,
                      const SetOptions& options) {
  if (!internal_) return;

  ValidateReference(document);
  internal_->Set(document, data, options);
}

void Transaction::Update(const DocumentReference& document,
                         const MapFieldValue& data) {
  if (!internal_) return;

  ValidateReference(document);
  internal_->Update(document, data);
}

void Transaction::Update(const DocumentReference& document,
                         const MapFieldPathValue& data) {
  if (!internal_) return;

  ValidateReference(document);
  internal_->Update(document, data);
}

void Transaction::Delete(const DocumentReference& document) {
  if (!internal_) return;

  ValidateReference(document);
  internal_->Delete(document);
}

DocumentSnapshot Transaction::Get(const DocumentReference& document,
                                  Error* error_code,
                                  std::string* error_message) {
  if (!internal_) return {};

  ValidateReference(document);
  return internal_->Get(document, error_code, error_message);
}

}  // namespace firestore
}  // namespace firebase
