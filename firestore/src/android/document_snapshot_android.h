// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_

#include <mutex>
#include <string>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"

namespace firebase::firestore {

class DocumentSnapshotInternal : public Wrapper {
 public:
  using Wrapper::Wrapper;

  DocumentSnapshotInternal(const DocumentSnapshotInternal& other);
  DocumentSnapshotInternal(DocumentSnapshotInternal&& other) noexcept;
  DocumentSnapshotInternal& operator=(const DocumentSnapshotInternal& other) =
      delete;
  DocumentSnapshotInternal& operator=(
      DocumentSnapshotInternal&& other) noexcept = delete;

  static void Initialize(jni::Loader& loader);
  ~DocumentSnapshotInternal() override = default;

  Firestore* firestore() const;
  const std::string& id() const;
  DocumentReference reference() const;
  SnapshotMetadata metadata() const;
  bool exists() const;
  MapFieldValue GetData(DocumentSnapshot::ServerTimestampBehavior stb) const;
  FieldValue Get(const FieldPath& field,
                 DocumentSnapshot::ServerTimestampBehavior stb) const;
  std::size_t Hash() const;

 private:
  mutable std::once_flag id_once_flag_;
  mutable std::string cached_id_;
};

bool operator==(const DocumentSnapshotInternal& lhs,
                const DocumentSnapshotInternal& rhs);
inline bool operator!=(const DocumentSnapshotInternal& lhs,
                       const DocumentSnapshotInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_
