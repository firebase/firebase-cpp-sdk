#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_QUERY_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_QUERY_IOS_H_

#include <cstdint>
#include <vector>

#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/query.h"
#include "firestore/src/ios/firestore_ios.h"
#include "firestore/src/ios/promise_factory_ios.h"
#include "firestore/src/ios/user_data_converter_ios.h"
#include "Firestore/core/src/api/query_core.h"
#include "Firestore/core/src/core/bound.h"
#include "Firestore/core/src/core/order_by.h"
#include "Firestore/core/src/core/query.h"

namespace firebase {
namespace firestore {

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

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener);

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const QuerySnapshot&, Error, const std::string&)>
          callback);

  // Delegating methods

  Query WhereEqualTo(const FieldPath& field, const FieldValue& value) const {
    return Where(field, Operator::Equal, value);
  }

  Query WhereNotEqualTo(const FieldPath& field, const FieldValue& value) const {
    return Where(field, Operator::NotEqual, value);
  }

  Query WhereLessThan(const FieldPath& field, const FieldValue& value) const {
    return Where(field, Operator::LessThan, value);
  }

  Query WhereLessThanOrEqualTo(const FieldPath& field,
                               const FieldValue& value) const {
    return Where(field, Operator::LessThanOrEqual, value);
  }

  Query WhereGreaterThan(const FieldPath& field,
                         const FieldValue& value) const {
    return Where(field, Operator::GreaterThan, value);
  }

  Query WhereGreaterThanOrEqualTo(const FieldPath& field,
                                  const FieldValue& value) const {
    return Where(field, Operator::GreaterThanOrEqual, value);
  }

  Query WhereArrayContains(const FieldPath& field,
                           const FieldValue& value) const {
    return Where(field, Operator::ArrayContains, value);
  }

  Query WhereArrayContainsAny(const FieldPath& field,
                              const std::vector<FieldValue>& values) const {
    return Where(field, Operator::ArrayContainsAny, values);
  }

  Query WhereIn(const FieldPath& field,
                const std::vector<FieldValue>& values) const {
    return Where(field, Operator::In, values);
  }

  Query WhereNotIn(const FieldPath& field,
                   const std::vector<FieldValue>& values) const {
    return Where(field, Operator::NotIn, values);
  }

  Query StartAt(const DocumentSnapshot& snapshot) const {
    return WithBound(BoundPosition::kStartAt, snapshot);
  }

  Query StartAt(const std::vector<FieldValue>& values) const {
    return WithBound(BoundPosition::kStartAt, values);
  }

  Query StartAfter(const DocumentSnapshot& snapshot) const {
    return WithBound(BoundPosition::kStartAfter, snapshot);
  }

  Query StartAfter(const std::vector<FieldValue>& values) const {
    return WithBound(BoundPosition::kStartAfter, values);
  }

  Query EndBefore(const DocumentSnapshot& snapshot) const {
    return WithBound(BoundPosition::kEndBefore, snapshot);
  }

  Query EndBefore(const std::vector<FieldValue>& values) const {
    return WithBound(BoundPosition::kEndBefore, values);
  }

  Query EndAt(const DocumentSnapshot& snapshot) const {
    return WithBound(BoundPosition::kEndAt, snapshot);
  }

  Query EndAt(const std::vector<FieldValue>& values) const {
    return WithBound(BoundPosition::kEndAt, values);
  }

  friend bool operator==(const QueryInternal& lhs, const QueryInternal& rhs);

 protected:
  enum class AsyncApis {
    kGet,
    // Important: `Query` and `CollectionReference` use the same
    // `PromiseFactory`. That is because the most natural thing to register and
    // unregister objects in a `FutureManager` (contained within the
    // `PromiseFactory`) is using the `this` pointer; however, due to
    // inheritance, `Query` and `CollectionReference` are pretty much guaranteed
    // to have the same `this` pointer. Consequently, if both were to have their
    // own `PromiseFactory`, they would either clash when registering, leading
    // to incorrect behavior, or have to come up with some other kind of
    // a handle unique to each object.
    //
    // `Query`, being the base object, is created before the
    // `CollectionReference` part, and destroyed after the `CollectionReference`
    // part; therefore, the `PromiseFactory` is guaranteed to be alive as long
    // as a `CollectionReference` is alive.
    kCollectionReferenceAdd,
    kCount,
  };

  const api::Query& query_core_api() const { return query_; }
  const UserDataConverter& converter() const { return user_data_converter_; }
  PromiseFactory<AsyncApis>& promise_factory() { return promise_factory_; }

 private:
  enum class BoundPosition {
    kStartAt,
    kStartAfter,
    kEndBefore,
    kEndAt,
  };

  using Operator = core::Filter::Operator;

  Query Where(const FieldPath& field, Operator op,
              const FieldValue& value) const;
  Query Where(const FieldPath& field, Operator op,
              const std::vector<FieldValue>& values) const;

  Query WithBound(BoundPosition bound_pos,
                  const DocumentSnapshot& snapshot) const;
  Query WithBound(BoundPosition bound_pos,
                  const std::vector<FieldValue>& values) const;

  model::FieldValue ConvertDocumentId(const model::FieldValue& from,
                                      const core::Query& internal_query) const;

  static bool IsBefore(BoundPosition bound_pos);

  core::Bound ToBound(BoundPosition bound_pos,
                      const DocumentSnapshot& public_snapshot) const;
  core::Bound ToBound(BoundPosition bound_pos,
                      const std::vector<FieldValue>& field_values) const;
  api::Query CreateQueryWithBound(BoundPosition bound_pos,
                                  core::Bound&& bound) const;

  api::Query query_;
  PromiseFactory<AsyncApis> promise_factory_;
  UserDataConverter user_data_converter_;
};

inline bool operator!=(const QueryInternal& lhs, const QueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_QUERY_IOS_H_
