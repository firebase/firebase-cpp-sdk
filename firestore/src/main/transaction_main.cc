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

#include "firestore/src/main/transaction_main.h"

#include <chrono>
#include <condition_variable>  // NOLINT(build/c++11)
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include "Firestore/core/src/core/user_data.h"
#include "Firestore/core/src/model/document.h"
#include "Firestore/core/src/model/document_key.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/util/exception.h"
#include "Firestore/core/src/util/status.h"
#include "Firestore/core/src/util/statusor.h"
#include "absl/types/optional.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/document_reference_main.h"
#include "firestore/src/main/field_value_main.h"
#include "firestore/src/main/set_options_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

namespace {

using core::ParsedSetData;
using core::ParsedUpdateData;
using model::Document;
using util::Status;
using util::StatusOr;

const model::DocumentKey& GetKey(const DocumentReference& document) {
  return GetInternal(&document)->key();
}

DocumentSnapshot ConvertToSingleSnapshot(
    const std::shared_ptr<api::Firestore>& firestore,
    model::DocumentKey key,
    const std::vector<Document>& documents) {
  SIMPLE_HARD_ASSERT(
      documents.size() == 1,
      "Expected core::Transaction::Lookup() to return a single document");

  const Document& doc = documents.front();

  if (doc->is_found_document()) {
    api::DocumentSnapshot snapshot = api::DocumentSnapshot::FromDocument(
        firestore, Document(doc),
        api::SnapshotMetadata{/*from_cache=*/false,
                              /*has_pending_writes=*/false});
    return MakePublic(std::move(snapshot));

  } else if (doc->is_no_document()) {
    api::DocumentSnapshot snapshot = api::DocumentSnapshot::FromNoDocument(
        firestore, key,
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
                   doc.ToString() + "'";
    SIMPLE_HARD_FAIL(message);
  }
}

}  // namespace

TransactionInternal::TransactionInternal(
    std::shared_ptr<core::Transaction> transaction,
    std::shared_ptr<api::Firestore> firestore_core,
    Firestore* firestore_public)
    : transaction_{std::move(NOT_NULL(transaction))},
      firestore_core_{std::move(NOT_NULL(firestore_core))},
      firestore_public_{firestore_public},
      user_data_converter_{&firestore_core_->database_id()} {}

Firestore* TransactionInternal::firestore() {
  return firestore_public_;
}

void TransactionInternal::Set(const DocumentReference& document,
                              const MapFieldValue& data,
                              const SetOptions& options) {
  ValidateReference(document);
  ParsedSetData parsed = user_data_converter_.ParseSetData(data, options);
  transaction_->Set(GetKey(document), std::move(parsed));
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldValue& data) {
  ValidateReference(document);
  transaction_->Update(GetKey(document),
                       user_data_converter_.ParseUpdateData(data));
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldPathValue& data) {
  ValidateReference(document);
  transaction_->Update(GetKey(document),
                       user_data_converter_.ParseUpdateData(data));
}

void TransactionInternal::Delete(const DocumentReference& document) {
  ValidateReference(document);
  transaction_->Delete(GetKey(document));
}

DocumentSnapshot TransactionInternal::Get(const DocumentReference& document,
                                          Error* error_code,
                                          std::string* error_message) {
  ValidateReference(document);

  struct LookupState {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    StatusOr<DocumentSnapshot> result =
        Status(Error::kErrorAborted, "Transaction lookup was cancelled.");
  };
  auto state = std::make_shared<LookupState>();
  model::DocumentKey key = GetKey(document);
  std::shared_ptr<api::Firestore> firestore_core = firestore_core_;

  struct CancelGuard {
    std::shared_ptr<LookupState> state;
    CancelGuard() = default;
    CancelGuard(CancelGuard&&) = default;
    ~CancelGuard() {
      if (state) {
        std::scoped_lock lock(state->mutex);
        if (!state->done) {
          state->done = true;
          state->cv.notify_all();
        }
      }
    }
  };
  auto guard = std::make_shared<CancelGuard>();
  guard->state = state;

  transaction_->Lookup(
      {key},
      [state, guard, firestore_core = std::move(firestore_core),
       key = std::move(key)](
          const util::StatusOr<std::vector<Document>>& maybe_docs) mutable {
        StatusOr<DocumentSnapshot> outcome =
            maybe_docs.ok()
                ? StatusOr<DocumentSnapshot>(ConvertToSingleSnapshot(
                      firestore_core, std::move(key), maybe_docs.ValueOrDie()))
                : StatusOr<DocumentSnapshot>(maybe_docs.status());
        {
          std::scoped_lock lock(state->mutex);
          if (!state->done) {
            state->result = std::move(outcome);
            state->done = true;
            state->cv.notify_all();
          }
        }
        guard->state.reset();
      });
  guard.reset();

  StatusOr<DocumentSnapshot> result;
  {
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->cv.wait_for(lock, std::chrono::seconds(60),
                            [&state] { return state->done; })) {
      state->done = true;
      result = Status(Error::kErrorDeadlineExceeded,
                      "Transaction lookup timed out.");
    } else {
      result = std::move(state->result);
    }
  }

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

void TransactionInternal::ValidateReference(const DocumentReference& document) {
  auto* internal_doc = GetInternal(&document);
  SIMPLE_HARD_ASSERT(internal_doc, "Invalid document reference provided.");

  if (internal_doc->firestore() != firestore()) {
    SimpleThrowInvalidArgument(
        "Provided document reference is from a different Cloud Firestore "
        "instance.");
  }
}

}  // namespace firestore
}  // namespace firebase


namespace firebase {
namespace firestore {
FirestoreInternal* TransactionInternal::firestore_internal() {
  return GetInternal(firestore_public_);
}
}
}
