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

#include "firestore/src/main/user_data_converter_main.h"

#include <set>
#include <string>

#include "Firestore/core/src/core/user_data.h"
#include "Firestore/core/src/model/field_mask.h"
#include "Firestore/core/src/model/transform_operation.h"
#include "Firestore/core/src/model/value_util.h"
#include "Firestore/core/src/nanopb/byte_string.h"
#include "Firestore/core/src/nanopb/nanopb_util.h"
#include "Firestore/core/src/util/exception.h"
#include "absl/memory/memory.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/field_value_main.h"
#include "firestore/src/main/set_options_main.h"

namespace firebase {
namespace firestore {

namespace {

using core::ParseAccumulator;
using core::ParseContext;
using core::ParsedSetData;
using core::ParsedUpdateData;
using core::UserDataSource;
using model::ArrayTransform;
using model::DatabaseId;
using model::FieldMask;
using model::NullValue;
using model::NumericIncrementTransform;
using model::ObjectValue;
using model::ServerTimestampTransform;
using model::TransformOperation;
using nanopb::ByteString;
using nanopb::CheckedSize;
using nanopb::MakeArray;
using nanopb::Message;

using Type = FieldValue::Type;

FIRESTORE_ATTRIBUTE_NORETURN
void ThrowInvalidData(const ParseContext& context, const std::string& message) {
  std::string full_message =
      "Invalid data. " + message + context.FieldDescription();
  SimpleThrowInvalidArgument(full_message);
}

bool IsSentinel(const FieldValue& value) {
  switch (value.type()) {
    case Type::kDelete:
    case Type::kServerTimestamp:
    case Type::kArrayUnion:
    case Type::kArrayRemove:
    case Type::kIncrementDouble:
    case Type::kIncrementInteger:
      return true;

    default:
      return false;
  }
}

void ParseDelete(ParseContext&& context) {
  if (context.data_source() == UserDataSource::MergeSet) {
    // No transform to add for a delete, but we need to add it to our field mask
    // so it gets deleted.
    context.AddToFieldMask(*context.path());
    return;
  }

  if (context.data_source() == UserDataSource::Update) {
    SIMPLE_HARD_ASSERT(
        !context.path()->empty(),
        "FieldValue.Delete() at the top level should have already been "
        "handled.");
    ThrowInvalidData(context,
                     "FieldValue::Delete() can only appear at the top level of "
                     "your update data");
  }

  // We shouldn't encounter delete sentinels for queries or non-merge `Set`
  // calls.
  ThrowInvalidData(context,
                   "FieldValue::Delete() can only be used with Update() and "
                   "Set() with merge == true");
}

void ParseServerTimestamp(ParseContext&& context) {
  context.AddToFieldTransforms(*context.path(), ServerTimestampTransform{});
}

void ParseArrayTransform(Type type,
                         Message<google_firestore_v1_ArrayValue> elements,
                         ParseContext&& context) {
  auto transform_type = [type] {
    switch (type) {
      case Type::kArrayUnion:
        return TransformOperation::Type::ArrayUnion;
      case Type::kArrayRemove:
        return TransformOperation::Type::ArrayRemove;
      default: {
        // TODO(b/147444199): use string formatting.
        // HARD_FAIL("Unexpected type '%s' given to ParseArrayTransform", type);
        auto message = std::string("Unexpected type '") +
                       std::to_string(static_cast<int>(type)) +
                       "' given to ParseArrayTransform";
        SIMPLE_HARD_FAIL(message);
      }
    }
  }();

  context.AddToFieldTransforms(
      *context.path(), ArrayTransform{transform_type, std::move(elements)});
}

void ParseNumericIncrement(const FieldValue& value, ParseContext&& context) {
  Message<google_firestore_v1_Value> operand;

  switch (value.type()) {
    case Type::kIncrementDouble:
      operand->which_value_type = google_firestore_v1_Value_double_value_tag;
      operand->double_value = GetInternal(&value)->double_increment_value();
      break;

    case Type::kIncrementInteger:
      operand->which_value_type = google_firestore_v1_Value_integer_value_tag;
      operand->integer_value = GetInternal(&value)->integer_increment_value();
      break;

    default:
      SIMPLE_HARD_FAIL("A non-increment value given to ParseNumericIncrement");
  }

  context.AddToFieldTransforms(*context.path(),
                               NumericIncrementTransform{std::move(operand)});
}

FieldMask CreateFieldMask(const ParseAccumulator& accumulator,
                          const std::unordered_set<FieldPath>& field_paths) {
  std::set<model::FieldPath> validated;

  for (const FieldPath& public_path : field_paths) {
    const model::FieldPath& path = GetInternal(public_path);

    // Verify that all elements specified in the field mask are part of the
    // parsed context.
    if (!accumulator.Contains(path)) {
      // TODO(b/147444199): use string formatting.
      // ThrowInvalidArgument(
      //     "Field '%s' is specified in your field mask but missing from your "
      //     "input data.",
      //     path.CanonicalString());
      auto message =
          std::string("Field '") + path.CanonicalString() +
          "' is specified in your field mask but not in your input data.";
      SimpleThrowInvalidArgument(message);
    }

    validated.insert(path);
  }

  return FieldMask{std::move(validated)};
}

}  // namespace

// Public entry points

ParsedSetData UserDataConverter::ParseSetData(const MapFieldValue& data,
                                              const SetOptions& options) const {
  SetOptionsInternal internal_options{options};

  switch (internal_options.type()) {
    case SetOptions::Type::kOverwrite:
      return ParseSetData(data);
    case SetOptions::Type::kMergeAll:
      return ParseMergeData(data);
    case SetOptions::Type::kMergeSpecific:
      return ParseMergeData(data, internal_options.field_mask());
  }

  FIRESTORE_UNREACHABLE();
}

ParsedUpdateData UserDataConverter::ParseUpdateData(
    const MapFieldValue& input) const {
  UpdateDataInput converted_input;
  converted_input.reserve(input.size());

  for (const auto& kv : input) {
    converted_input.emplace_back(
        model::FieldPath::FromDotSeparatedString(kv.first), &kv.second);
  }

  return ParseUpdateData(converted_input);
}

ParsedUpdateData UserDataConverter::ParseUpdateData(
    const MapFieldPathValue& input) const {
  UpdateDataInput converted_input;
  converted_input.reserve(input.size());

  for (const auto& kv : input) {
    converted_input.emplace_back(GetInternal(kv.first), &kv.second);
  }

  return ParseUpdateData(converted_input);
}

// Implementation

ParsedSetData UserDataConverter::ParseSetData(
    const MapFieldValue& input) const {
  ParseAccumulator accumulator{UserDataSource::Set};
  auto data = ParseMap(input, accumulator.RootContext());

  return std::move(accumulator).SetData(ObjectValue{std::move(data)});
}

ParsedSetData UserDataConverter::ParseMergeData(
    const MapFieldValue& input,
    const absl::optional<std::unordered_set<FieldPath>>& maybe_field_mask)
    const {
  ParseAccumulator accumulator{UserDataSource::MergeSet};

  auto update_data = ParseMap(input, accumulator.RootContext());
  ObjectValue update_object{std::move(update_data)};

  if (!maybe_field_mask) {
    return std::move(accumulator).MergeData(std::move(update_object));
  }

  return std::move(accumulator)
      .MergeData(std::move(update_object),
                 CreateFieldMask(accumulator, maybe_field_mask.value()));
}

Message<google_firestore_v1_Value> UserDataConverter::ParseQueryValue(
    const FieldValue& input, bool allow_arrays) const {
  ParseAccumulator accumulator{allow_arrays ? UserDataSource::ArrayArgument
                                            : UserDataSource::Argument};

  auto parsed = ParseData(input, accumulator.RootContext());
  SIMPLE_HARD_ASSERT(parsed, "Parsed data should not be nullopt.");
  SIMPLE_HARD_ASSERT(accumulator.field_transforms().empty(),
                     "Field transforms should have been disallowed.");
  return std::move(*parsed);
}

absl::optional<Message<google_firestore_v1_Value>> UserDataConverter::ParseData(
    const FieldValue& value, ParseContext&& context) const {
  auto maybe_add_to_field_mask = [&] {
    if (context.path()) {
      context.AddToFieldMask(*context.path());
    }
  };

  if (IsSentinel(value)) {
    ParseSentinel(value, std::move(context));
    return absl::nullopt;
  }

  switch (value.type()) {
    case Type::kArray:
      maybe_add_to_field_mask();
      return ParseArray(value.array_value(), std::move(context));

    case Type::kMap:
      return ParseMap(value.map_value(), std::move(context));

    default:
      maybe_add_to_field_mask();
      return ParseScalar(value, std::move(context));
  }
}

nanopb::Message<google_firestore_v1_Value> UserDataConverter::ParseArray(
    const std::vector<FieldValue>& input, ParseContext&& context) const {
  // In the case of IN queries, the parsed data is an array (representing the
  // set of values to be included for the IN query) that may directly contain
  // additional arrays (each representing an individual field value), so we
  // disable this validation.
  if (context.array_element() &&
      context.data_source() != core::UserDataSource::ArrayArgument) {
    ThrowInvalidData(context, "Nested arrays are not supported");
  }

  Message<google_firestore_v1_Value> result;
  result->which_value_type = google_firestore_v1_Value_array_value_tag;
  result->array_value.values_count = CheckedSize(input.size());
  result->array_value.values = nanopb::MakeArray<google_firestore_v1_Value>(
      result->array_value.values_count);

  for (size_t i = 0; i != input.size(); ++i) {
    auto parsed = ParseData(input[i], context.ChildContext(i));
    if (!parsed) {
      parsed = Message<google_firestore_v1_Value>(NullValue());
    }
    result->array_value.values[i] = *parsed->release();
  }

  return result;
}

Message<google_firestore_v1_Value> UserDataConverter::ParseMap(
    const MapFieldValue& input, ParseContext&& context) const {
  Message<google_firestore_v1_Value> result;
  result->which_value_type = google_firestore_v1_Value_map_value_tag;
  result->map_value = {};

  if (input.empty()) {
    const model::FieldPath* path = context.path();
    if (path && !path->empty()) {
      context.AddToFieldMask(*path);
    }
  } else {
    // Compute the final size of the fields array, which contains an entry for
    // all fields that are not FieldValue sentinels
    pb_size_t count = 0;
    for (const auto& kv : input) {
      if (!IsSentinel(kv.second)) {
        ++count;
      }
    }

    result->map_value.fields_count = count;
    result->map_value.fields =
        nanopb::MakeArray<google_firestore_v1_MapValue_FieldsEntry>(count);

    pb_size_t index = 0;
    for (const auto& kv : input) {
      const std::string& key = kv.first;
      const FieldValue& value = kv.second;

      auto parsed_value = ParseData(value, context.ChildContext(key));
      if (parsed_value) {
        result->map_value.fields[index].key = nanopb::MakeBytesArray(key);
        result->map_value.fields[index].value = *parsed_value->release();
        ++index;
      }
    }
  }

  return result;
}

void UserDataConverter::ParseSentinel(const FieldValue& value,
                                      ParseContext&& context) const {
  // Sentinels are only supported with writes, and not within arrays.
  if (!context.write()) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidData(
    //     context, "%s can only be used with Update() and Set()%s",
    //     Describe(value.type()), context.FieldDescription());
    auto message =
        Describe(value.type()) + " can only be used with Update() and Set()";
    ThrowInvalidData(context, message);
  }

  if (!context.path()) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidData(context, "%s is not currently supported inside arrays",
    //                  Describe(value.type()));
    auto message =
        Describe(value.type()) + " is not currently supported inside arrays";
    ThrowInvalidData(context, message);
  }

  switch (value.type()) {
    case Type::kDelete:
      ParseDelete(std::move(context));
      break;

    case Type::kServerTimestamp:
      ParseServerTimestamp(std::move(context));
      break;

    case Type::kArrayUnion:  // Fallthrough
    case Type::kArrayRemove:
      ParseArrayTransform(value.type(), ParseArrayTransformElements(value),
                          std::move(context));
      break;

    case Type::kIncrementDouble:
    case Type::kIncrementInteger:
      ParseNumericIncrement(value, std::move(context));
      break;

    default:
      // TODO(b/147444199): use string formatting.
      // HARD_FAIL("Unknown FieldValue type: '%s'", Describe(value.type()));
      auto message = std::string("Unknown FieldValue type: '") +
                     Describe(value.type()) + "'";
      SIMPLE_HARD_FAIL(message);
  }
}

Message<google_firestore_v1_Value> UserDataConverter::ParseScalar(
    const FieldValue& value, ParseContext&& context) const {
  Message<google_firestore_v1_Value> result;

  switch (value.type()) {
    case Type::kNull:
      result->which_value_type = google_firestore_v1_Value_null_value_tag;
      result->null_value = {};
      break;

    case Type::kBoolean:
      result->which_value_type = google_firestore_v1_Value_boolean_value_tag;
      result->boolean_value = value.boolean_value();
      break;

    case Type::kInteger:
      result->which_value_type = google_firestore_v1_Value_integer_value_tag;
      result->integer_value = value.integer_value();
      break;

    case Type::kDouble:
      result->which_value_type = google_firestore_v1_Value_double_value_tag;
      result->double_value = value.double_value();
      break;

    case Type::kTimestamp:
      result->which_value_type = google_firestore_v1_Value_timestamp_value_tag;
      result->timestamp_value.seconds = value.timestamp_value().seconds();
      // Truncate to microsecond precision immediately.
      result->timestamp_value.nanos =
          value.timestamp_value().nanoseconds() / 1000 * 1000;
      break;

    case Type::kString:
      result->which_value_type = google_firestore_v1_Value_string_value_tag;
      result->string_value = nanopb::MakeBytesArray(value.string_value());
      break;

    case Type::kBlob: {
      result->which_value_type = google_firestore_v1_Value_bytes_value_tag;
      // Copy the blob so that pb_release can do the right thing.
      result->bytes_value =
          nanopb::MakeBytesArray(value.blob_value(), value.blob_size());
      break;
    }

    case Type::kReference: {
      DocumentReference reference = value.reference_value();

      const DatabaseId& other =
          GetInternal(reference.firestore())->database_id();
      if (other != *database_id_) {
        // TODO(b/147444199): use string formatting.
        // ThrowInvalidData(
        //     context,
        //     "DocumentReference is for database %s/%s but should be for "
        //     "database %s/%s%s",
        //     other.project_id(), other.database_id(),
        //     database_id_->project_id(), database_id_->database_id(),
        //     context.FieldDescription());
        auto actual_db = other.project_id() + "/" + other.database_id();
        auto expected_db =
            database_id_->project_id() + "/" + database_id_->database_id();
        auto message = std::string("Document reference is for database ") +
                       actual_db + " but should be for database " + expected_db;
        ThrowInvalidData(context, message);
      }

      const model::DocumentKey& key = GetInternal(&reference)->key();
      std::string reference_name =
          model::ResourcePath({"projects", database_id_->project_id(),
                               "databases", database_id_->database_id(),
                               "documents", key.ToString()})
              .CanonicalString();

      result->which_value_type = google_firestore_v1_Value_reference_value_tag;
      result->reference_value = nanopb::MakeBytesArray(reference_name);

      break;
    }

    case Type::kGeoPoint:
      result->which_value_type = google_firestore_v1_Value_geo_point_value_tag;
      result->geo_point_value.latitude = value.geo_point_value().latitude();
      result->geo_point_value.longitude = value.geo_point_value().longitude();
      break;

    default:
      SIMPLE_HARD_FAIL("A non-scalar field value given to ParseScalar");
  }

  return result;
}

Message<google_firestore_v1_ArrayValue>
UserDataConverter::ParseArrayTransformElements(const FieldValue& value) const {
  std::vector<FieldValue> elements =
      GetInternal(&value)->array_transform_value();
  ParseAccumulator accumulator{UserDataSource::Argument};

  Message<google_firestore_v1_ArrayValue> array_value;
  array_value->values_count = CheckedSize(elements.size());
  array_value->values =
      nanopb::MakeArray<google_firestore_v1_Value>(array_value->values_count);

  for (size_t i = 0; i != elements.size(); ++i) {
    const FieldValue& element = elements[i];
    // Although array transforms are used with writes, the actual elements being
    // unioned or removed are not considered writes since they cannot contain
    // any FieldValue sentinels, etc.
    ParseContext context = accumulator.RootContext();

    auto parsed_element = ParseData(element, context.ChildContext(i));
    // TODO(b/147444199): use string formatting.
    // HARD_ASSERT(parsed_element && accumulator.field_transforms().empty(),
    //             "Failed to properly parse array transform element: %s",
    //             Describe(element.type()));
    if (!parsed_element || !accumulator.field_transforms().empty()) {
      auto message =
          std::string("Failed to properly parse array transform element: ") +
          Describe(element.type());
      SIMPLE_HARD_FAIL(message);
    }

    array_value->values[i] = *parsed_element->release();
  }

  return array_value;
}

ParsedUpdateData UserDataConverter::ParseUpdateData(
    const UpdateDataInput& input) const {
  ParseAccumulator accumulator{UserDataSource::Update};
  ParseContext context = accumulator.RootContext();
  model::ObjectValue update_data;

  for (const auto& kv : input) {
    const model::FieldPath& path = kv.first;
    const FieldValue& value = *kv.second;

    if (value.type() == Type::kDelete) {
      // Add it to the field mask, but don't add anything to update_data.
      context.AddToFieldMask(path);
    } else {
      auto parsed = ParseData(value, context.ChildContext(path));
      if (parsed) {
        context.AddToFieldMask(path);
        update_data.Set(path, std::move(*parsed));
      }
    }
  }

  return std::move(accumulator).UpdateData(std::move(update_data));
}

}  // namespace firestore
}  // namespace firebase
