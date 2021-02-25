#include "firestore/src/ios/user_data_converter_ios.h"

#include <set>
#include <string>

#include "firestore/src/common/macros.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/field_value_ios.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "firestore/src/ios/set_options_ios.h"
#include "absl/memory/memory.h"
#include "Firestore/core/src/core/user_data.h"
#include "Firestore/core/src/model/field_mask.h"
#include "Firestore/core/src/model/transform_operation.h"
#include "Firestore/core/src/nanopb/byte_string.h"
#include "Firestore/core/src/util/exception.h"

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
using model::NumericIncrementTransform;
using model::ServerTimestampTransform;
using model::TransformOperation;
using nanopb::ByteString;
using util::ThrowInvalidArgumentIos;

using Type = FieldValue::Type;

void ParseDelete(ParseContext&& context) {
  if (context.data_source() == UserDataSource::MergeSet) {
    // No transform to add for a delete, but we need to add it to our field mask
    // so it gets deleted.
    context.AddToFieldMask(*context.path());
    return;
  }

  if (context.data_source() == UserDataSource::Update) {
    HARD_ASSERT_IOS(
        !context.path()->empty(),
        "FieldValue.Delete() at the top level should have already been "
        "handled.");
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidArgument(
    //     "FieldValue::Delete() can only appear at the top level of your "
    //     "update data%s",
    //     context.FieldDescription());
    auto message =
        std::string(
            "FieldValue::Delete() can only appear at the top level of your "
            "update data") +
        context.FieldDescription();
    ThrowInvalidArgumentIos(message.c_str());
  }

  // We shouldn't encounter delete sentinels for queries or non-merge `Set`
  // calls.
  // TODO(b/147444199): use string formatting.
  // ThrowInvalidArgument(
  //     "FieldValue::Delete() can only be used with Update() and Set() with "
  //     "merge == true%s",
  //     context.FieldDescription());
  auto message =
      std::string(
          "FieldValue::Delete() can only be used with Update() and Set() with "
          "merge == true") +
      context.FieldDescription();
  ThrowInvalidArgumentIos(message.c_str());
}

void ParseServerTimestamp(ParseContext&& context) {
  context.AddToFieldTransforms(*context.path(), ServerTimestampTransform{});
}

void ParseArrayTransform(Type type, const model::FieldValue::Array& elements,
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
        HARD_FAIL_IOS(message.c_str());
      }
    }
  }();

  context.AddToFieldTransforms(
      *context.path(), ArrayTransform{transform_type, std::move(elements)});
}

void ParseNumericIncrement(const FieldValue& value, ParseContext&& context) {
  model::FieldValue operand;

  switch (value.type()) {
    case Type::kIncrementDouble:
      operand = model::FieldValue::FromDouble(
          GetInternal(&value)->double_increment_value());
      break;

    case Type::kIncrementInteger:
      operand = model::FieldValue::FromInteger(
          GetInternal(&value)->integer_increment_value());
      break;

    default:
      HARD_FAIL_IOS("A non-increment value given to ParseNumericIncrement");
  }

  context.AddToFieldTransforms(*context.path(),
                               NumericIncrementTransform{std::move(operand)});
}

FieldMask CreateFieldMask(const ParseAccumulator& accumulator,
                          const std::vector<FieldPath>& field_paths) {
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
          "' is specified in your field mask but missing from your input data.";
      ThrowInvalidArgumentIos(message.c_str());
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

  return std::move(accumulator).SetData(std::move(data));
}

ParsedSetData UserDataConverter::ParseMergeData(
    const MapFieldValue& input,
    const absl::optional<std::vector<FieldPath>>& maybe_field_mask) const {
  ParseAccumulator accumulator{UserDataSource::MergeSet};
  auto data = ParseMap(input, accumulator.RootContext());

  if (!maybe_field_mask) {
    return std::move(accumulator).MergeData(std::move(data));
  }

  return std::move(accumulator)
      .MergeData(std::move(data),
                 CreateFieldMask(accumulator, maybe_field_mask.value()));
}

