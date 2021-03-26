#include "firestore/src/ios/write_batch_ios.h"

#include <utility>

#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/document_reference_ios.h"
#include "firestore/src/ios/field_value_ios.h"
#include "firestore/src/ios/listener_ios.h"
#include "firestore/src/ios/set_options_ios.h"
#include "firestore/src/ios/util_ios.h"
#include "Firestore/core/src/core/user_data.h"
#include "Firestore/core/src/model/field_path.h"

namespace firebase {
namespace firestore {

using core::ParsedSetData;
using core::ParsedUpdateData;

WriteBatchInternal::WriteBatchInternal(api::WriteBatch&& batch)
    : batch_{std::move(batch)},
      promise_factory_{PromiseFactory<AsyncApis>::Create(this)},
      user_data_converter_{&firestore_internal()->database_id()} {}

Firestore* WriteBatchInternal::firestore() { return GetFirestore(&batch_); }

FirestoreInternal* WriteBatchInternal::firestore_internal() {
  return GetFirestoreInternal(&batch_);
}

void WriteBatchInternal::Set(const DocumentReference& document,
                             const MapFieldValue& data,
                             const SetOptions& options) {
  ParsedSetData parsed = user_data_converter_.ParseSetData(data, options);
  batch_.SetData(GetCoreApi(document), std::move(parsed));
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldValue& data) {
  batch_.UpdateData(GetCoreApi(document),
                    user_data_converter_.ParseUpdateData(data));
}

void WriteBatchInternal::Update(const DocumentReference& document,
                                const MapFieldPathValue& data) {
  batch_.UpdateData(GetCoreApi(document),
                    user_data_converter_.ParseUpdateData(data));
}

void WriteBatchInternal::Delete(const DocumentReference& document) {
  batch_.DeleteData(GetCoreApi(document));
}

Future<void> WriteBatchInternal::Commit() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kCommit);
  auto callback = StatusCallbackWithPromise(promise);
  batch_.Commit(std::move(callback));
  return promise.future();
}

}  // namespace firestore
}  // namespace firebase
