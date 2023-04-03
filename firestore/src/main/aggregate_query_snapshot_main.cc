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

#include "firestore/src/main/aggregate_query_snapshot_main.h"

#include <utility>
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

AggregateQuerySnapshotInternal::AggregateQuerySnapshotInternal(
    api::AggregateQuery&& aggregate_query, int64_t count_result)
    : aggregate_query_(std::move(aggregate_query)),
      count_result_(count_result) {}

FirestoreInternal* AggregateQuerySnapshotInternal::firestore_internal() {
  return GetFirestoreInternal(&aggregate_query_.query());
}

const FirestoreInternal* AggregateQuerySnapshotInternal::firestore_internal()
    const {
  return GetFirestoreInternal(&aggregate_query_.query());
}

AggregateQuery AggregateQuerySnapshotInternal::query() const {
  return MakePublic(api::AggregateQuery(aggregate_query_));
}

int64_t AggregateQuerySnapshotInternal::count() const { return count_result_; }

bool operator==(const AggregateQuerySnapshotInternal& lhs,
                const AggregateQuerySnapshotInternal& rhs) {
  // TODO(b/276440573) - there needs to be equals operator defined on
  // api::AggregateQuery
  return lhs.aggregate_query_.query() == rhs.aggregate_query_.query() &&
         lhs.count_result_ == rhs.count_result_;
}

}  // namespace firestore
}  // namespace firebase