model::FieldValue UserDataConverter::ParseQueryValue(const FieldValue& input,
                                                     bool allow_arrays) const {
  ParseAccumulator accumulator{allow_arrays ? UserDataSource::ArrayArgument
                                           : UserDataSource::Argument};

  absl::optional<model::FieldValue> parsed =
      ParseData(input, accumulator.RootContext());
  HARD_ASSERT_IOS(parsed, "Parsed data should not be nullopt.");
  HARD_ASSERT_IOS(accumulator.field_transforms().empty(),
                  "Field transforms should have been disallowed.");
  return parsed.value();
}

absl::optional<model::FieldValue> UserDataConverter::ParseData(
    const FieldValue& value, ParseContext&& context) const {
  auto maybe_add_to_field_mask = [&] {
    if (context.path()) {
      context.AddToFieldMask(*context.path());
    }
  };

  switch (value.type()) {
    case Type::kArray:
      maybe_add_to_field_mask();
      return model::FieldValue::FromArray(
          ParseArray(value.array_value(), std::move(context)));

    case Type::kMap:
      return ParseMap(value.map_value(), std::move(context)).AsFieldValue();

    case Type::kDelete:
    case Type::kServerTimestamp:
    case Type::kArrayUnion:
    case Type::kArrayRemove:
    case Type::kIncrementDouble:
    case Type::kIncrementInteger:
      ParseSentinel(value, std::move(context));
      return absl::nullopt;

    default:
      maybe_add_to_field_mask();
      return ParseScalar(value, std::move(context));
  }
}

model::FieldValue::Array UserDataConverter::ParseArray(
    const std::vector<FieldValue>& input, ParseContext&& context) const {
  // In the case of IN queries, the parsed data is an array (representing the
  // set of values to be included for the IN query) that may directly contain
  // additional arrays (each representing an individual field value), so we
  // disable this validation.
  if (context.array_element() &&
      context.data_source() != core::UserDataSource::ArrayArgument) {
    ThrowInvalidArgumentIos("Nested arrays are not supported");
  }

  model::FieldValue::Array result;

  for (size_t i = 0; i != input.size(); ++i) {
    absl::optional<model::FieldValue> parsed =
        ParseData(input[i], context.ChildContext(i));
    if (!parsed) {
      parsed = model::FieldValue::Null();
    }
    result.push_back(std::move(parsed).value());
  }

  return result;
}

model::ObjectValue UserDataConverter::ParseMap(const MapFieldValue& input,
                                               ParseContext&& context) const {
  if (input.empty()) {
    const model::FieldPath* path = context.path();
    if (path && !path->empty()) {
      context.AddToFieldMask(*path);
    }
    return model::ObjectValue{};
  }

  model::FieldValue::Map result;
  for (const auto& kv : input) {
    const std::string& key = kv.first;
    const FieldValue& value = kv.second;

    absl::optional<model::FieldValue> parsed =
        ParseData(value, context.ChildContext(key));
    if (parsed) {
      result = result.insert(key, std::move(parsed).value());
    }
  }

  return model::ObjectValue::FromMap(std::move(result));
}

void UserDataConverter::ParseSentinel(const FieldValue& value,
                                      ParseContext&& context) const {
  // Sentinels are only supported with writes, and not within arrays.
  if (!context.write()) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidArgument("%s can only be used with Update() and Set()%s",
    //                      Describe(value.type()), context.FieldDescription());
    auto message = Describe(value.type()) +
                   " can only be used with Update() and Set()" +
                   context.FieldDescription();
    ThrowInvalidArgumentIos(message.c_str());
  }

  if (!context.path()) {
    // TODO(b/147444199): use string formatting.
    // ThrowInvalidArgument("%s is not currently supported inside arrays",
    //                      Describe(value.type()));
    auto message =
        Describe(value.type()) + " is not currently supported inside arrays";
    ThrowInvalidArgumentIos(message.c_str());
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
      HARD_FAIL_IOS(message.c_str());
  }
}

