#include "firestore/src/ios/query_ios.h"

#include <utility>

#include "app/src/assert.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/document_snapshot_ios.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "firestore/src/ios/listener_ios.h"
#include "firestore/src/ios/promise_ios.h"
#include "firestore/src/ios/set_options_ios.h"
#include "firestore/src/ios/source_ios.h"
#include "firestore/src/ios/util_ios.h"
#include "Firestore/core/src/api/listener_registration.h"
#include "Firestore/core/src/core/filter.h"
#include "Firestore/core/src/core/listen_options.h"
#include "Firestore/core/src/model/document_key.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/model/resource_path.h"
#include "Firestore/core/src/util/exception.h"

namespace firebase {
namespace firestore {

using model::DocumentKey;
using model::ResourcePath;
using util::ThrowInvalidArgumentIos;

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

Query QueryInternal::Where(const FieldPath& field_path, Operator op,
                           const FieldValue& value) const {
  const model::FieldPath& path = GetInternal(field_path);
  model::FieldValue parsed = user_data_converter_.ParseQueryValue(value);
  auto describer = [&value] { return Describe(value.type()); };

  api::Query decorated = query_.Filter(path, op, std::move(parsed), describer);
  return MakePublic(std::move(decorated));
}

Query QueryInternal::Where(const FieldPath& field_path, Operator op,
                           const std::vector<FieldValue>& values) const {
  const model::FieldPath& path = GetInternal(field_path);
  auto array_value = FieldValue::Array(values);
  model::FieldValue parsed =
      user_data_converter_.ParseQueryValue(array_value, true);
  auto describer = [&array_value] { return Describe(array_value.type()); };

  api::Query decorated = query_.Filter(path, op, std::move(parsed), describer);
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
    ThrowInvalidArgumentIos(
        "Invalid query. You are trying to start or end a query using a "
        "document that doesn't exist.");
  }

  const api::DocumentSnapshot& api_snapshot = GetCoreApi(public_snapshot);
  const model::DocumentKey& key =
      api_snapshot.internal_document().value().key();
  const model::DatabaseId& database_id = firestore_internal()->database_id();
  const core::Query& internal_query = query_.query();
  std::vector<model::FieldValue> components;

  // Because people expect to continue/end a query at the exact document
  // provided, we need to use the implicit sort order rather than the explicit
  // sort order, because it's guaranteed to contain the document key. That way
  // the position becomes unambiguous and the query continues/ends exactly at
  // the provided document. Without the key (by using the explicit sort orders),
  // multiple documents could match the position, yielding duplicate results.

  for (const core::OrderBy& order_by : internal_query.order_bys()) {
    const model::FieldPath& field_path = order_by.field();

    if (field_path.IsKeyFieldPath()) {
      components.push_back(model::FieldValue::FromReference(database_id, key));
      continue;
    }

    absl::optional<model::FieldValue> value = api_snapshot.GetValue(field_path);
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
      ThrowInvalidArgumentIos(message.c_str());
    }

    if (value->type() == model::FieldValue::Type::ServerTimestamp) {
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
          "' is an uncommitted server timestamp. (Since the value of this"
          "field is unknown, you cannot start/end a query with it.)";
      ThrowInvalidArgumentIos(message.c_str());
    }

    components.push_back(std::move(value).value());
  }

  return core::Bound{std::move(components), IsBefore(bound_pos)};
}

core::Bound QueryInternal::ToBound(
    BoundPosition bound_pos,
    const std::vector<FieldValue>& field_values) const {
  const core::Query& internal_query = query_.query();
  // Use explicit order bys  because it has to match the query the user made.
  const core::OrderByList& explicit_order_bys =
      internal_query.explicit_order_bys();

  if (field_values.size() > explicit_order_bys.size()) {
    ThrowInvalidArgumentIos(
        "Invalid query. You are trying to start or end a query using more "
        "values than were specified in the order by.");
  }

  std::vector<model::FieldValue> components;

  for (int i = 0; i != field_values.size(); ++i) {
    model::FieldValue field_value =
        user_data_converter_.ParseQueryValue(field_values[i]);
    const core::OrderBy& order_by = explicit_order_bys[i];
    if (order_by.field().IsKeyFieldPath()) {
      components.push_back(ConvertDocumentId(field_value, internal_query));
    } else {
      components.push_back(std::move(field_value));
    }
  }

  return core::Bound{std::move(components), IsBefore(bound_pos)};
}

model::FieldValue QueryInternal::ConvertDocumentId(
    const model::FieldValue& from, const core::Query& internal_query) const {
  if (from.type() != model::FieldValue::Type::String) {
    ThrowInvalidArgumentIos(
        "Invalid query. Expected a string for the document ID.");
  }
  const std::string& document_id = from.string_value();

  if (!internal_query.IsCollectionGroupQuery() &&
      document_id.find('/') != std::string::npos) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidArgument(
    //     "Invalid query. When querying a collection and ordering by document "
    //     "ID, you must pass a plain document ID, but '%s' contains a slash.",
    //     document_id);
    auto message = std::string(
                       "Invalid query. When querying a collection and ordering "
                       "by document "
                       "ID, you must pass a plain document ID, but '") +
                   document_id + "' contains a slash.";
    ThrowInvalidArgumentIos(message.c_str());
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
    ThrowInvalidArgumentIos(message.c_str());
  }

  const model::DatabaseId& database_id = firestore_internal()->database_id();
  return model::FieldValue::FromReference(database_id,
                                          DocumentKey{std::move(path)});
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

bool QueryInternal::IsBefore(BoundPosition bound_pos) {
  switch (bound_pos) {
    case BoundPosition::kStartAt:
    case BoundPosition::kEndBefore:
      return true;

    case BoundPosition::kStartAfter:
    case BoundPosition::kEndAt:
      return false;
  }

  FIRESTORE_UNREACHABLE();
}

}  // namespace firestore
}  // namespace firebase
