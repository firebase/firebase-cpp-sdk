/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_

#include <string>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"

namespace firebase {
namespace firestore {

// This is the Android implementation of DocumentSnapshot.
class DocumentSnapshotInternal : public Wrapper {
 public:
  using Wrapper::Wrapper;

  static void Initialize(jni::Loader& loader);

  ~DocumentSnapshotInternal() override {}

  /** Gets the Firestore instance associated with this document snapshot. */
  Firestore* firestore() const;

  /** Gets the document-id of this document. */
  const std::string& id() const;

  /** Gets the document location. */
  DocumentReference reference() const;

  /** Gets the metadata about this snapshot. */
  SnapshotMetadata metadata() const;

  /** Verifies the existence. */
  bool exists() const;

  /** Gets all data in the document as a map. */
  MapFieldValue GetData(DocumentSnapshot::ServerTimestampBehavior stb) const;

  /** Gets a specific field from the document. */
  FieldValue Get(const FieldPath& field,
                 DocumentSnapshot::ServerTimestampBehavior stb) const;

  std::size_t Hash() const;

 private:
  mutable std::string cached_id_;
};

bool operator==(const DocumentSnapshotInternal& lhs,
                const DocumentSnapshotInternal& rhs);

inline bool operator!=(const DocumentSnapshotInternal& lhs,
                       const DocumentSnapshotInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_