model::FieldValue UserDataConverter::ParseScalar(const FieldValue& value,
                                                 ParseContext&& context) const {
  switch (value.type()) {
    case Type::kNull:
      return model::FieldValue::Null();

    case Type::kBoolean:
      return model::FieldValue::FromBoolean(value.boolean_value());

    case Type::kInteger:
      return model::FieldValue::FromInteger(value.integer_value());

    case Type::kDouble:
      return model::FieldValue::FromDouble(value.double_value());

    case Type::kTimestamp: {
      // Truncate to microsecond precision immediately.
      Timestamp truncated{value.timestamp_value().seconds(),
                          value.timestamp_value().nanoseconds() / 1000 * 1000};
      return model::FieldValue::FromTimestamp(truncated);
    }

    case Type::kString:
      return model::FieldValue::FromString(value.string_value());

    case Type::kBlob: {
      ByteString blob{value.blob_value(), value.blob_size()};
      return model::FieldValue::FromBlob(std::move(blob));
    }

    case Type::kReference: {
      DocumentReference reference = value.reference_value();

      const DatabaseId& other =
          GetInternal(reference.firestore())->database_id();
      if (other != *database_id_) {
        // TODO(b/147444199): use string formatting.
        // ThrowInvalidArgument(
        //     "DocumentReference is for database %s/%s but should be for "
        //     "database %s/%s%s",
        //     other.project_id(), other.database_id(),
        //     database_id_->project_id(), database_id_->database_id(),
        //     context.FieldDescription());
        auto actual_db = other.project_id() + "/" + other.database_id();
        auto expected_db =
            database_id_->project_id() + "/" + database_id_->database_id();
        auto message = std::string("DocumentReference is for database ") +
                       actual_db + " but should be for database " +
                       expected_db + context.FieldDescription();
        ThrowInvalidArgumentIos(message.c_str());
      }

      const model::DocumentKey& key = GetInternal(&reference)->key();
      return model::FieldValue::FromReference(*database_id_, key);
    }

    case Type::kGeoPoint:
      return model::FieldValue::FromGeoPoint(value.geo_point_value());

    default:
      HARD_FAIL_IOS("A non-scalar field value given to ParseScalar");
  }
}

model::FieldValue::Array UserDataConverter::ParseArrayTransformElements(
    const FieldValue& value) const {
  std::vector<FieldValue> elements =
      GetInternal(&value)->array_transform_value();
  model::FieldValue::Array result;
  ParseAccumulator accumulator{UserDataSource::Argument};

  for (size_t i = 0; i != elements.size(); ++i) {
    const FieldValue& element = elements[i];
    // Although array transforms are used with writes, the actual elements being
    // unioned or removed are not considered writes since they cannot contain
    // any FieldValue sentinels, etc.
    ParseContext context = accumulator.RootContext();

    absl::optional<model::FieldValue> parsed_element =
        ParseData(element, context.ChildContext(i));
    // TODO(b/147444199): use string formatting.
    // HARD_ASSERT(parsed_element && accumulator.field_transforms().empty(),
    //             "Failed to properly parse array transform element: %s",
    //             Describe(element.type()));
    if (!parsed_element || !accumulator.field_transforms().empty()) {
      auto message =
          std::string("Failed to properly parse array transform element: ") +
          Describe(element.type());
      HARD_FAIL_IOS(message.c_str());
    }

    result.push_back(std::move(parsed_element).value());
  }

  return result;
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
      absl::optional<model::FieldValue> parsed =
          ParseData(value, context.ChildContext(path));
      if (parsed) {
        context.AddToFieldMask(path);
        update_data = update_data.Set(path, std::move(parsed).value());
      }
    }
  }

  return std::move(accumulator).UpdateData(std::move(update_data));
}

}  // namespace firestore
}  // namespace firebase
