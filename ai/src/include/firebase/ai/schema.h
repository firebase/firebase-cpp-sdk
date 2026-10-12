/*
 * Copyright 2025 Google LLC
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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_SCHEMA_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_SCHEMA_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "firebase/ai/types.h"
#include "firebase/variant.h"

namespace firebase {
namespace ai {

/// @brief Value types supported by `Schema` and `JsonSchema`.
enum SchemaType {
  /// Type is unspecified (e.g., when `any_of` is used).
  kSchemaTypeUnspecified = 0,
  /// String type (`"STRING"` in OpenAPI Schema, `"string"` in JSON Schema).
  kSchemaTypeString,
  /// Floating-point number (`"NUMBER"` in OpenAPI, `"number"` in JSON Schema).
  kSchemaTypeNumber,
  /// Integral number (`"INTEGER"` in OpenAPI, `"integer"` in JSON Schema).
  kSchemaTypeInteger,
  /// Boolean (`"BOOLEAN"` in OpenAPI, `"boolean"` in JSON Schema).
  kSchemaTypeBoolean,
  /// Array of items (`"ARRAY"` in OpenAPI, `"array"` in JSON Schema).
  kSchemaTypeArray,
  /// Key-value object (`"OBJECT"` in OpenAPI, `"object"` in JSON Schema).
  kSchemaTypeObject,
};

/// @brief Defines the structure of input parameters for function declarations
/// or structured JSON output (`GenerationConfig::response_schema`).
///
/// Mirrors `Firebase.AI.Schema` in Unity and `Schema` in Flutter.
class Schema {
 public:
  /// @brief Default constructor creates an unspecified schema.
  Schema() : type_(kSchemaTypeUnspecified) {}

  /// @brief Construct a Schema with the given type.
  ///
  /// @param type The schema data type.
  explicit Schema(SchemaType type) : type_(type) {}

  /// @brief Returns a Schema for a boolean value.
  ///
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @return A boolean `Schema`.
  static Schema Boolean(
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>());

  /// @brief Returns a Schema for a 32-bit signed integer.
  ///
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @param minimum Optional inclusive minimum value.
  /// @param maximum Optional inclusive maximum value.
  /// @return An integer `Schema` with format `"int32"`.
  static Schema Int(
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>(),
      const Optional<double>& minimum = Optional<double>(),
      const Optional<double>& maximum = Optional<double>());

  /// @brief Returns a Schema for a 64-bit signed integer.
  ///
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @param minimum Optional inclusive minimum value.
  /// @param maximum Optional inclusive maximum value.
  /// @return An integer `Schema` with format `"int64"`.
  static Schema Long(
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>(),
      const Optional<double>& minimum = Optional<double>(),
      const Optional<double>& maximum = Optional<double>());

  /// @brief Returns a Schema for a single-precision floating-point number.
  ///
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @param minimum Optional inclusive minimum value.
  /// @param maximum Optional inclusive maximum value.
  /// @return A number `Schema` with format `"float"`.
  static Schema Float(
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>(),
      const Optional<double>& minimum = Optional<double>(),
      const Optional<double>& maximum = Optional<double>());

  /// @brief Returns a Schema for a double-precision floating-point number.
  ///
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @param minimum Optional inclusive minimum value.
  /// @param maximum Optional inclusive maximum value.
  /// @return A number `Schema`.
  static Schema Double(
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>(),
      const Optional<double>& minimum = Optional<double>(),
      const Optional<double>& maximum = Optional<double>());

  /// @brief Returns a Schema for a string.
  ///
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param format Optional string format (e.g. `"date-time"`).
  /// @param title Optional human-readable title.
  /// @return A string `Schema`.
  static Schema String(
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& format = Optional<std::string>(),
      const Optional<std::string>& title = Optional<std::string>());

  /// @brief Returns a Schema for an enumeration of string values.
  ///
  /// @param values The list of valid string values.
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @return An enum string `Schema`.
  static Schema Enum(
      const std::vector<std::string>& values,
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>());

  /// @brief Returns a Schema for an array of elements.
  ///
  /// @param items The schema for the elements in the array.
  /// @param description Optional explanation of the field.
  /// @param nullable Optional flag indicating whether the value may be null.
  /// @param title Optional human-readable title.
  /// @param min_items Optional minimum number of elements.
  /// @param max_items Optional maximum number of elements.
  /// @return An array `Schema`.
  static Schema Array(
      const Schema& items,
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>(),
      const Optional<int64_t>& min_items = Optional<int64_t>(),
      const Optional<int64_t>& max_items = Optional<int64_t>());

  /// @brief Returns a Schema for a complex object with named properties.
  ///
  /// @param properties Map of property names to their `Schema` definitions.
  /// @param optional_properties List of property names that are not required.
  /// All properties not in `optional_properties` will be marked as required.
  /// @param description Optional explanation of the object.
  /// @param nullable Optional flag indicating whether the object may be null.
  /// @param title Optional human-readable title.
  /// @param property_ordering Optional explicit ordering of property keys.
  /// @return An object `Schema`.
  static Schema Object(
      const std::map<std::string, Schema>& properties,
      const std::vector<std::string>& optional_properties =
          std::vector<std::string>(),
      const Optional<std::string>& description = Optional<std::string>(),
      const Optional<bool>& nullable = Optional<bool>(),
      const Optional<std::string>& title = Optional<std::string>(),
      const std::vector<std::string>& property_ordering =
          std::vector<std::string>());

  /// @brief Returns a Schema representing a union (`anyOf`) of schemas.
  ///
  /// @param schemas The candidate schemas.
  /// @return A union `Schema`.
  static Schema AnyOf(const std::vector<Schema>& schemas);

  /// @brief Returns the schema type.
  SchemaType type() const { return type_; }
  /// @brief Sets the schema type.
  void set_type(SchemaType type) { type_ = type; }

  /// @brief Returns the description.
  const Optional<std::string>& description() const { return description_; }
  /// @brief Sets the description.
  void set_description(const std::string& description) {
    description_ = description;
  }

  /// @brief Returns the format specifier.
  const Optional<std::string>& format() const { return format_; }
  /// @brief Sets the format specifier.
  void set_format(const std::string& format) { format_ = format; }

  /// @brief Returns whether the value is nullable.
  const Optional<bool>& nullable() const { return nullable_; }
  /// @brief Sets whether the value is nullable.
  void set_nullable(bool nullable) { nullable_ = nullable; }

  /// @brief Returns the enum values, if any.
  const std::vector<std::string>& enum_values() const { return enum_values_; }
  /// @brief Sets the enum values.
  void set_enum_values(const std::vector<std::string>& values) {
    enum_values_ = values;
  }

  /// @brief Returns the properties map for an object schema.
  const std::map<std::string, Schema>& properties() const {
    return properties_;
  }
  /// @brief Sets the properties map for an object schema.
  void set_properties(const std::map<std::string, Schema>& properties) {
    properties_ = properties;
  }

  /// @brief Returns the list of required property names.
  const std::vector<std::string>& required_properties() const {
    return required_properties_;
  }
  /// @brief Sets the list of required property names.
  void set_required_properties(const std::vector<std::string>& required) {
    required_properties_ = required;
  }

  /// @brief Returns the property ordering list.
  const std::vector<std::string>& property_ordering() const {
    return property_ordering_;
  }
  /// @brief Sets the property ordering list.
  void set_property_ordering(const std::vector<std::string>& ordering) {
    property_ordering_ = ordering;
  }

  /// @brief Returns the element schema for an array schema, or nullptr.
  const Schema* items() const { return items_.get(); }
  /// @brief Sets the element schema for an array schema.
  void set_items(const Schema& items) { items_.reset(new Schema(items)); }

  /// @brief Returns the title.
  const Optional<std::string>& title() const { return title_; }
  /// @brief Sets the title.
  void set_title(const std::string& title) { title_ = title; }

  /// @brief Returns the minimum items count for an array schema.
  const Optional<int64_t>& min_items() const { return min_items_; }
  /// @brief Sets the minimum items count for an array schema.
  void set_min_items(int64_t min_items) { min_items_ = min_items; }

  /// @brief Returns the maximum items count for an array schema.
  const Optional<int64_t>& max_items() const { return max_items_; }
  /// @brief Sets the maximum items count for an array schema.
  void set_max_items(int64_t max_items) { max_items_ = max_items; }

  /// @brief Returns the minimum numeric value.
  const Optional<double>& minimum() const { return minimum_; }
  /// @brief Sets the minimum numeric value.
  void set_minimum(double minimum) { minimum_ = minimum; }

  /// @brief Returns the maximum numeric value.
  const Optional<double>& maximum() const { return maximum_; }
  /// @brief Sets the maximum numeric value.
  void set_maximum(double maximum) { maximum_ = maximum; }

  /// @brief Returns the `anyOf` sub-schemas.
  const std::vector<Schema>& any_of() const { return any_of_; }
  /// @brief Sets the `anyOf` sub-schemas.
  void set_any_of(const std::vector<Schema>& any_of) { any_of_ = any_of; }

 private:
  SchemaType type_;
  Optional<std::string> description_;
  Optional<std::string> format_;
  Optional<bool> nullable_;
  std::vector<std::string> enum_values_;
  std::map<std::string, Schema> properties_;
  std::vector<std::string> required_properties_;
  std::vector<std::string> property_ordering_;
  std::shared_ptr<Schema> items_;
  Optional<std::string> title_;
  Optional<int64_t> min_items_;
  Optional<int64_t> max_items_;
  Optional<double> minimum_;
  Optional<double> maximum_;
  std::vector<Schema> any_of_;
};

/// @brief Standard JSON Schema definition used with
/// `GenerationConfig::response_json_schema`.
///
/// Mirrors `Firebase.AI.JsonSchema` in the Unity SDK, serializing types in
/// lowercase (`"string"`, `"object"`, etc.) and representing `nullable` via
/// `["<type>", "null"]` or `anyOf` with `{"type": "null"}`.
using JsonSchema = Schema;

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_SCHEMA_H_
