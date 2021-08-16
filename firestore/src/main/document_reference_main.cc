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

#include "firestore/src/main/document_reference_main.h"

#include "Firestore/core/src/api/listener_registration.h"
#include "Firestore/core/src/core/listen_options.h"
#include "Firestore/core/src/core/user_data.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/listener_main.h"
#include "firestore/src/main/promise_main.h"
#include "firestore/src/main/set_options_main.h"
#include "firestore/src/main/source_main.h"
#include "firestore/src/main/util_main.h"

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
