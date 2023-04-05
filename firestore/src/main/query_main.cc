/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/main/query_main.h"

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
#include "firestore/src/main/listener_main.h"
#include "firestore/src/main/promise_main.h"
#include "firestore/src/main/set_options_main.h"
#include "firestore/src/main/source_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

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
      user_data_converter_{&firestore_internal()->database_id()} {}

Firestore* QueryInternal::firestore() { return GetFirestore(&query_); }

FirestoreInternal* QueryInternal::firestore_internal() {
  return GetFirestoreInternal(&query_);
}

const FirestoreInternal* QueryInternal::firestore_internal() const {
  return GetFirestoreInternal(&query_);
}

Query QueryInternal::OrderBy(const FieldPath& field_path,
                             Query::Direction direction) const {
  api::Query decorated = query_.OrderBy(
      GetInternal(field_path), direction == Query::Direction::kDescending);
  return MakePublic(std::move(decorated));
}

Query QueryInternal::Limit(int32_t limit) const {
  return MakePublic(query_.LimitToFirst(limit));
}

Query QueryInternal::LimitToLast(int32_t limit) const {
  return MakePublic(query_.LimitToLast(limit));
}

Future<QuerySnapshot> QueryInternal::Get(Source source) {
  auto promise = promise_factory_.CreatePromise<QuerySnapshot>(AsyncApis::kGet);
  auto listener = ListenerWithPromise<api::QuerySnapshot>(promise);
  query_.GetDocuments(ToCoreApi(source), std::move(listener));
  return promise.future();
}

AggregateQuery QueryInternal::Count() { return MakePublic(query_.Count()); }

Query QueryInternal::Where(const FieldPath& field_path,
                           Operator op,
                           const FieldValue& value) const {
  const model::FieldPath& path = GetInternal(field_path);
  Message<google_firestore_v1_Value> parsed =
      user_data_converter_.ParseQueryValue(value);
  auto describer = [&value] { return Describe(value.type()); };

  api::Query decorated = query_.AddNewFilter(
      query_.ParseFieldFilter(path, op, std::move(parsed), describer));
  return MakePublic(std::move(decorated));
}

Query QueryInternal::Where(const FieldPath& field_path,
                           Operator op,
                           const std::vector<FieldValue>& values) const {
  const model::FieldPath& path = GetInternal(field_path);
  auto array_value = FieldValue::Array(values);
  Message<google_firestore_v1_Value> parsed =
      user_data_converter_.ParseQueryValue(array_value, true);
  auto describer = [&array_value] { return Describe(array_value.type()); };

  api::Query decorated = query_.AddNewFilter(
      query_.ParseFieldFilter(path, op, std::move(parsed), describer));
  return MakePublic(std::move(decorated));
}

Query QueryInternal::WithBound(BoundPosition bound_pos,
                               const DocumentSnapshot& snapshot) const {
  core::Bound bound = ToBound(bound_pos, snapshot);
  return MakePublic(CreateQueryWithBound(bound_pos, std::move(bound)));
}

Query QueryInternal::WithBound(BoundPosition bound_pos,
                               const std::vector<FieldValue>& values) const {
  core::Bound bound = ToBound(bound_pos, values);
  return MakePublic(CreateQueryWithBound(bound_pos, std::move(bound)));
}

ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes, EventListener<QuerySnapshot>* listener) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  auto result = query_.AddSnapshotListener(
      options, ListenerWithEventListener<api::QuerySnapshot>(listener));
  return MakePublic(std::move(result), firestore_internal());
}

ListenerRegistration QueryInternal::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const QuerySnapshot&, Error, const std::string&)>
        callback) {
  auto options = core::ListenOptions::FromIncludeMetadataChanges(
      metadata_changes == MetadataChanges::kInclude);
  auto result = query_.AddSnapshotListener(
      options, ListenerWithCallback<api::QuerySnapshot>(std::move(callback)));
  return MakePublic(std::move(result), firestore_internal());
}

bool operator==(const QueryInternal& lhs, const QueryInternal& rhs) {
  return lhs.query_ == rhs.query_;
}

