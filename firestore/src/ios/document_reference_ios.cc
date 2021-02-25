#include "firestore/src/ios/document_reference_ios.h"

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/document_snapshot_ios.h"
#include "firestore/src/ios/listener_ios.h"
#include "firestore/src/ios/promise_ios.h"
#include "firestore/src/ios/set_options_ios.h"
#include "firestore/src/ios/source_ios.h"
#include "firestore/src/ios/util_ios.h"
#include "Firestore/core/src/api/listener_registration.h"
#include "Firestore/core/src/core/listen_options.h"
#include "Firestore/core/src/core/user_data.h"

namespace firebase {
namespace firestore {

using core::ParsedSetData;
using core::ParsedUpdateData;

DocumentReferenceInternal::DocumentReferenceInternal(
    api::DocumentReference&& reference)
    : reference_{std::move(reference)},
      promise_factory_{PromiseFactory<AsyncApis>::Create(this)},
      user_data_converter_{&firestore_internal()->database_id()} {}

Firestore* DocumentReferenceInternal::firestore() {
  return GetFirestore(&reference_);
}

FirestoreInternal* DocumentReferenceInternal::firestore_internal() {
  return GetFirestoreInternal(&reference_);
}

CollectionReference DocumentReferenceInternal::Parent() {
  return MakePublic(reference_.Parent());
}

CollectionReference DocumentReferenceInternal::Collection(
    const std::string& collection_path) {
  return MakePublic(reference_.GetCollectionReference(collection_path));
}

Future<DocumentSnapshot> DocumentReferenceInternal::Get(Source source) {
  auto promise =
      promise_factory_.CreatePromise<DocumentSnapshot>(AsyncApis::kGet);
  auto listener = ListenerWithPromise<api::DocumentSnapshot>(promise);
  reference_.GetDocument(ToCoreApi(source), std::move(listener));
  return promise.future();
}

Future<void> DocumentReferenceInternal::Set(const MapFieldValue& data,
                                            const SetOptions& options) {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kSet);
  auto callback = StatusCallbackWithPromise(promise);
  ParsedSetData parsed = user_data_converter_.ParseSetData(data, options);
  reference_.SetData(std::move(parsed), std::move(callback));
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
  auto callback = StatusCallbackWithPromise(promise);
  reference_.UpdateData(std::move(parsed), std::move(callback));
  return promise.future();
}

Future<void> DocumentReferenceInternal::Delete() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApis::kDelete);
  auto callback = StatusCallbackWithPromise(promise);
  reference_.DeleteDocument(std::move(callback));
  return promise.future();
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    EventListener<DocumentSnapshot>* listener) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  auto result = reference_.AddSnapshotListener(
      options, ListenerWithEventListener<api::DocumentSnapshot>(listener));
  return MakePublic(std::move(result), firestore_internal());
}

ListenerRegistration DocumentReferenceInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error, const std::string&)>
        callback) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  auto result = reference_.AddSnapshotListener(
      options,
      ListenerWithCallback<api::DocumentSnapshot>(std::move(callback)));
  return MakePublic(std::move(result), firestore_internal());
}

}  // namespace firestore
}  // namespace firebase
