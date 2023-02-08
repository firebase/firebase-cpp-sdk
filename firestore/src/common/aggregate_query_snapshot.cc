/*
 * Copyright 2023 Google LLC
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

#include "firestore/src/include/firebase/firestore/aggregate_query_snapshot.h"

#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/aggregate_query.h"

#if defined(__ANDROID__)
// TODO(tomandersen) #include
// "firestore/src/android/aggregate_query_snapshot_android.h"
#else
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnAggregateQuerySnapshot = CleanupFn<AggregateQuerySnapshot>;

AggregateQuerySnapshot::AggregateQuerySnapshot() {}

AggregateQuerySnapshot::AggregateQuerySnapshot(
    const AggregateQuerySnapshot& query) {
  if (query.internal_) {
    internal_ = new AggregateQuerySnapshotInternal(*query.internal_);
  }
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
}

AggregateQuerySnapshot::AggregateQuerySnapshot(AggregateQuerySnapshot&& query) {
  CleanupFnAggregateQuerySnapshot::Unregister(&query, query.internal_);
  std::swap(internal_, query.internal_);
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
}

AggregateQuerySnapshot::AggregateQuerySnapshot(
    AggregateQuerySnapshotInternal* internal)
    : internal_(internal) {
  // NOTE: We don't assert internal != nullptr here since internal can be
  // nullptr when called by the CollectionReference copy constructor.
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
}

AggregateQuerySnapshot::~AggregateQuerySnapshot() {
  CleanupFnAggregateQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

AggregateQuerySnapshot& AggregateQuerySnapshot::operator=(
    const AggregateQuerySnapshot& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnAggregateQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  if (snapshot.internal_) {
    internal_ = new AggregateQuerySnapshotInternal(*snapshot.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
  return *this;
}

AggregateQuerySnapshot& AggregateQuerySnapshot::operator=(
    AggregateQuerySnapshot&& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnAggregateQuerySnapshot::Unregister(&snapshot, snapshot.internal_);
  CleanupFnAggregateQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = snapshot.internal_;
  snapshot.internal_ = nullptr;
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
  return *this;
}

AggregateQuery AggregateQuerySnapshot::query() const {
  if (!internal_) return {};
  return internal_->query();
}

int64_t AggregateQuerySnapshot::count() const {
  if (!internal_) return 0;
  return internal_->count();
}

bool operator==(const AggregateQuerySnapshot& lhs,
                const AggregateQuerySnapshot& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

}  // namespace firestore
}  // namespace firebase
