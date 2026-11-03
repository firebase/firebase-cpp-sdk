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

#ifndef FIREBASE_AI_SRC_COMMON_SERIALIZATION_H_
#define FIREBASE_AI_SRC_COMMON_SERIALIZATION_H_

#include <map>
#include <string>
#include <vector>

#include "firebase/ai/function_calling.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generation_config.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/safety.h"
#include "firebase/ai/schema.h"
#include "firebase/ai/types.h"
#include "firebase/variant.h"

namespace firebase {
namespace ai {
namespace internal {

/// @brief Converts an OpenAPI `Schema` to a `Variant` map.
Variant SchemaToVariant(const Schema& schema);

/// @brief Converts a standard `JsonSchema` to a `Variant` map.
Variant JsonSchemaToVariant(const JsonSchema& schema);

/// @brief Converts a `Part` to a `Variant` map.
Variant PartToVariant(const Part& part);

/// @brief Parses a `Part` from a `Variant` map.
bool PartFromVariant(const Variant& variant, Part* out_part);

/// @brief Converts a `ModelContent` to a `Variant` map.
Variant ModelContentToVariant(const ModelContent& content);

/// @brief Parses a `ModelContent` from a `Variant` map.
bool ModelContentFromVariant(const Variant& variant, ModelContent* out_content);

/// @brief Converts a `SafetySetting` to a `Variant` map for the given backend.
Variant SafetySettingToVariant(const SafetySetting& setting,
                               BackendProvider provider);

/// @brief Parses a `SafetyRating` from a `Variant` map.
SafetyRating SafetyRatingFromVariant(const Variant& variant);

/// @brief Converts a `GenerationConfig` to a `Variant` map.
Variant GenerationConfigToVariant(const GenerationConfig& config);

/// @brief Converts a `Tool` to a `Variant` map.
Variant ToolToVariant(const Tool& tool);

/// @brief Converts a `ToolConfig` to a `Variant` map.
Variant ToolConfigToVariant(const ToolConfig& config);

/// @brief Builds the JSON request body for `:generateContent` or
/// `:streamGenerateContent`.
std::string BuildGenerateContentRequestJson(
    const std::vector<ModelContent>& contents,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction, BackendProvider provider);

/// @brief Builds the JSON request body for `:countTokens`.
std::string BuildCountTokensRequestJson(
    const std::string& model_name, const std::vector<ModelContent>& contents,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction, BackendProvider provider);

/// @brief Builds the JSON request body for `:templateGenerateContent` or
/// `:templateStreamGenerateContent`.
std::string BuildTemplateGenerateContentRequestJson(
    const std::map<std::string, Variant>& inputs,
    const std::vector<ModelContent>& history = std::vector<ModelContent>());

/// @brief Builds the JSON request body for `:templateGenerateContent` from a
/// raw JSON inputs string.
std::string BuildTemplateGenerateContentRequestFromRawJson(
    const std::string& json_inputs,
    const std::vector<ModelContent>& history = std::vector<ModelContent>());

/// @brief Parses a `GenerateContentResponse` from a JSON string.
bool ParseGenerateContentResponseJson(const std::string& json,
                                      BackendProvider provider,
                                      GenerateContentResponse* out_response,
                                      std::string* out_error);

/// @brief Parses a `CountTokensResponse` from a JSON string.
bool ParseCountTokensResponseJson(const std::string& json,
                                  CountTokensResponse* out_response,
                                  std::string* out_error);

/// @brief Extracts a human-readable error message from a Google API JSON error
/// response body.
std::string ParseHttpErrorJson(int status_code, const std::string& body);

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_SERIALIZATION_H_
