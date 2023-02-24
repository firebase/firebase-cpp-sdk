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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_AGGREGATE_QUERY_SNAPSHOT_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_AGGREGATE_QUERY_SNAPSHOT_MAIN_H_

#include <cstddef>
#include <vector>

#include "Firestore/core/src/api/aggregate_query.h"
#include "firestore/src/include/firebase/firestore/aggregate_query.h"
#include "firestore/src/main/firestore_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class AggregateQuerySnapshotInternal {
 public:
  explicit AggregateQuerySnapshotInternal(api::AggregateQuery&& aggregate_query,
                                          int64_t count);

  FirestoreInternal* firestore_internal();
  const FirestoreInternal* firestore_internal() const;

  AggregateQuery query() const;
  int64_t count() const;

  std::size_t Hash() const {
    return util::Hash(aggregate_query_.query().Hash(), count_result_);
  }

  friend bool operator==(const AggregateQuerySnapshotInternal& lhs,
                         const AggregateQuerySnapshotInternal& rhs);

 private:
  api::AggregateQuery aggregate_query_;
  int64_t count_result_ = 0;
};

inline bool operator!=(const AggregateQuerySnapshotInternal& lhs,
                       const AggregateQuerySnapshotInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_AGGREGATE_QUERY_SNAPSHOT_MAIN_H_