core::Bound QueryInternal::ToBound(
    BoundPosition bound_pos, const DocumentSnapshot& public_snapshot) const {
  if (!public_snapshot.exists()) {
    SimpleThrowInvalidArgument(
        "Invalid query. You are trying to start or end a query using a "
        "document that doesn't exist.");
  }

  const api::DocumentSnapshot& api_snapshot = GetCoreApi(public_snapshot);
  const model::DocumentKey& key =
      api_snapshot.internal_document().value()->key();
  const model::DatabaseId& database_id = firestore_internal()->database_id();
  const core::Query& internal_query = query_.query();

  SharedMessage<google_firestore_v1_ArrayValue> components{{}};
  components->values_count = CheckedSize(internal_query.order_bys().size());
  components->values =
      MakeArray<google_firestore_v1_Value>(components->values_count);

  // Because people expect to continue/end a query at the exact document
  // provided, we need to use the implicit sort order rather than the explicit
  // sort order, because it's guaranteed to contain the document key. That way
  // the position becomes unambiguous and the query continues/ends exactly at
  // the provided document. Without the key (by using the explicit sort orders),
  // multiple documents could match the position, yielding duplicate results.

  for (size_t i = 0; i < internal_query.order_bys().size(); ++i) {
    const core::OrderBy& order_by = internal_query.order_bys()[i];
    const model::FieldPath& field_path = order_by.field();

    if (field_path.IsKeyFieldPath()) {
      components->values[i] = *RefValue(database_id, key).release();
      continue;
    }

    absl::optional<google_firestore_v1_Value> value =
        api_snapshot.GetValue(field_path);
    if (!value) {
      // TODO(b/147444199): use string formatting.
      // ThrowInvalidArgument(
      //     "Invalid query. You are trying to start or end a query using a "
      //     "document for which the field '%s' (used as the order by) does not
      //     " "exist.", field_path.CanonicalString());
      auto message = std::string(
                         "Invalid query. You are trying to start or end a "
                         "query using a document for which the field '") +
                     field_path.CanonicalString() +
                     "' (used as the order by) does not exist.";
      SimpleThrowInvalidArgument(message);
    }

    if (IsServerTimestamp(*value)) {
      // TODO(b/147444199): use string formatting.
      // ThrowInvalidArgument(
      //     "Invalid query. You are trying to start or end a query using a "
      //     "document for which the field '%s' is an uncommitted server "
      //     "timestamp. (Since the value of this field is unknown, you cannot "
      //     "start/end a query with it.)",
      //     field_path.CanonicalString());
      auto message =
          std::string(
              "Invalid query. You are trying to start or end a query using a "
              "document for which the field '") +
          field_path.CanonicalString() +
          "' is an uncommitted server timestamp. (Since the value of this "
          "field is unknown, you cannot start/end a query with it.)";
      SimpleThrowInvalidArgument(message);
    }

    components->values[i] = *DeepClone(*value).release();
  }

  return core::Bound::FromValue(std::move(components), IsInclusive(bound_pos));
}

core::Bound QueryInternal::ToBound(
    BoundPosition bound_pos,
    const std::vector<FieldValue>& field_values) const {
  const core::Query& internal_query = query_.query();
  // Use explicit order bys  because it has to match the query the user made.
  const std::vector<core::OrderBy>& explicit_order_bys =
      internal_query.explicit_order_bys();

  if (field_values.size() > explicit_order_bys.size()) {
    SimpleThrowInvalidArgument(
        "Invalid query. You are trying to start or end a query using more "
        "values than were specified in the order by.");
  }

  SharedMessage<google_firestore_v1_ArrayValue> components{{}};
  components->values_count = CheckedSize(field_values.size());
  components->values =
      MakeArray<google_firestore_v1_Value>(components->values_count);

  for (int i = 0; i != field_values.size(); ++i) {
    Message<google_firestore_v1_Value> field_value =
        user_data_converter_.ParseQueryValue(field_values[i]);
    const core::OrderBy& order_by = explicit_order_bys[i];
    if (order_by.field().IsKeyFieldPath()) {
      components->values[i] =
          *ConvertDocumentId(field_value, internal_query).release();
    } else {
      components->values[i] = *field_value.release();
    }
  }

  return core::Bound::FromValue(std::move(components), IsInclusive(bound_pos));
}

Message<google_firestore_v1_Value> QueryInternal::ConvertDocumentId(
    const Message<google_firestore_v1_Value>& from,
    const core::Query& internal_query) const {
  if (GetTypeOrder(*from) != TypeOrder::kString) {
    SimpleThrowInvalidArgument(
        "Invalid query. Expected a string for the document ID.");
  }

  std::string document_id = MakeString(from->string_value);

  if (!internal_query.IsCollectionGroupQuery() &&
      document_id.find('/') != std::string::npos) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidArgument(
    //     "Invalid query. When querying a collection and ordering by document "
    //     "ID, you must pass a plain document ID, but '%s' contains a slash.",
    //     document_id);
    auto message =
        std::string(
            "Invalid query. When querying a collection and ordering "
            "by document ID, you must pass a plain document ID, but '") +
        document_id + "' contains a slash.";
    SimpleThrowInvalidArgument(message);
  }

  ResourcePath path =
      internal_query.path().Append(ResourcePath::FromString(document_id));
  if (!DocumentKey::IsDocumentKey(path)) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidArgument(
    //     "Invalid query. When querying a collection group and ordering by "
    //     "document ID, you must pass a value that results in a valid document
    //     " "path, but '%s' is not because it contains an odd number of
    //     segments.", path.CanonicalString());
    auto message =
        std::string(
            "Invalid query. When querying a collection group and ordering by "
            "document ID, you must pass a value that results in a valid "
            "document path, but '") +
        path.CanonicalString() +
        "' is not because it contains an odd number of segments.";
    SimpleThrowInvalidArgument(message);
  }

  const model::DatabaseId& database_id = firestore_internal()->database_id();
  return RefValue(database_id, DocumentKey{std::move(path)});
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

}  // namespace firestore
}  // namespace firebase
