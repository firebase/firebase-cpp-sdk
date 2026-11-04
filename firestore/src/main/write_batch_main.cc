// Copyright 2021 Google LLC
#include "firestore/src/main/write_batch_main.h"

#include <utility>
#include "Firestore/core/src/core/user_data.h"
#include "Firestore/core/src/model/field_path.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/document_reference_main.h"
#include "firestore/src/main/field_value_main.h"
#include "firestore/src/main/listener_main.h"
#include "firestore/src/main/set_options_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase::firestore {

WriteBatchInternal::WriteBatchInternal(api::WriteBatch&& batch)
    : batch_{std::move(batch)},
      promise_factory_{PromiseFactory<AsyncApis>::Create(this)},
      user_data_converter_{&batch_.firestore()->database_id()} {}

Firestore* WriteBatchInternal::firestore() { return GetFirestore(&batch_); }
FirestoreInternal* WriteBatchInternal::firestore_internal() {
  return GetFirestoreInternal(&batch_);
}

void WriteBatchInternal::Set(const DocumentReference& document, const MapFieldValue& data,
                             const SetOptions& options) {
  if (!document.is_valid()) SimpleThrowInvalidArgument("Invalid document reference provided.");
  batch_.SetData(GetCoreApi(document), user_data_converter_.ParseSetData(data, options));
}

void WriteBatchInternal::Update(const DocumentReference& document, const MapFieldValue& data) {
  if (!document.is_valid()) SimpleThrowInvalidArgument("Invalid document reference provided.");
  batch_.UpdateData(GetCoreApi(document), user_data_converter_.ParseUpdateData(data));
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldPathValue& data) {
  if (!document.is_valid()) SimpleThrowInvalidArgument("Invalid document reference provided.");
  batch_.UpdateData(GetCoreApi(document), user_data_converter_.ParseUpdateData(data));
}

void WriteBatchInternal::Delete(const DocumentReference& document) {
  if (!document.is_valid()) SimpleThrowInvalidArgument("Invalid document reference provided.");
  batch_.DeleteData(GetCoreApi(document));
}

Future<void> WriteBatchInternal::Commit() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kCommit);
  batch_.Commit(StatusCallbackWithPromise(promise));
  return promise.future();
}

}  // namespace firebase::firestore
