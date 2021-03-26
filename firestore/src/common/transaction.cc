#include "firestore/src/include/firebase/firestore/transaction.h"

#include "app/src/assert.h"
#include "firestore/src/common/cleanup.h"

#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#if defined(__ANDROID__)
#include "firestore/src/android/transaction_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/transaction_stub.h"
#else
#include "firestore/src/ios/transaction_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnTransaction = CleanupFn<Transaction>;

Transaction::Transaction(TransactionInternal* internal) : internal_(internal) {
  FIREBASE_ASSERT(internal != nullptr);
  CleanupFnTransaction::Register(this, internal_);
}

Transaction::~Transaction() {
  CleanupFnTransaction::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

void Transaction::Set(const DocumentReference& document,
                      const MapFieldValue& data, const SetOptions& options) {
  if (!internal_) return;
  internal_->Set(document, data, options);
}

void Transaction::Update(const DocumentReference& document,
                         const MapFieldValue& data) {
  if (!internal_) return;
  internal_->Update(document, data);
}

void Transaction::Update(const DocumentReference& document,
                         const MapFieldPathValue& data) {
  if (!internal_) return;
  internal_->Update(document, data);
}

void Transaction::Delete(const DocumentReference& document) {
  if (!internal_) return;
  internal_->Delete(document);
}

DocumentSnapshot Transaction::Get(const DocumentReference& document,
                                  Error* error_code,
                                  std::string* error_message) {
  if (!internal_) return {};
  return internal_->Get(document, error_code, error_message);
}

}  // namespace firestore
}  // namespace firebase
