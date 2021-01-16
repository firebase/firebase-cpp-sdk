#include "firestore/src/ios/document_snapshot_ios.h"

#include <utility>

#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/util_ios.h"
#include "absl/memory/memory.h"
#include "Firestore/core/src/api/document_reference.h"
#include "Firestore/core/src/model/database_id.h"
#include "Firestore/core/src/model/document_key.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/model/field_value_options.h"
#include "Firestore/core/src/nanopb/byte_string.h"

namespace firebase {
namespace firestore {

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;
using Type = model::FieldValue::Type;
using model::DocumentKey;

DocumentSnapshotInternal::DocumentSnapshotInternal(
    api::DocumentSnapshot&& snapshot)
    : snapshot_{std::move(snapshot)} {}

Firestore* DocumentSnapshotInternal::firestore() {
  return GetFirestore(&snapshot_);
}

FirestoreInternal* DocumentSnapshotInternal::firestore_internal() {
  return GetFirestoreInternal(&snapshot_);
}

const FirestoreInternal* DocumentSnapshotInternal::firestore_internal() const {
  return GetFirestoreInternal(&snapshot_);
}

const std::string& DocumentSnapshotInternal::id() const {
  return snapshot_.document_id();
}

DocumentReference DocumentSnapshotInternal::reference() const {
  return MakePublic(snapshot_.CreateReference());
}

SnapshotMetadata DocumentSnapshotInternal::metadata() const {
  const auto& result = snapshot_.metadata();
  return SnapshotMetadata{result.pending_writes(), result.from_cache()};
}

bool DocumentSnapshotInternal::exists() const { return snapshot_.exists(); }

MapFieldValue DocumentSnapshotInternal::GetData(
    ServerTimestampBehavior stb) const {
  using Map = model::FieldValue::Map;

  absl::optional<model::ObjectValue> maybe_object = snapshot_.GetData();
  const Map& map =
      maybe_object ? maybe_object.value().GetInternalValue() : Map{};
  FieldValue result = ConvertObject(map, stb);
  HARD_ASSERT_IOS(result.type() == FieldValue::Type::kMap,
                  "Expected snapshot data to parse to a map");
  return result.map_value();
}

FieldValue DocumentSnapshotInternal::Get(const FieldPath& field,
                                         ServerTimestampBehavior stb) const {
  return GetValue(GetInternal(field), stb);
}

FieldValue DocumentSnapshotInternal::GetValue(
    const model::FieldPath& path, ServerTimestampBehavior stb) const {
  absl::optional<model::FieldValue> maybe_value = snapshot_.GetValue(path);
  if (maybe_value) {
    return ConvertAnyValue(std::move(maybe_value).value(), stb);
  } else {
    return FieldValue();
  }
}

// FieldValue parsing

FieldValue DocumentSnapshotInternal::ConvertAnyValue(
    const model::FieldValue& input, ServerTimestampBehavior stb) const {
  switch (input.type()) {
    case Type::Object:
      return ConvertObject(input.object_value(), stb);
    case Type::Array:
      return ConvertArray(input.array_value(), stb);
    default:
      return ConvertScalar(input, stb);
  }
}

FieldValue DocumentSnapshotInternal::ConvertObject(
    const model::FieldValue::Map& object, ServerTimestampBehavior stb) const {
  MapFieldValue result;
  for (const auto& kv : object) {
    result[kv.first] = ConvertAnyValue(kv.second, stb);
  }

  return FieldValue::Map(std::move(result));
}

FieldValue DocumentSnapshotInternal::ConvertArray(
    const model::FieldValue::Array& array, ServerTimestampBehavior stb) const {
  std::vector<FieldValue> result;
  for (const auto& value : array) {
    result.push_back(ConvertAnyValue(value, stb));
  }

  return FieldValue::Array(std::move(result));
}

FieldValue DocumentSnapshotInternal::ConvertScalar(
    const model::FieldValue& scalar, ServerTimestampBehavior stb) const {
  switch (scalar.type()) {
    case Type::Null:
      return FieldValue::Null();
    case Type::Boolean:
      return FieldValue::Boolean(scalar.boolean_value());
    case Type::Integer:
      return FieldValue::Integer(scalar.integer_value());
    case Type::Double:
      return FieldValue::Double(scalar.double_value());
    case Type::String:
      return FieldValue::String(scalar.string_value());
    case Type::Timestamp:
      return FieldValue::Timestamp(scalar.timestamp_value());
    case Type::GeoPoint:
      return FieldValue::GeoPoint(scalar.geo_point_value());
    case Type::Blob:
      return FieldValue::Blob(scalar.blob_value().data(),
                              scalar.blob_value().size());
    case Type::Reference:
      return ConvertReference(scalar.reference_value());
    case Type::ServerTimestamp:
      return ConvertServerTimestamp(scalar.server_timestamp_value(), stb);
    default: {
      // TODO(b/147444199): use string formatting.
      // HARD_FAIL("Unexpected kind of FieldValue: '%s'", scalar.type());
      auto message = std::string("Unexpected kind of FieldValue: '") +
                     std::to_string(static_cast<int>(scalar.type())) + "'";
      HARD_FAIL_IOS(message.c_str());
    }
  }
}

FieldValue DocumentSnapshotInternal::ConvertReference(
    const model::FieldValue::Reference& reference) const {
  HARD_ASSERT_IOS(
      reference.database_id() == firestore_internal()->database_id(),
      "Converted reference is from another database");

  api::DocumentReference api_reference{reference.key(), snapshot_.firestore()};
  return FieldValue::Reference(MakePublic(std::move(api_reference)));
}

FieldValue DocumentSnapshotInternal::ConvertServerTimestamp(
    const model::FieldValue::ServerTimestamp& server_timestamp,
    ServerTimestampBehavior stb) const {
  switch (stb) {
    case ServerTimestampBehavior::kNone:
      return FieldValue::Null();
    case ServerTimestampBehavior::kEstimate: {
      return FieldValue::Timestamp(server_timestamp.local_write_time());
    }
    case ServerTimestampBehavior::kPrevious:
      if (server_timestamp.previous_value()) {
        return ConvertScalar(server_timestamp.previous_value().value(), stb);
      }
      return FieldValue::Null();
  }
  FIRESTORE_UNREACHABLE();
}

}  // namespace firestore
}  // namespace firebase
