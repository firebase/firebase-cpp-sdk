// Copyright 2020 Google LLC

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
  ValidateReference(document);

  if (!internal_) return;
  internal_->Set(document, data, options);
}

void Transaction::Update(const DocumentReference& document,
                         const MapFieldValue& data) {
  ValidateReference(document);

  if (!internal_) return;
  internal_->Update(document, data);
}

void Transaction::Update(const DocumentReference& document,
                         const MapFieldPathValue& data) {
  ValidateReference(document);

  if (!internal_) return;
  internal_->Update(document, data);
}

void Transaction::Delete(const DocumentReference& document) {
  ValidateReference(document);

  if (!internal_) return;
  internal_->Delete(document);
}

DocumentSnapshot Transaction::Get(const DocumentReference& document,
                                  Error* error_code,
                                  std::string* error_message) {
  ValidateReference(document);

  if (!internal_) return {};
  return internal_->Get(document, error_code, error_message);
}

}  // namespace firestore
}  // namespace firebase
