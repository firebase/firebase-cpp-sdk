#include "firestore/src/ios/transaction_ios.h"

#include <future>  // NOLINT(build/c++11)
#include <utility>

#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/document_reference_ios.h"
#include "firestore/src/ios/field_value_ios.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "firestore/src/ios/set_options_ios.h"
#include "firestore/src/ios/util_ios.h"
#include "absl/types/optional.h"
#include "Firestore/core/src/core/user_data.h"
#include "Firestore/core/src/model/document.h"
#include "Firestore/core/src/model/document_key.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/model/maybe_document.h"
#include "Firestore/core/src/util/status.h"
#include "Firestore/core/src/util/statusor.h"

namespace firebase {
namespace firestore {

namespace {

using core::ParsedSetData;
using core::ParsedUpdateData;
using model::Document;
using model::MaybeDocument;
using util::Status;
using util::StatusOr;

const model::DocumentKey& GetKey(const DocumentReference& document) {
  return GetInternal(&document)->key();
}

DocumentSnapshot ConvertToSingleSnapshot(
    const std::shared_ptr<api::Firestore>& firestore, model::DocumentKey key,
    const std::vector<MaybeDocument>& documents) {
  HARD_ASSERT_IOS(
      documents.size() == 1,
      "Expected core::Transaction::Lookup() to return a single document");

  const MaybeDocument& doc = documents.front();

  if (doc.is_no_document()) {
    api::DocumentSnapshot snapshot = api::DocumentSnapshot::FromNoDocument(
        firestore, key,
        api::SnapshotMetadata{/*from_cache=*/false,
                              /*has_pending_writes=*/false});
    return MakePublic(std::move(snapshot));

  } else if (doc.is_document()) {
    api::DocumentSnapshot snapshot = api::DocumentSnapshot::FromDocument(
        firestore, Document(doc),
        api::SnapshotMetadata{/*from_cache=*/false,
                              /*has_pending_writes=*/false});
    return MakePublic(std::move(snapshot));

  } else {
    // TODO(b/147444199): use string formatting.
    // HARD_FAIL(
    //     "core::Transaction::Lookup() returned unexpected document type:
    //     '%s'", doc.type());
    auto message = std::string(
                       "core::Transaction::Lookup() returned unexpected "
                       "document type: '") +
                   std::to_string(static_cast<int>(doc.type())) + "'";
    HARD_FAIL_IOS(message.c_str());
  }
}

}  // namespace

TransactionInternal::TransactionInternal(
    std::shared_ptr<core::Transaction> transaction,
    FirestoreInternal* firestore_internal)
    : transaction_{std::move(NOT_NULL(transaction))},
      firestore_internal_{NOT_NULL(firestore_internal)},
      user_data_converter_{&firestore_internal->database_id()} {}

Firestore* TransactionInternal::firestore() {
  return Firestore::GetInstance(firestore_internal_->app());
}

FirestoreInternal* TransactionInternal::firestore_internal() {
  return firestore_internal_;
}

void TransactionInternal::Set(const DocumentReference& document,
                              const MapFieldValue& data,
                              const SetOptions& options) {
  ParsedSetData parsed = user_data_converter_.ParseSetData(data, options);
  transaction_->Set(GetKey(document), std::move(parsed));
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldValue& data) {
  transaction_->Update(GetKey(document),
                       user_data_converter_.ParseUpdateData(data));
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldPathValue& data) {
  transaction_->Update(GetKey(document),
                       user_data_converter_.ParseUpdateData(data));
}

void TransactionInternal::Delete(const DocumentReference& document) {
  transaction_->Delete(GetKey(document));
}

DocumentSnapshot TransactionInternal::Get(const DocumentReference& document,
                                          Error* error_code,
                                          std::string* error_message) {
  std::promise<StatusOr<DocumentSnapshot>> promise;
  model::DocumentKey key = GetKey(document);

  transaction_->Lookup(
      {key}, [&](const util::StatusOr<std::vector<MaybeDocument>>& maybe_docs) {
        if (maybe_docs.ok()) {
          DocumentSnapshot snapshot =
              ConvertToSingleSnapshot(firestore_internal_->firestore_core(),
                                      std::move(key), maybe_docs.ValueOrDie());
          promise.set_value(std::move(snapshot));
        } else {
          promise.set_value(maybe_docs.status());
        }
      });

  auto future = promise.get_future();
  future.wait();
  StatusOr<DocumentSnapshot> result = future.get();

  if (result.ok()) {
    if (error_code != nullptr) {
      *error_code = Error::kErrorOk;
    }
    if (error_message != nullptr) {
      *error_message = "";
    }
    return std::move(result).ValueOrDie();
  } else {
    if (error_code != nullptr) {
      *error_code = result.status().code();
    }
    if (error_message != nullptr) {
      *error_message = result.status().error_message();
    }
    return DocumentSnapshot{};
  }
}

void TransactionInternal::MarkPermanentlyFailed() {
  transaction_->MarkPermanentlyFailed();
}

}  // namespace firestore
}  // namespace firebase
