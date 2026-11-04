// Copyright 2021 Google LLC
#include "firestore/src/main/query_main.h"

#include <cassert>
#include <cstdint>
#include <utility>
#include "Firestore/core/src/api/aggregate_query.h"
#include "Firestore/core/src/api/listener_registration.h"
#include "Firestore/core/src/core/filter.h"
#include "Firestore/core/src/core/listen_options.h"
#include "Firestore/core/src/model/document_key.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/model/resource_path.h"
#include "Firestore/core/src/model/server_timestamp_util.h"
#include "Firestore/core/src/model/value_util.h"
#include "Firestore/core/src/nanopb/nanopb_util.h"
#include "Firestore/core/src/util/exception.h"
#include "app/src/assert.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/aggregate_query_main.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/filter_main.h"
#include "firestore/src/main/listener_main.h"
#include "firestore/src/main/promise_main.h"
#include "firestore/src/main/set_options_main.h"
#include "firestore/src/main/source_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase::firestore {

using model::DeepClone;
using model::DocumentKey;
using model::GetTypeOrder;
using model::IsServerTimestamp;
using model::RefValue;
using model::ResourcePath;
using model::TypeOrder;
using nanopb::CheckedSize;
using nanopb::MakeArray;
using nanopb::MakeString;
using nanopb::Message;
using nanopb::SharedMessage;

QueryInternal::QueryInternal(api::Query&& query)
    : query_{std::move(query)},
      promise_factory_{PromiseFactory<AsyncApis>::Create(this)},
      user_data_converter_{&query_.firestore()->database_id()} {}

Firestore* QueryInternal::firestore() { return GetFirestore(&query_); }
FirestoreInternal* QueryInternal::firestore_internal() { return GetFirestoreInternal(&query_); }
const FirestoreInternal* QueryInternal::firestore_internal() const {
  return GetFirestoreInternal(&query_);
}

Query QueryInternal::OrderBy(const FieldPath& field_path, Query::Direction direction) const {
  return MakePublic(
      query_.OrderBy(GetInternal(field_path), direction == Query::Direction::kDescending));
}

Query QueryInternal::Limit(int32_t limit) const { return MakePublic(query_.LimitToFirst(limit)); }
Query QueryInternal::LimitToLast(int32_t limit) const {
  return MakePublic(query_.LimitToLast(limit));
}

Future<QuerySnapshot> QueryInternal::Get(Source source) {
  auto promise = promise_factory_.CreatePromise<QuerySnapshot>(AsyncApis::kGet);
  query_.GetDocuments(ToCoreApi(source), ListenerWithPromise<api::QuerySnapshot>(promise));
  return promise.future();
}

AggregateQuery QueryInternal::Count() const { return MakePublic(query_.Count()); }

Query QueryInternal::Where(const Filter& filter) const {
  if (filter.IsEmpty()) return MakePublic(api::Query{query_});
  return MakePublic(
      query_.AddNewFilter(GetInternal(&filter)->ToCoreFilter(query_, user_data_converter_)));
}

Query QueryInternal::Where(const FieldPath& field_path, Operator op,
                           const FieldValue& value) const {
  return Where(UnaryFilterInternal::UnaryFilter(field_path, op, value));
}

Query QueryInternal::Where(const FieldPath& field_path, Operator op,
                           const std::vector<FieldValue>& values) const {
  return Where(UnaryFilterInternal::UnaryFilter(field_path, op, values));
}

Query QueryInternal::WithBound(BoundPosition bound_pos, const DocumentSnapshot& snapshot) const {
  return MakePublic(CreateQueryWithBound(bound_pos, ToBound(bound_pos, snapshot)));
}

Query QueryInternal::WithBound(BoundPosition bound_pos,
                               const std::vector<FieldValue>& values) const {
  return MakePublic(CreateQueryWithBound(bound_pos, ToBound(bound_pos, values)));
}

ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  return MakePublic(
      query_.AddSnapshotListener(options, ListenerWithEventListener<api::QuerySnapshot>(listener)),
      firestore_internal());
}

ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const QuerySnapshot&, Error, const std::string&)> callback) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  return MakePublic(query_.AddSnapshotListener(
                        options, ListenerWithCallback<api::QuerySnapshot>(std::move(callback))),
                    firestore_internal());
}

bool operator==(const QueryInternal& lhs, const QueryInternal& rhs) {
  return lhs.query_ == rhs.query_;
}

