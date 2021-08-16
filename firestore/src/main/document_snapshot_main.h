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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_SNAPSHOT_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_SNAPSHOT_MAIN_H_

#include <memory>
#include <string>
#include <vector>

#include "Firestore/core/src/api/document_snapshot.h"
#include "Firestore/core/src/model/field_value.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/main/firestore_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class Firestore;
class FirestoreInternal;

class DocumentSnapshotInternal {
 public:
  explicit DocumentSnapshotInternal(api::DocumentSnapshot&& snapshot);

  Firestore* firestore();
  FirestoreInternal* firestore_internal();
  const FirestoreInternal* firestore_internal() const;

  const std::string& id() const;
  bool exists() const;
  SnapshotMetadata metadata() const;

  MapFieldValue GetData(DocumentSnapshot::ServerTimestampBehavior stb) const;

  FieldValue Get(const FieldPath& field,
                 DocumentSnapshot::ServerTimestampBehavior stb) const;

  const api::DocumentSnapshot& document_snapshot_core() const {
    return snapshot_;
  }

  DocumentReference reference() const;

 private:
  using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

  FieldValue GetValue(const model::FieldPath& path,
                      ServerTimestampBehavior stb) const;

  // Note: these are member functions only because access to `api::Firestore`
  // is needed to create a `DocumentReferenceInternal`.
  FieldValue ConvertAnyValue(const model::FieldValue& input,
                             ServerTimestampBehavior stb) const;
  FieldValue ConvertObject(const model::FieldValue::Map& object,
                           ServerTimestampBehavior stb) const;
  FieldValue ConvertArray(const model::FieldValue::Array& array,
                          ServerTimestampBehavior stb) const;
  FieldValue ConvertReference(
      const model::FieldValue::Reference& reference) const;
  FieldValue ConvertServerTimestamp(
      const model::FieldValue::ServerTimestamp& server_timestamp,
      ServerTimestampBehavior stb) const;
  FieldValue ConvertScalar(const model::FieldValue& scalar,
                           ServerTimestampBehavior stb) const;

  api::DocumentSnapshot snapshot_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_SNAPSHOT_MAIN_H_
