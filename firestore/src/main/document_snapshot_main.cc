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

#include "firestore/src/main/document_snapshot_main.h"

#include <utility>

#include "Firestore/core/src/api/document_reference.h"
#include "Firestore/core/src/model/database_id.h"
#include "Firestore/core/src/model/document_key.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/model/server_timestamp_util.h"
#include "Firestore/core/src/nanopb/byte_string.h"
#include "Firestore/core/src/nanopb/message.h"
#include "Firestore/core/src/nanopb/nanopb_util.h"
#include "absl/memory/memory.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;
using model::DatabaseId;
using model::DocumentKey;
using model::GetLocalWriteTime;
using model::GetPreviousValue;
using model::IsServerTimestamp;
using nanopb::MakeString;
using nanopb::Message;

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
  absl::optional<google_firestore_v1_Value> data =
      snapshot_.GetValue(model::FieldPath::EmptyPath());
  if (!data) return MapFieldValue{};

  FieldValue result = ConvertObject(data->map_value, stb);
  SIMPLE_HARD_ASSERT(result.type() == FieldValue::Type::kMap,
                     "Expected snapshot data to parse to a map");
  return result.map_value();
}

FieldValue DocumentSnapshotInternal::Get(const FieldPath& field,
                                         ServerTimestampBehavior stb) const {
  return GetValue(GetInternal(field), stb);
}

FieldValue DocumentSnapshotInternal::GetValue(
    const model::FieldPath& path, ServerTimestampBehavior stb) const {
  absl::optional<google_firestore_v1_Value> maybe_value =
      snapshot_.GetValue(path);
  if (maybe_value) {
    return ConvertAnyValue(std::move(maybe_value).value(), stb);
  } else {
    return FieldValue();
  }
}

// FieldValue parsing

FieldValue DocumentSnapshotInternal::ConvertAnyValue(
    const google_firestore_v1_Value& input, ServerTimestampBehavior stb) const {
  switch (input.which_value_type) {
    case google_firestore_v1_Value_map_value_tag:
      if (IsServerTimestamp(input)) {
        return ConvertServerTimestamp(input, stb);
      }
      return ConvertObject(input.map_value, stb);
    case google_firestore_v1_Value_array_value_tag:
      return ConvertArray(input.array_value, stb);
    default:
      return ConvertScalar(input, stb);
  }
}

FieldValue DocumentSnapshotInternal::ConvertObject(
    const google_firestore_v1_MapValue& object,
    ServerTimestampBehavior stb) const {
  MapFieldValue result;
  for (pb_size_t i = 0; i < object.fields_count; ++i) {
    std::string key = MakeString(object.fields[i].key);
    const google_firestore_v1_Value& value = object.fields[i].value;
    result[std::move(key)] = ConvertAnyValue(value, stb);
  }

  return FieldValue::Map(std::move(result));
}

FieldValue DocumentSnapshotInternal::ConvertArray(
    const google_firestore_v1_ArrayValue& array,
    ServerTimestampBehavior stb) const {
  std::vector<FieldValue> result;
  for (pb_size_t i = 0; i < array.values_count; ++i) {
    result.push_back(ConvertAnyValue(array.values[i], stb));
  }

  return FieldValue::Array(std::move(result));
}

FieldValue DocumentSnapshotInternal::ConvertScalar(
    const google_firestore_v1_Value& scalar,
    ServerTimestampBehavior stb) const {
  switch (scalar.which_value_type) {
    case google_firestore_v1_Value_null_value_tag:
      return FieldValue::Null();
    case google_firestore_v1_Value_boolean_value_tag:
      return FieldValue::Boolean(scalar.boolean_value);
    case google_firestore_v1_Value_integer_value_tag:
      return FieldValue::Integer(scalar.integer_value);
    case google_firestore_v1_Value_double_value_tag:
      return FieldValue::Double(scalar.double_value);
    case google_firestore_v1_Value_string_value_tag:
      return FieldValue::String(MakeString(scalar.string_value));
    case google_firestore_v1_Value_timestamp_value_tag:
      return FieldValue::Timestamp(Timestamp(scalar.timestamp_value.seconds,
                                             scalar.timestamp_value.nanos));
    case google_firestore_v1_Value_geo_point_value_tag:
      return FieldValue::GeoPoint(GeoPoint(scalar.geo_point_value.latitude,
                                           scalar.geo_point_value.longitude));
    case google_firestore_v1_Value_bytes_value_tag:
      return scalar.bytes_value ? FieldValue::Blob(scalar.bytes_value->bytes,
                                                   scalar.bytes_value->size)
                                : FieldValue::Blob(nullptr, 0);
    case google_firestore_v1_Value_reference_value_tag:
      return ConvertReference(scalar);
    default: {
      // TODO(b/147444199): use string formatting.
      // HARD_FAIL("Unexpected kind of FieldValue: '%s'", scalar.type());
      auto message = std::string("Unexpected kind of FieldValue");
      SIMPLE_HARD_FAIL(message);
    }
  }
}

FieldValue DocumentSnapshotInternal::ConvertReference(
    const google_firestore_v1_Value& reference) const {
  std::string ref = MakeString(reference.reference_value);
  DatabaseId database_id = DatabaseId::FromName(ref);
  DocumentKey key = DocumentKey::FromName(ref);

  SIMPLE_HARD_ASSERT(database_id == firestore_internal()->database_id(),
                     "Converted reference is from another database");

  api::DocumentReference api_reference{std::move(key), snapshot_.firestore()};
  return FieldValue::Reference(MakePublic(std::move(api_reference)));
}

FieldValue DocumentSnapshotInternal::ConvertServerTimestamp(
    const google_firestore_v1_Value& server_timestamp,
    ServerTimestampBehavior stb) const {
  switch (stb) {
    case ServerTimestampBehavior::kNone:
      return FieldValue::Null();
    case ServerTimestampBehavior::kEstimate: {
      google_protobuf_Timestamp timestamp = GetLocalWriteTime(server_timestamp);
      return FieldValue::Timestamp(
          Timestamp(timestamp.seconds, timestamp.nanos));
    }
    case ServerTimestampBehavior::kPrevious: {
      absl::optional<google_firestore_v1_Value> previous_value =
          GetPreviousValue(server_timestamp);
      if (previous_value) {
        return ConvertScalar(*previous_value, stb);
      }
      return FieldValue::Null();
    }
  }
  FIRESTORE_UNREACHABLE();
}

bool operator==(const DocumentSnapshotInternal& lhs,
                const DocumentSnapshotInternal& rhs) {
  return lhs.snapshot_ == rhs.snapshot_;
}

}  // namespace firestore
}  // namespace firebase
