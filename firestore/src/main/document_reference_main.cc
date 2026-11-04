// Copyright 2021 Google LLC
#include "firestore/src/main/document_reference_main.h"

#include "Firestore/core/src/api/listener_registration.h"
#include "Firestore/core/src/core/listen_options.h"
#include "Firestore/core/src/core/user_data.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/listener_main.h"
#include "firestore/src/main/promise_main.h"
#include "firestore/src/main/set_options_main.h"
#include "firestore/src/main/source_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase::firestore {

using core::ParsedSetData;
using core::ParsedUpdateData;

DocumentReferenceInternal::DocumentReferenceInternal(api::DocumentReference&& reference)
    : reference_{std::move(reference)},
      promise_factory_{PromiseFactory<AsyncApis>::Create(this)},
      user_data_converter_{&reference_.firestore()->database_id()} {}

Firestore* DocumentReferenceInternal::firestore() const { return GetFirestore(&reference_); }
FirestoreInternal* DocumentReferenceInternal::firestore_internal() const {
  return GetFirestoreInternal(&reference_);
}

CollectionReference DocumentReferenceInternal::Parent() const {
  return MakePublic(reference_.Parent());
}

CollectionReference DocumentReferenceInternal::Collection(
    const std::string& collection_path) const {
  if (collection_path.find("//") != std::string::npos) {
    SimpleThrowInvalidArgument("Invalid path (" + collection_path +
                               "). Paths must not contain // in them.");
  }
  return MakePublic(reference_.GetCollectionReference(collection_path));
}

Future<DocumentSnapshot> DocumentReferenceInternal::Get(Source source) {
  auto promise = promise_factory_.CreatePromise<DocumentSnapshot>(AsyncApis::kGet);
  reference_.GetDocument(ToCoreApi(source), ListenerWithPromise<api::DocumentSnapshot>(promise));
  return promise.future();
}

Future<void> DocumentReferenceInternal::Set(const MapFieldValue& data, const SetOptions& options) {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kSet);
  reference_.SetData(user_data_converter_.ParseSetData(data, options),
                     StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> DocumentReferenceInternal::Update(const MapFieldValue& data) {
  return UpdateImpl(user_data_converter_.ParseUpdateData(data));
}

Future<void> DocumentReferenceInternal::Update(const MapFieldPathValue& data) {
  return UpdateImpl(user_data_converter_.ParseUpdateData(data));
}

Future<void> DocumentReferenceInternal::UpdateImpl(ParsedUpdateData&& parsed) {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kUpdate);
  reference_.UpdateData(std::move(parsed), StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> DocumentReferenceInternal::Delete() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kDelete);
  reference_.DeleteDocument(StatusCallbackWithPromise(promise));
  return promise.future();
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<DocumentSnapshot>* listener) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  return MakePublic(
      reference_.AddSnapshotListener(options,
                                     ListenerWithEventListener<api::DocumentSnapshot>(listener)),
      firestore_internal());
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error, const std::string&)> callback) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  return MakePublic(
      reference_.AddSnapshotListener(
          options, ListenerWithCallback<api::DocumentSnapshot>(std::move(callback))),
      firestore_internal());
}

}  // namespace firebase::firestore
