// Copyright 2020 Google LLC
#include "firestore/src/include/firebase/firestore/query.h"

#include <cstdint>
#include <utility>
#include "app/src/include/firebase/future.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/aggregate_query.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/filter.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_android.h"
#else
#include "firestore/src/main/query_main.h"
#endif

namespace firebase::firestore {

using CleanupFnQuery = CleanupFn<Query>;

Query::Query() {}

Query::Query(const Query& query)
    : internal_(query.internal_ ? new QueryInternal(*query.internal_) : nullptr) {
  CleanupFnQuery::Register(this, internal_);
}

Query::Query(Query&& query) {
  CleanupFnQuery::Unregister(&query, query.internal_);
  std::swap(internal_, query.internal_);
  CleanupFnQuery::Register(this, internal_);
}

Query::Query(QueryInternal* internal) : internal_(internal) {
  CleanupFnQuery::Register(this, internal_);
}

Query::~Query() {
  CleanupFnQuery::Unregister(this, internal_);
  delete std::exchange(internal_, nullptr);
}

Query& Query::operator=(const Query& query) {
  if (this == &query) return *this;
  QueryInternal* new_internal = query.internal_ ? new QueryInternal(*query.internal_) : nullptr;
  CleanupFnQuery::Unregister(this, internal_);
  delete std::exchange(internal_, new_internal);
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

Query& Query::operator=(Query&& query) {
  if (this == &query) return *this;
  CleanupFnQuery::Unregister(&query, query.internal_);
  CleanupFnQuery::Unregister(this, internal_);
  delete std::exchange(internal_, std::exchange(query.internal_, nullptr));
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

const Firestore* Query::firestore() const { return internal_ ? internal_->firestore() : nullptr; }
Firestore* Query::firestore() { return internal_ ? internal_->firestore() : nullptr; }
AggregateQuery Query::Count() const { return internal_ ? internal_->Count() : AggregateQuery{}; }

Query Query::Where(const Filter& filter) const {
  if (!internal_) return {};
  return filter.IsEmpty() ? *this : internal_->Where(filter);
}

namespace {

bool ValidateFieldAndValue(const FieldPath& field, const FieldValue& value) {
  if (!field.is_valid()) {
    SimpleThrowInvalidArgument("Field path cannot be invalid.");
    return false;
  }
  if (!value.is_valid()) {
    SimpleThrowInvalidArgument("Field value cannot be invalid.");
    return false;
  }
  return true;
}

bool ValidateValues(const std::vector<FieldValue>& values) {
  for (const auto& value : values) {
    if (!value.is_valid()) {
      SimpleThrowInvalidArgument("Field value cannot be invalid.");
      return false;
    }
  }
  return true;
}

bool ValidateFieldAndValues(const FieldPath& field, const std::vector<FieldValue>& values) {
  if (!field.is_valid()) {
    SimpleThrowInvalidArgument("Field path cannot be invalid.");
    return false;
  }
  return ValidateValues(values);
}

}  // namespace

Query Query::WhereEqualTo(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereEqualTo(FieldPath::FromDotSeparatedString(field), value) : Query{};
}
Query Query::WhereEqualTo(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value) ? internal_->WhereEqualTo(field, value)
                                                          : Query{};
}

Query Query::WhereNotEqualTo(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereNotEqualTo(FieldPath::FromDotSeparatedString(field), value) : Query{};
}
Query Query::WhereNotEqualTo(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value)
             ? internal_->WhereNotEqualTo(field, value)
             : Query{};
}

Query Query::WhereLessThan(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereLessThan(FieldPath::FromDotSeparatedString(field), value) : Query{};
}
Query Query::WhereLessThan(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value) ? internal_->WhereLessThan(field, value)
                                                          : Query{};
}

Query Query::WhereLessThanOrEqualTo(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereLessThanOrEqualTo(FieldPath::FromDotSeparatedString(field), value)
                   : Query{};
}
Query Query::WhereLessThanOrEqualTo(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value)
             ? internal_->WhereLessThanOrEqualTo(field, value)
             : Query{};
}

Query Query::WhereGreaterThan(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereGreaterThan(FieldPath::FromDotSeparatedString(field), value) : Query{};
}
Query Query::WhereGreaterThan(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value)
             ? internal_->WhereGreaterThan(field, value)
             : Query{};
}

Query Query::WhereGreaterThanOrEqualTo(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereGreaterThanOrEqualTo(FieldPath::FromDotSeparatedString(field), value)
                   : Query{};
}
Query Query::WhereGreaterThanOrEqualTo(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value)
             ? internal_->WhereGreaterThanOrEqualTo(field, value)
             : Query{};
}

