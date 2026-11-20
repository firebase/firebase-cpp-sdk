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

#include "firebase/ai/model_content.h"

#include <algorithm>
#include <set>
#include <sstream>

#include "firebase/ai/function_calling.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/schema.h"

namespace firebase {
namespace ai {

// --- Schema factory methods ---

Schema Schema::Boolean(const Optional<std::string>& description,
                       const Optional<bool>& nullable,
                       const Optional<std::string>& title) {
  Schema s(kSchemaTypeBoolean);
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  return s;
}

Schema Schema::Int(const Optional<std::string>& description,
                   const Optional<bool>& nullable,
                   const Optional<std::string>& title,
                   const Optional<double>& minimum,
                   const Optional<double>& maximum) {
  Schema s(kSchemaTypeInteger);
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  s.format_ = std::string("int32");
  s.minimum_ = minimum;
  s.maximum_ = maximum;
  return s;
}

Schema Schema::Long(const Optional<std::string>& description,
                    const Optional<bool>& nullable,
                    const Optional<std::string>& title,
                    const Optional<double>& minimum,
                    const Optional<double>& maximum) {
  Schema s(kSchemaTypeInteger);
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  s.format_ = std::string("int64");
  s.minimum_ = minimum;
  s.maximum_ = maximum;
  return s;
}

Schema Schema::Float(const Optional<std::string>& description,
                     const Optional<bool>& nullable,
                     const Optional<std::string>& title,
                     const Optional<double>& minimum,
                     const Optional<double>& maximum) {
  Schema s(kSchemaTypeNumber);
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  s.format_ = std::string("float");
  s.minimum_ = minimum;
  s.maximum_ = maximum;
  return s;
}

Schema Schema::Double(const Optional<std::string>& description,
                      const Optional<bool>& nullable,
                      const Optional<std::string>& title,
                      const Optional<double>& minimum,
                      const Optional<double>& maximum) {
  Schema s(kSchemaTypeNumber);
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  s.minimum_ = minimum;
  s.maximum_ = maximum;
  return s;
}

Schema Schema::String(const Optional<std::string>& description,
                      const Optional<bool>& nullable,
                      const Optional<std::string>& format,
                      const Optional<std::string>& title) {
  Schema s(kSchemaTypeString);
  s.description_ = description;
  s.nullable_ = nullable;
  s.format_ = format;
  s.title_ = title;
  return s;
}

Schema Schema::Enum(const std::vector<std::string>& values,
                    const Optional<std::string>& description,
                    const Optional<bool>& nullable,
                    const Optional<std::string>& title) {
  Schema s(kSchemaTypeString);
  s.enum_values_ = values;
  s.format_ = std::string("enum");
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  return s;
}

Schema Schema::Array(const Schema& items,
                     const Optional<std::string>& description,
                     const Optional<bool>& nullable,
                     const Optional<std::string>& title,
                     const Optional<int64_t>& min_items,
                     const Optional<int64_t>& max_items) {
  Schema s(kSchemaTypeArray);
  s.items_.reset(new Schema(items));
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  s.min_items_ = min_items;
  s.max_items_ = max_items;
  return s;
}

Schema Schema::Object(const std::map<std::string, Schema>& properties,
                      const std::vector<std::string>& optional_properties,
                      const Optional<std::string>& description,
                      const Optional<bool>& nullable,
                      const Optional<std::string>& title,
                      const std::vector<std::string>& property_ordering) {
  Schema s(kSchemaTypeObject);
  s.properties_ = properties;
  s.description_ = description;
  s.nullable_ = nullable;
  s.title_ = title;
  s.property_ordering_ = property_ordering;

  std::set<std::string> opt_set(optional_properties.begin(),
                                optional_properties.end());
  for (const auto& kv : properties) {
    if (opt_set.find(kv.first) == opt_set.end()) {
      s.required_properties_.push_back(kv.first);
    }
  }
  return s;
}

Schema Schema::AnyOf(const std::vector<Schema>& schemas) {
  Schema s(kSchemaTypeUnspecified);
  s.any_of_ = schemas;
  return s;
}

// --- FunctionDeclaration ---

FunctionDeclaration::FunctionDeclaration(
    const std::string& name, const std::string& description,
    const std::map<std::string, Schema>& parameters,
    const std::vector<std::string>& optional_parameters)
    : name_(name),
      description_(description),
      parameters_(Schema::Object(parameters, optional_parameters)),
      uses_json_schema_(false) {}

FunctionDeclaration::FunctionDeclaration(
    const std::string& name, const std::string& description,
    const JsonSchema& parameters_json_schema)
    : name_(name),
      description_(description),
      parameters_(parameters_json_schema),
      uses_json_schema_(true) {}

// --- ModelContent factory methods ---

ModelContent ModelContent::Text(const std::string& text) {
  return ModelContent("user", std::vector<Part>(1, Part(TextPart(text))));
}

ModelContent ModelContent::InlineData(const std::string& mime_type,
                                      const std::vector<uint8_t>& data) {
  return ModelContent(
      "user", std::vector<Part>(1, Part(InlineDataPart(mime_type, data))));
}

ModelContent ModelContent::InlineData(const std::string& mime_type,
                                      const uint8_t* bytes, size_t size) {
  return ModelContent(
      "user",
      std::vector<Part>(1, Part(InlineDataPart(mime_type, bytes, size))));
}

ModelContent ModelContent::FileData(const std::string& mime_type,
                                    const std::string& uri) {
  return ModelContent("user",
                      std::vector<Part>(1, Part(FileDataPart(mime_type, uri))));
}

ModelContent ModelContent::FunctionResponse(
    const std::string& name, const std::map<std::string, Variant>& response,
    const Optional<std::string>& id) {
  return ModelContent(
      "user",
      std::vector<Part>(1, Part(FunctionResponsePart(name, response, id))));
}

ModelContent ModelContent::FunctionResponses(
    const std::vector<FunctionResponsePart>& responses) {
  std::vector<Part> parts;
  parts.reserve(responses.size());
  for (const auto& r : responses) {
    parts.push_back(Part(r));
  }
  return ModelContent("user", parts);
}

ModelContent ModelContent::System(const std::string& text) {
  return ModelContent("system", std::vector<Part>(1, Part(TextPart(text))));
}

ModelContent ModelContent::User(const std::string& text) {
  return ModelContent("user", std::vector<Part>(1, Part(TextPart(text))));
}

ModelContent ModelContent::User(const std::vector<Part>& parts) {
  return ModelContent("user", parts);
}

ModelContent ModelContent::Model(const std::string& text) {
  return ModelContent("model", std::vector<Part>(1, Part(TextPart(text))));
}

ModelContent ModelContent::Model(const std::vector<Part>& parts) {
  return ModelContent("model", parts);
}

// --- GenerateContentResponse convenience accessors ---

std::string GenerateContentResponse::text() const {
  if (candidates_.empty()) return "";
  std::ostringstream oss;
  for (const auto& part : candidates_[0].content.parts()) {
    if (part.is_text() && !part.is_thought()) {
      oss << part.text_part().text;
    }
  }
  return oss.str();
}

std::string GenerateContentResponse::thought_summary() const {
  if (candidates_.empty()) return "";
  std::ostringstream oss;
  for (const auto& part : candidates_[0].content.parts()) {
    if (part.is_text() && part.is_thought()) {
      oss << part.text_part().text;
    }
  }
  return oss.str();
}

std::vector<FunctionCallPart> GenerateContentResponse::function_calls() const {
  std::vector<FunctionCallPart> result;
  if (candidates_.empty()) return result;
  for (const auto& part : candidates_[0].content.parts()) {
    if (part.is_function_call()) {
      result.push_back(part.function_call_part());
    }
  }
  return result;
}

std::vector<InlineDataPart> GenerateContentResponse::inline_data_parts() const {
  std::vector<InlineDataPart> result;
  if (candidates_.empty()) return result;
  for (const auto& part : candidates_[0].content.parts()) {
    if (part.is_inline_data() && !part.is_thought()) {
      result.push_back(part.inline_data_part());
    }
  }
  return result;
}

}  // namespace ai
}  // namespace firebase