core::Bound QueryInternal::ToBound(BoundPosition bound_pos,
                                   const DocumentSnapshot& public_snapshot) const {
  if (!public_snapshot.exists()) {
    SimpleThrowInvalidArgument(
        "Invalid query. You are trying to start or end a query using a document that doesn't exist.");
  }
  const api::DocumentSnapshot& api_snapshot = GetCoreApi(public_snapshot);
  const model::DatabaseId& database_id = query_.firestore()->database_id();
  if (api_snapshot.firestore()->database_id() != database_id) {
    SimpleThrowInvalidArgument(
        "Invalid query. You are trying to start or end a query using a document from a different database.");
  }
  const model::DocumentKey& key = api_snapshot.internal_document().value()->key();
  const core::Query& internal_query = query_.query();

  SharedMessage<google_firestore_v1_ArrayValue> components{{}};
  components->values_count = CheckedSize(internal_query.normalized_order_bys().size());
  components->values = MakeArray<google_firestore_v1_Value>(components->values_count);

  for (size_t i = 0; i < internal_query.normalized_order_bys().size(); ++i) {
    const model::FieldPath& field_path = internal_query.normalized_order_bys()[i].field();
    if (field_path.IsKeyFieldPath()) {
      components->values[i] = *RefValue(database_id, key).release();
      continue;
    }
    absl::optional<google_firestore_v1_Value> value = api_snapshot.GetValue(field_path);
    if (!value) {
      SimpleThrowInvalidArgument(
          "Invalid query. You are trying to start or end a query using a document for which the field '" +
          field_path.CanonicalString() + "' (used as the order by) does not exist.");
    }
    if (IsServerTimestamp(*value)) {
      SimpleThrowInvalidArgument(
          "Invalid query. You are trying to start or end a query using a document for which the field '" +
          field_path.CanonicalString() +
          "' is an uncommitted server timestamp. (Since the value of this field is unknown, you cannot start/end a query with it.)");
    }
    components->values[i] = *DeepClone(*value).release();
  }
  return core::Bound::FromValue(std::move(components), IsInclusive(bound_pos));
}

core::Bound QueryInternal::ToBound(BoundPosition bound_pos,
                                   const std::vector<FieldValue>& field_values) const {
  const core::Query& internal_query = query_.query();
  const std::vector<core::OrderBy>& explicit_order_bys = internal_query.explicit_order_bys();
  if (field_values.size() > explicit_order_bys.size()) {
    SimpleThrowInvalidArgument(
        "Invalid query. You are trying to start or end a query using more values than were specified in the order by.");
  }

  SharedMessage<google_firestore_v1_ArrayValue> components{{}};
  components->values_count = CheckedSize(field_values.size());
  components->values = MakeArray<google_firestore_v1_Value>(components->values_count);

  for (size_t i = 0; i != field_values.size(); ++i) {
    Message<google_firestore_v1_Value> field_value =
        user_data_converter_.ParseQueryValue(field_values[i]);
    if (explicit_order_bys[i].field().IsKeyFieldPath()) {
      components->values[i] = *ConvertDocumentId(field_value, internal_query).release();
    } else {
      components->values[i] = *field_value.release();
    }
  }
  return core::Bound::FromValue(std::move(components), IsInclusive(bound_pos));
}

Message<google_firestore_v1_Value> QueryInternal::ConvertDocumentId(
    const Message<google_firestore_v1_Value>& from, const core::Query& internal_query) const {
  if (GetTypeOrder(*from) != TypeOrder::kString) {
    SimpleThrowInvalidArgument("Invalid query. Expected a string for the document ID.");
  }
  std::string document_id = MakeString(from->string_value);
  if (document_id.empty()) {
    SimpleThrowInvalidArgument("Invalid query. Document ID must not be an empty string.");
  }
  if (!internal_query.IsCollectionGroupQuery() && document_id.find('/') != std::string::npos) {
    SimpleThrowInvalidArgument(
        "Invalid query. When querying a collection and ordering by document ID, you must pass a plain document ID, but '" +
        document_id + "' contains a slash.");
  }
  ResourcePath path = internal_query.path().Append(ResourcePath::FromString(document_id));
  if (!DocumentKey::IsDocumentKey(path)) {
    SimpleThrowInvalidArgument(
        "Invalid query. When querying a collection group and ordering by document ID, you must pass a value that results in a valid document path, but '" +
        path.CanonicalString() + "' is not because it contains an odd number of segments.");
  }
  return RefValue(query_.firestore()->database_id(), DocumentKey{std::move(path)});
}

api::Query QueryInternal::CreateQueryWithBound(BoundPosition bound_pos,
                                               core::Bound&& bound) const {
  switch (bound_pos) {
    case BoundPosition::kStartAt:
    case BoundPosition::kStartAfter:
      return query_.StartAt(std::move(bound));
    case BoundPosition::kEndBefore:
    case BoundPosition::kEndAt:
      return query_.EndAt(std::move(bound));
  }
  FIRESTORE_UNREACHABLE();
}

bool QueryInternal::IsInclusive(BoundPosition bound_pos) {
  switch (bound_pos) {
    case BoundPosition::kStartAt:
    case BoundPosition::kEndAt:
      return true;
    case BoundPosition::kStartAfter:
    case BoundPosition::kEndBefore:
      return false;
  }
  FIRESTORE_UNREACHABLE();
}

}  // namespace firebase::firestore
