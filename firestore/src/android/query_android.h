// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_QUERY_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_QUERY_ANDROID_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/query.h"

namespace firebase::firestore {

class Firestore;

class QueryInternal : public Wrapper {
 public:
  enum class AsyncFn {
    kGet = 0,
    kAdd,
    kCount,
  };

  static void Initialize(jni::Loader& loader);

  QueryInternal(FirestoreInternal* firestore, const jni::Object& object)
      : Wrapper(firestore, object), promises_(firestore) {}

  ~QueryInternal() override = default;
  QueryInternal(const QueryInternal& other) = default;
  QueryInternal(QueryInternal&& other) noexcept = default;

  Firestore* firestore() const;
  AggregateQuery Count() const;
  Query Where(const Filter& filter) const;
  Query WhereEqualTo(const FieldPath& field, const FieldValue& value) const;
  Query WhereNotEqualTo(const FieldPath& field, const FieldValue& value) const;
  Query WhereLessThan(const FieldPath& field, const FieldValue& value) const;
  Query WhereLessThanOrEqualTo(const FieldPath& field,
                               const FieldValue& value) const;
  Query WhereGreaterThan(const FieldPath& field, const FieldValue& value) const;
  Query WhereGreaterThanOrEqualTo(const FieldPath& field,
                                  const FieldValue& value) const;
  Query WhereArrayContains(const FieldPath& field,
                           const FieldValue& value) const;
  Query WhereArrayContainsAny(const FieldPath& field,
                              const std::vector<FieldValue>& values) const;
  Query WhereIn(const FieldPath& field,
                const std::vector<FieldValue>& values) const;
  Query WhereNotIn(const FieldPath& field,
                   const std::vector<FieldValue>& values) const;
  Query OrderBy(const FieldPath& field, Query::Direction direction) const;
  Query Limit(int32_t limit) const;
  Query LimitToLast(int32_t limit) const;
  Query StartAt(const DocumentSnapshot& snapshot) const;
  Query StartAt(const std::vector<FieldValue>& values) const;
  Query StartAfter(const DocumentSnapshot& snapshot) const;
  Query StartAfter(const std::vector<FieldValue>& values) const;
  Query EndBefore(const DocumentSnapshot& snapshot) const;
  Query EndBefore(const std::vector<FieldValue>& values) const;
  Query EndAt(const DocumentSnapshot& snapshot) const;
  Query EndAt(const std::vector<FieldValue>& values) const;

  Future<QuerySnapshot> Get(Source source);

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const QuerySnapshot&, Error, const std::string&)>
          callback);
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      EventListener<QuerySnapshot>* listener);
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::unique_ptr<EventListener<QuerySnapshot>> listener);

  size_t Hash() const;

 protected:
  PromiseFactory<AsyncFn> promises_;

 private:
  friend class FirestoreInternal;

  Query Where(const FieldPath& field, const jni::Method<jni::Object>& method,
              const FieldValue& value) const;
  Query Where(const FieldPath& field, const jni::Method<jni::Object>& method,
              const std::vector<FieldValue>& values) const;
  Query WithBound(const jni::Method<jni::Object>& method,
                  const DocumentSnapshot& snapshot) const;
  Query WithBound(const jni::Method<jni::Object>& method,
                  const std::vector<FieldValue>& values) const;
};

bool operator==(const QueryInternal& lhs, const QueryInternal& rhs);
inline bool operator!=(const QueryInternal& lhs, const QueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_QUERY_ANDROID_H_
