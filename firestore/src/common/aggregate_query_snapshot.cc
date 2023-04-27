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
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/aggregate_query.h"

#if defined(__ANDROID__)
#include "firestore/src/android/aggregate_query_snapshot_android.h"
#else
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnAggregateQuerySnapshot = CleanupFn<AggregateQuerySnapshot>;

AggregateQuerySnapshot::AggregateQuerySnapshot() {}

AggregateQuerySnapshot::AggregateQuerySnapshot(
    const AggregateQuerySnapshot& other) {
  if (other.internal_) {
    internal_ = new AggregateQuerySnapshotInternal(*other.internal_);
  }
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
}

AggregateQuerySnapshot::AggregateQuerySnapshot(AggregateQuerySnapshot&& other) {
  CleanupFnAggregateQuerySnapshot::Unregister(&other, other.internal_);
  std::swap(internal_, other.internal_);
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
}

AggregateQuerySnapshot::AggregateQuerySnapshot(
    AggregateQuerySnapshotInternal* internal)
    : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
}

AggregateQuerySnapshot::~AggregateQuerySnapshot() {
  CleanupFnAggregateQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

AggregateQuerySnapshot& AggregateQuerySnapshot::operator=(
    const AggregateQuerySnapshot& other) {
  if (this == &other) {
    return *this;
  }

  CleanupFnAggregateQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  if (other.internal_) {
    internal_ = new AggregateQuerySnapshotInternal(*other.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnAggregateQuerySnapshot::Register(this, internal_);
  return *this;
}

AggregateQuerySnapshot& AggregateQuerySnapshot::operator=(
    AggregateQuerySnapshot&& other) {
  if (this == &other) {
    return *this;
  }

  CleanupFnAggregateQuerySnapshot::Unregister(&other, other.internal_);
  CleanupFnAggregateQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = other.internal_;
  other.internal_ = nullptr;
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

size_t AggregateQuerySnapshot::Hash() const {
  if (!internal_) return {};
  return internal_->Hash();
}

}  // namespace firestore
}  // namespace firebase
