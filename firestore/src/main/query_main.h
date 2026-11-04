// Copyright 2021 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_QUERY_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_QUERY_MAIN_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Firestore/Protos/nanopb/google/firestore/v1/document.nanopb.h"
#include "Firestore/core/src/api/query_core.h"
#include "Firestore/core/src/core/bound.h"
#include "Firestore/core/src/core/field_filter.h"
#include "Firestore/core/src/core/order_by.h"
#include "Firestore/core/src/core/query.h"
#include "Firestore/core/src/nanopb/message.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/filter.h"
#include "firestore/src/include/firebase/firestore/query.h"
#include "firestore/src/main/firestore_main.h"
#include "firestore/src/main/promise_factory_main.h"
#include "firestore/src/main/user_data_converter_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase::firestore {

class Firestore;

class QueryInternal {
 public:
  explicit QueryInternal(api::Query&& query);
  virtual ~QueryInternal() = default;

  Firestore* firestore();
  FirestoreInternal* firestore_internal();
  const FirestoreInternal* firestore_internal() const;

  Query OrderBy(const FieldPath& field, Query::Direction direction) const;
  Query Limit(int32_t limit) const;
  Query LimitToLast(int32_t limit) const;

  virtual Future<QuerySnapshot> Get(Source source);
  AggregateQuery Count() const;

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener);
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const QuerySnapshot&, Error, const std::string&)> callback);

  Query Where(const Filter& filter) const;
  Query WhereEqualTo(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::Equal, v); }
  Query WhereNotEqualTo(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::NotEqual, v); }
  Query WhereLessThan(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::LessThan, v); }
  Query WhereLessThanOrEqualTo(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::LessThanOrEqual, v); }
  Query WhereGreaterThan(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::GreaterThan, v); }
  Query WhereGreaterThanOrEqualTo(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::GreaterThanOrEqual, v); }
  Query WhereArrayContains(const FieldPath& f, const FieldValue& v) const { return Where(f, Operator::ArrayContains, v); }
  Query WhereArrayContainsAny(const FieldPath& f, const std::vector<FieldValue>& v) const { return Where(f, Operator::ArrayContainsAny, v); }
  Query WhereIn(const FieldPath& f, const std::vector<FieldValue>& v) const { return Where(f, Operator::In, v); }
  Query WhereNotIn(const FieldPath& f, const std::vector<FieldValue>& v) const { return Where(f, Operator::NotIn, v); }

  Query StartAt(const DocumentSnapshot& s) const { return WithBound(BoundPosition::kStartAt, s); }
  Query StartAt(const std::vector<FieldValue>& v) const { return WithBound(BoundPosition::kStartAt, v); }
  Query StartAfter(const DocumentSnapshot& s) const { return WithBound(BoundPosition::kStartAfter, s); }
  Query StartAfter(const std::vector<FieldValue>& v) const { return WithBound(BoundPosition::kStartAfter, v); }
  Query EndBefore(const DocumentSnapshot& s) const { return WithBound(BoundPosition::kEndBefore, s); }
  Query EndBefore(const std::vector<FieldValue>& v) const { return WithBound(BoundPosition::kEndBefore, v); }
  Query EndAt(const DocumentSnapshot& s) const { return WithBound(BoundPosition::kEndAt, s); }
  Query EndAt(const std::vector<FieldValue>& v) const { return WithBound(BoundPosition::kEndAt, v); }

  size_t Hash() const { return query_.Hash(); }
  friend bool operator==(const QueryInternal& lhs, const QueryInternal& rhs);

 protected:
  enum class AsyncApis {
    kGet,
    kDocumentAdd,
    kCount,
  };

  const api::Query& query_core_api() const { return query_; }
  const UserDataConverter& converter() const { return user_data_converter_; }
  PromiseFactory<AsyncApis>& promise_factory() { return promise_factory_; }

 private:
  enum class BoundPosition { kStartAt, kStartAfter, kEndBefore, kEndAt };
  using Operator = core::FieldFilter::Operator;

  Query Where(const FieldPath& field, Operator op, const FieldValue& value) const;
  Query Where(const FieldPath& field, Operator op, const std::vector<FieldValue>& values) const;
  Query WithBound(BoundPosition bound_pos, const DocumentSnapshot& snapshot) const;
  Query WithBound(BoundPosition bound_pos, const std::vector<FieldValue>& values) const;

  nanopb::Message<google_firestore_v1_Value> ConvertDocumentId(
      const nanopb::Message<google_firestore_v1_Value>& from,
      const core::Query& internal_query) const;
  static bool IsInclusive(BoundPosition bound_pos);

  core::Bound ToBound(BoundPosition bound_pos, const DocumentSnapshot& public_snapshot) const;
  core::Bound ToBound(BoundPosition bound_pos, const std::vector<FieldValue>& field_values) const;
  api::Query CreateQueryWithBound(BoundPosition bound_pos, core::Bound&& bound) const;

  api::Query query_;
  PromiseFactory<AsyncApis> promise_factory_;
  UserDataConverter user_data_converter_;
};

inline bool operator!=(const QueryInternal& lhs, const QueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_QUERY_MAIN_H_