Query Query::WhereArrayContains(const std::string& field, const FieldValue& value) const {
  return internal_ ? WhereArrayContains(FieldPath::FromDotSeparatedString(field), value) : Query{};
}
Query Query::WhereArrayContains(const FieldPath& field, const FieldValue& value) const {
  return internal_ && ValidateFieldAndValue(field, value)
             ? internal_->WhereArrayContains(field, value)
             : Query{};
}

Query Query::WhereArrayContainsAny(const std::string& field,
                                   const std::vector<FieldValue>& values) const {
  return internal_ ? WhereArrayContainsAny(FieldPath::FromDotSeparatedString(field), values)
                   : Query{};
}
Query Query::WhereArrayContainsAny(const FieldPath& field,
                                   const std::vector<FieldValue>& values) const {
  return internal_ && ValidateFieldAndValues(field, values)
             ? internal_->WhereArrayContainsAny(field, values)
             : Query{};
}

Query Query::WhereIn(const std::string& field, const std::vector<FieldValue>& values) const {
  return internal_ ? WhereIn(FieldPath::FromDotSeparatedString(field), values) : Query{};
}
Query Query::WhereIn(const FieldPath& field, const std::vector<FieldValue>& values) const {
  return internal_ && ValidateFieldAndValues(field, values) ? internal_->WhereIn(field, values)
                                                            : Query{};
}

Query Query::WhereNotIn(const std::string& field, const std::vector<FieldValue>& values) const {
  return internal_ ? WhereNotIn(FieldPath::FromDotSeparatedString(field), values) : Query{};
}
Query Query::WhereNotIn(const FieldPath& field, const std::vector<FieldValue>& values) const {
  return internal_ && ValidateFieldAndValues(field, values) ? internal_->WhereNotIn(field, values)
                                                            : Query{};
}

Query Query::OrderBy(const std::string& field, Direction direction) const {
  return internal_ ? OrderBy(FieldPath::FromDotSeparatedString(field), direction) : Query{};
}
Query Query::OrderBy(const FieldPath& field, Direction direction) const {
  if (!internal_) return {};
  if (!field.is_valid()) {
    SimpleThrowInvalidArgument("Field path cannot be invalid.");
    return {};
  }
  return internal_->OrderBy(field, direction);
}

Query Query::Limit(int32_t limit) const { return internal_ ? internal_->Limit(limit) : Query{}; }
Query Query::LimitToLast(int32_t limit) const {
  return internal_ ? internal_->LimitToLast(limit) : Query{};
}

Query Query::StartAt(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  if (!snapshot.is_valid()) {
    SimpleThrowInvalidArgument("Document snapshot cannot be invalid.");
    return {};
  }
  return internal_->StartAt(snapshot);
}

Query Query::StartAt(const std::vector<FieldValue>& values) const {
  return internal_ && ValidateValues(values) ? internal_->StartAt(values) : Query{};
}

Query Query::StartAfter(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  if (!snapshot.is_valid()) {
    SimpleThrowInvalidArgument("Document snapshot cannot be invalid.");
    return {};
  }
  return internal_->StartAfter(snapshot);
}

Query Query::StartAfter(const std::vector<FieldValue>& values) const {
  return internal_ && ValidateValues(values) ? internal_->StartAfter(values) : Query{};
}

Query Query::EndBefore(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  if (!snapshot.is_valid()) {
    SimpleThrowInvalidArgument("Document snapshot cannot be invalid.");
    return {};
  }
  return internal_->EndBefore(snapshot);
}

Query Query::EndBefore(const std::vector<FieldValue>& values) const {
  return internal_ && ValidateValues(values) ? internal_->EndBefore(values) : Query{};
}

Query Query::EndAt(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  if (!snapshot.is_valid()) {
    SimpleThrowInvalidArgument("Document snapshot cannot be invalid.");
    return {};
  }
  return internal_->EndAt(snapshot);
}

Query Query::EndAt(const std::vector<FieldValue>& values) const {
  return internal_ && ValidateValues(values) ? internal_->EndAt(values) : Query{};
}

Future<QuerySnapshot> Query::Get(Source source) const {
  return internal_ ? internal_->Get(source) : FailedFuture<QuerySnapshot>();
}

ListenerRegistration Query::AddSnapshotListener(
    std::function<void(const QuerySnapshot&, Error, const std::string&)> callback) {
  return AddSnapshotListener(MetadataChanges::kExclude, std::move(callback));
}

ListenerRegistration Query::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const QuerySnapshot&, Error, const std::string&)> callback) {
  if (!callback) {
    SimpleThrowInvalidArgument("Snapshot listener callback cannot be an empty function.");
    return {};
  }
  return internal_ ? internal_->AddSnapshotListener(metadata_changes, std::move(callback))
                   : ListenerRegistration{};
}

size_t Query::Hash() const { return internal_ ? internal_->Hash() : 0; }

bool operator==(const Query& lhs, const Query& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

}  // namespace firebase::firestore
