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

#include "firestore/src/include/firebase/firestore/document_snapshot.h"

#include <ostream>
#include <utility>

#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/to_string.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_snapshot_android.h"
#else
#include "firestore/src/main/document_snapshot_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnDocumentSnapshot = CleanupFn<DocumentSnapshot>;

DocumentSnapshot::DocumentSnapshot() {}

DocumentSnapshot::DocumentSnapshot(const DocumentSnapshot& snapshot) {
  if (snapshot.internal_) {
    internal_ = new DocumentSnapshotInternal(*snapshot.internal_);
  }
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::DocumentSnapshot(DocumentSnapshot&& snapshot) {
  CleanupFnDocumentSnapshot::Unregister(&snapshot, snapshot.internal_);
  std::swap(internal_, snapshot.internal_);
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::DocumentSnapshot(DocumentSnapshotInternal* internal)
    : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::~DocumentSnapshot() {
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

DocumentSnapshot& DocumentSnapshot::operator=(
    const DocumentSnapshot& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete internal_;
  if (snapshot.internal_) {
    internal_ = new DocumentSnapshotInternal(*snapshot.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnDocumentSnapshot::Register(this, internal_);
  return *this;
}

DocumentSnapshot& DocumentSnapshot::operator=(DocumentSnapshot&& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnDocumentSnapshot::Unregister(&snapshot, snapshot.internal_);
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = snapshot.internal_;
  snapshot.internal_ = nullptr;
  CleanupFnDocumentSnapshot::Register(this, internal_);
  return *this;
}

const std::string& DocumentSnapshot::id() const {
  if (!internal_) return EmptyString();
  return internal_->id();
}

DocumentReference DocumentSnapshot::reference() const {
  if (!internal_) return {};
  return internal_->reference();
}

SnapshotMetadata DocumentSnapshot::metadata() const {
  if (!internal_) return SnapshotMetadata{false, false};
  return internal_->metadata();
}

bool DocumentSnapshot::exists() const {
  if (!internal_) return {};
  return internal_->exists();
}

MapFieldValue DocumentSnapshot::GetData(ServerTimestampBehavior stb) const {
  if (!internal_) return MapFieldValue{};
  return internal_->GetData(stb);
}

FieldValue DocumentSnapshot::Get(const char* field,
                                 ServerTimestampBehavior stb) const {
  if (!field) {
    SimpleThrowInvalidArgument("Field name cannot be null.");
  }

  if (!internal_) return {};
  return internal_->Get(FieldPath::FromDotSeparatedString(field), stb);
}

FieldValue DocumentSnapshot::Get(const std::string& field,
                                 ServerTimestampBehavior stb) const {
  if (!internal_) return {};
  return internal_->Get(FieldPath::FromDotSeparatedString(field), stb);
}

FieldValue DocumentSnapshot::Get(const FieldPath& field,
                                 ServerTimestampBehavior stb) const {
  if (!internal_) return {};
  return internal_->Get(field, stb);
}

std::string DocumentSnapshot::ToString() const {
  if (!internal_) return "DocumentSnapshot(invalid)";

  return std::string("DocumentSnapshot(id=") + id() +
         ", metadata=" + metadata().ToString() +
         ", doc=" + ::firebase::firestore::ToString(GetData()) + ')';
}

size_t DocumentSnapshot::Hash() const {
  if (!internal_) return {};
  return internal_->Hash();
}

std::ostream& operator<<(std::ostream& out, const DocumentSnapshot& document) {
  return out << document.ToString();
}

bool operator==(const DocumentSnapshot& lhs, const DocumentSnapshot& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

}  // namespace firestore
}  // namespace firebase
