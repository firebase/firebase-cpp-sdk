/*
 * Copyright 2022 Google LLC
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

#include "firestore/src/include/firebase/firestore/aggregate_query.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/aggregate_query_snapshot.h"

#if defined(__ANDROID__)
// TODO(tomandersen) #include "firestore/src/android/aggregate_query_android.h"
#else
#include "firestore/src/main/aggregate_query_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnQuery = CleanupFn<AggregateQuery>;

AggregateQuery::AggregateQuery() {}

AggregateQuery::AggregateQuery(const AggregateQuery& aggregate_query) {
  if (aggregate_query.internal_) {
    internal_ = new AggregateQueryInternal(*aggregate_query.internal_);
  }
  CleanupFnQuery::Register(this, internal_);
}

AggregateQuery::AggregateQuery(AggregateQuery&& aggregate_query) {
  CleanupFnQuery::Unregister(&aggregate_query, aggregate_query.internal_);
  std::swap(internal_, aggregate_query.internal_);
  CleanupFnQuery::Register(this, internal_);
}

AggregateQuery::AggregateQuery(AggregateQueryInternal* internal)
    : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnQuery::Register(this, internal_);
}

AggregateQuery::~AggregateQuery() {
  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

AggregateQuery& AggregateQuery::operator=(
    const AggregateQuery& aggregate_query) {
  if (this == &aggregate_query) {
    return *this;
  }

  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  if (aggregate_query.internal_) {
    internal_ = new AggregateQueryInternal(*aggregate_query.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

AggregateQuery& AggregateQuery::operator=(AggregateQuery&& aggregate_query) {
  if (this == &aggregate_query) {
    return *this;
  }

  CleanupFnQuery::Unregister(&aggregate_query, aggregate_query.internal_);
  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  internal_ = aggregate_query.internal_;
  aggregate_query.internal_ = nullptr;
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

Query AggregateQuery::query() const {
  if (!internal_) return {};
  return internal_->query();
}

Future<AggregateQuerySnapshot> AggregateQuery::Get(
    AggregateSource aggregate_source) const {
  if (!internal_) return FailedFuture<AggregateQuerySnapshot>();
  return internal_->Get(aggregate_source);
}

bool operator==(const AggregateQuery& lhs, const AggregateQuery& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

}  // namespace firestore
}  // namespace firebase
