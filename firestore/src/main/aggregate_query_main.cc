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

#include <utility>

#include "firestore/src/main/aggregate_query_main.h"

#include "Firestore/core/src/api/aggregate_query.h"
#include "Firestore/core/src/api/query_core.h"
#include "firestore/src/include/firebase/firestore/aggregate_query.h"
#include "firestore/src/include/firebase/firestore/aggregate_query_snapshot.h"
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#include "firestore/src/main/listener_main.h"
#include "firestore/src/main/promise_factory_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

AggregateQueryInternal::AggregateQueryInternal(
    api::AggregateQuery&& aggregate_query)
    : aggregate_query_{std::move(aggregate_query)},
      promise_factory_{PromiseFactory<AsyncApis>::Create(this)} {}

FirestoreInternal* AggregateQueryInternal::firestore_internal() {
  return GetFirestoreInternal(&aggregate_query_.query());
}

const FirestoreInternal* AggregateQueryInternal::firestore_internal() const {
  return GetFirestoreInternal(&aggregate_query_.query());
}

Query AggregateQueryInternal::query() {
  return MakePublic(api::Query(aggregate_query_.query()));
}

Future<AggregateQuerySnapshot> AggregateQueryInternal::Get(
    AggregateSource source) {
  api::AggregateQuery aggregate_query = aggregate_query_;
  auto promise =
      promise_factory_.CreatePromise<AggregateQuerySnapshot>(AsyncApis::kGet);
  // TODO(C++14)
  // https://en.wikipedia.org/wiki/C%2B%2B14#Lambda_capture_expressions
  aggregate_query_.Get(
      [aggregate_query, promise](util::StatusOr<int64_t> maybe_value) mutable {
        if (maybe_value.ok()) {
          int64_t count = maybe_value.ValueOrDie();
          AggregateQuerySnapshotInternal internal{std::move(aggregate_query),
                                                  count};
          AggregateQuerySnapshot snapshot = MakePublic(std::move(internal));
          promise.SetValue(std::move(snapshot));
        } else {
          promise.SetError(maybe_value.status());
        }
      });
  return promise.future();
}

bool operator==(const AggregateQueryInternal& lhs,
                const AggregateQueryInternal& rhs) {
  // TODO(b/276440573) - there needs to be equals operator defined on
  // api::AggregateQuery
  return lhs.aggregate_query_.query() == rhs.aggregate_query_.query();
}

}  // namespace firestore
}  // namespace firebase
