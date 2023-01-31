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
#include "firestore/src/include/firebase/firestore/aggregate_query_snapshot.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/util.h"

#if defined(__ANDROID__)
// TODO(tomandersen) #include "firestore/src/android/aggregate_query_android.h"
#else
#include "firestore/src/main/aggregate_query_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnQuery = CleanupFn<AggregateQuery>;

AggregateQuery::AggregateQuery() {}

AggregateQuery::AggregateQuery(const AggregateQuery& query)  {
  if (query.internal_) {
    internal_ = new AggregateQueryInternal(*query.internal_);
  }
  CleanupFnQuery::Register(this, internal_);
}

AggregateQuery::AggregateQuery(AggregateQuery&& query) {
  CleanupFnQuery::Unregister(&query, query.internal_);
  std::swap(internal_, query.internal_);
  CleanupFnQuery::Register(this, internal_);
}

AggregateQuery::AggregateQuery(AggregateQueryInternal* internal) : internal_(internal) {
  // NOTE: We don't assert internal != nullptr here since internal can be
  // nullptr when called by the CollectionReference copy constructor.
  CleanupFnQuery::Register(this, internal_);
}

AggregateQuery::~AggregateQuery() {
  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

AggregateQuery& AggregateQuery::operator=(const AggregateQuery& query) {
  if (this == &query) {
    return *this;
  }

  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  if (query.internal_) {
    internal_ = new AggregateQueryInternal(*query.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

AggregateQuery& AggregateQuery::operator=(AggregateQuery&& query) {
  if (this == &query) {
    return *this;
  }

  CleanupFnQuery::Unregister(&query, query.internal_);
  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  internal_ = query.internal_;
  query.internal_ = nullptr;
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

Query AggregateQuery::query() const {
  if (!internal_) return {};
  return internal_->query();
}

Future<AggregateQuerySnapshot> AggregateQuery::Get(AggregateSource aggregateSource) const {
  if (!internal_) return FailedFuture<AggregateQuerySnapshot>();
  return internal_->Get(aggregateSource);
}

bool operator==(const AggregateQuery& lhs, const AggregateQuery& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}


}  // namespace firestore
}  // namespace firebase
