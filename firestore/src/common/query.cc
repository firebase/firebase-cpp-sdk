#include "firestore/src/include/firebase/firestore/query.h"

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/event_listener.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/query_stub.h"
#else
#include "firestore/src/ios/query_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnQuery = CleanupFn<Query>;

Query::Query() {}

Query::Query(const Query& query) {
  if (query.internal_) {
    internal_ = new QueryInternal(*query.internal_);
  }
  CleanupFnQuery::Register(this, internal_);
}

Query::Query(Query&& query) {
  CleanupFnQuery::Unregister(&query, query.internal_);
  std::swap(internal_, query.internal_);
  CleanupFnQuery::Register(this, internal_);
}

Query::Query(QueryInternal* internal) : internal_(internal) {
  // NOTE: We don't assert internal != nullptr here since internal can be
  // nullptr when called by the CollectionReference copy constructor.
  CleanupFnQuery::Register(this, internal_);
}

Query::~Query() {
  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

Query& Query::operator=(const Query& query) {
  if (this == &query) {
    return *this;
  }

  CleanupFnQuery::Unregister(this, internal_);
  delete internal_;
  if (query.internal_) {
    internal_ = new QueryInternal(*query.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnQuery::Register(this, internal_);
  return *this;
}

Query& Query::operator=(Query&& query) {
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

const Firestore* Query::firestore() const {
  if (!internal_) return {};
  return internal_->firestore();
}

Firestore* Query::firestore() {
  if (!internal_) return {};
  return internal_->firestore();
}

Query Query::WhereEqualTo(const std::string& field,
                          const FieldValue& value) const {
  return WhereEqualTo(FieldPath::FromDotSeparatedString(field), value);
}

Query Query::WhereEqualTo(const FieldPath& field,
                          const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereEqualTo(field, value);
}

Query Query::WhereNotEqualTo(const std::string& field,
                             const FieldValue& value) const {
  return WhereNotEqualTo(FieldPath::FromDotSeparatedString(field), value);
}

Query Query::WhereNotEqualTo(const FieldPath& field,
                             const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereNotEqualTo(field, value);
}

Query Query::WhereLessThan(const std::string& field,
                           const FieldValue& value) const {
  return WhereLessThan(FieldPath::FromDotSeparatedString(field), value);
}

Query Query::WhereLessThan(const FieldPath& field,
                           const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereLessThan(field, value);
}

Query Query::WhereLessThanOrEqualTo(const std::string& field,
                                    const FieldValue& value) const {
  return WhereLessThanOrEqualTo(FieldPath::FromDotSeparatedString(field),
                                value);
}

Query Query::WhereLessThanOrEqualTo(const FieldPath& field,
                                    const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereLessThanOrEqualTo(field, value);
}

Query Query::WhereGreaterThan(const std::string& field,
                              const FieldValue& value) const {
  return WhereGreaterThan(FieldPath::FromDotSeparatedString(field), value);
}

Query Query::WhereGreaterThan(const FieldPath& field,
                              const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereGreaterThan(field, value);
}

Query Query::WhereGreaterThanOrEqualTo(const std::string& field,
                                       const FieldValue& value) const {
  return WhereGreaterThanOrEqualTo(FieldPath::FromDotSeparatedString(field),
                                   value);
}

Query Query::WhereGreaterThanOrEqualTo(const FieldPath& field,
                                       const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereGreaterThanOrEqualTo(field, value);
}

Query Query::WhereArrayContains(const std::string& field,
                                const FieldValue& value) const {
  return WhereArrayContains(FieldPath::FromDotSeparatedString(field), value);
}

Query Query::WhereArrayContains(const FieldPath& field,
                                const FieldValue& value) const {
  if (!internal_) return {};
  return internal_->WhereArrayContains(field, value);
}

Query Query::WhereArrayContainsAny(
    const std::string& field, const std::vector<FieldValue>& values) const {
  return WhereArrayContainsAny(FieldPath::FromDotSeparatedString(field),
                               values);
}

Query Query::WhereArrayContainsAny(
    const FieldPath& field, const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->WhereArrayContainsAny(field, values);
}

Query Query::WhereIn(const std::string& field,
                     const std::vector<FieldValue>& values) const {
  return WhereIn(FieldPath::FromDotSeparatedString(field), values);
}

Query Query::WhereIn(const FieldPath& field,
                     const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->WhereIn(field, values);
}

Query Query::WhereNotIn(const std::string& field,
                        const std::vector<FieldValue>& values) const {
  return WhereNotIn(FieldPath::FromDotSeparatedString(field), values);
}

Query Query::WhereNotIn(const FieldPath& field,
                        const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->WhereNotIn(field, values);
}

Query Query::OrderBy(const std::string& field, Direction direction) const {
  return OrderBy(FieldPath::FromDotSeparatedString(field), direction);
}

Query Query::OrderBy(const FieldPath& field, Direction direction) const {
  if (!internal_) return {};
  return internal_->OrderBy(field, direction);
}

Query Query::Limit(int32_t limit) const {
  if (!internal_) return {};
  return internal_->Limit(limit);
}

Query Query::LimitToLast(int32_t limit) const {
  if (!internal_) return {};
  return internal_->LimitToLast(limit);
}

Query Query::StartAt(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  return internal_->StartAt(snapshot);
}

Query Query::StartAt(const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->StartAt(values);
}

Query Query::StartAfter(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  return internal_->StartAfter(snapshot);
}

Query Query::StartAfter(const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->StartAfter(values);
}

Query Query::EndBefore(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  return internal_->EndBefore(snapshot);
}

Query Query::EndBefore(const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->EndBefore(values);
}

Query Query::EndAt(const DocumentSnapshot& snapshot) const {
  if (!internal_) return {};
  return internal_->EndAt(snapshot);
}

Query Query::EndAt(const std::vector<FieldValue>& values) const {
  if (!internal_) return {};
  return internal_->EndAt(values);
}

Future<QuerySnapshot> Query::Get(Source source) const {
  if (!internal_) return FailedFuture<QuerySnapshot>();
  return internal_->Get(source);
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
ListenerRegistration Query::AddSnapshotListener(
    std::function<void(const QuerySnapshot&, Error, const std::string&)>
        callback) {
  return AddSnapshotListener(MetadataChanges::kExclude,
                             firebase::Move(callback));
}

ListenerRegistration Query::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const QuerySnapshot&, Error, const std::string&)>
        callback) {
  FIREBASE_ASSERT_MESSAGE(callback, "invalid callback parameter is passed in.");
  if (!internal_) return {};
  return internal_->AddSnapshotListener(metadata_changes,
                                        firebase::Move(callback));
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

#if !defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
ListenerRegistration Query::AddSnapshotListener(
    EventListener<QuerySnapshot>* listener) {
  return AddSnapshotListener(MetadataChanges::kExclude, listener);
}

ListenerRegistration Query::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener) {
  if (!internal_) return {};
  return internal_->AddSnapshotListener(metadata_changes, listener);
}
#endif  // !defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

bool operator==(const Query& lhs, const Query& rhs) {
  if (!lhs.internal_ || !rhs.internal_) {
    return lhs.internal_ == rhs.internal_;
  }

  return lhs.internal_ == rhs.internal_ || *lhs.internal_ == *rhs.internal_;
}

}  // namespace firestore
}  // namespace firebase
