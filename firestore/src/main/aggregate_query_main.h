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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_AGGREGATE_QUERY_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_AGGREGATE_QUERY_MAIN_H_

#include "Firestore/core/src/api/aggregate_query.h"
#include "firestore/src/include/firebase/firestore/aggregate_source.h"
#include "firestore/src/main/firestore_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class AggregateQuerySnapshot;

class AggregateQueryInternal {
 public:
  explicit AggregateQueryInternal(api::AggregateQuery&& aggregate_query);

  FirestoreInternal* firestore_internal();
  const FirestoreInternal* firestore_internal() const;

  Query query();

  Future<AggregateQuerySnapshot> Get(AggregateSource source);

  size_t Hash() const { return aggregate_query_.query().Hash(); }

  friend bool operator==(const AggregateQueryInternal& lhs,
                         const AggregateQueryInternal& rhs);

 private:
  enum class AsyncApis {
    kGet,
    kCount,
  };

  friend class AggregateQuerySnapshotTest;

  api::AggregateQuery aggregate_query_;
  PromiseFactory<AsyncApis> promise_factory_;
};

inline bool operator!=(const AggregateQueryInternal& lhs,
                       const AggregateQueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_AGGREGATE_QUERY_MAIN_H_
