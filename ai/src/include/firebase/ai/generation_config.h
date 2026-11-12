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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATION_CONFIG_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATION_CONFIG_H_

#include <string>
#include <vector>

#include "firebase/ai/schema.h"
#include "firebase/ai/types.h"

namespace firebase {
namespace ai {

/// @brief Configuration for the "thinking" process of supported Gemini models.
///
/// Mirrors `Firebase.AI.ThinkingConfig` in Unity and `ThinkingConfig` in
/// Flutter.
struct ThinkingConfig {
  /// @brief Default constructor.
  ThinkingConfig() {}

  /// @brief Construct a `ThinkingConfig` with a token budget.
  ///
  /// @param thinking_budget Token budget for the model's thinking process (0 to
  /// disable thinking, -1 for dynamic budget).
  /// @param include_thoughts Optional flag indicating whether thought summaries
  /// should be included in the response.
  explicit ThinkingConfig(
      int thinking_budget,
      const Optional<bool>& include_thoughts = Optional<bool>())
      : thinking_budget(thinking_budget), include_thoughts(include_thoughts) {}

  /// @brief Construct a `ThinkingConfig` with a `ThinkingLevel`.
  ///
  /// @param thinking_level Discrete thinking effort level.
  /// @param include_thoughts Optional flag indicating whether thought summaries
  /// should be included in the response.
  explicit ThinkingConfig(
      ThinkingLevel thinking_level,
      const Optional<bool>& include_thoughts = Optional<bool>())
      : thinking_level(thinking_level), include_thoughts(include_thoughts) {}

  /// @brief Optional thinking token budget.
  Optional<int> thinking_budget;

  /// @brief Optional discrete thinking level.
  Optional<ThinkingLevel> thinking_level;

  /// @brief Optional flag to include thought summaries in the response.
  Optional<bool> include_thoughts;
};

/// @brief Configuration for image generation parameters when using Gemini image
/// generation models.
///
/// Mirrors `Firebase.AI.ImageConfig` in Unity.
struct ImageConfig {
  /// @brief Construct an `ImageConfig`.
  ///
  /// @param aspect_ratio Optional aspect ratio (e.g. `"1:1"`, `"16:9"`).
  /// @param image_size Optional image resolution preset (e.g. `"1K"`, `"2K"`).
  explicit ImageConfig(
      const Optional<std::string>& aspect_ratio = Optional<std::string>(),
      const Optional<std::string>& image_size = Optional<std::string>())
      : aspect_ratio(aspect_ratio), image_size(image_size) {}

  /// @brief Aspect ratio of generated images (e.g. `"1:1"`, `"3:4"`, `"4:3"`,
  /// `"9:16"`, `"16:9"`).
  Optional<std::string> aspect_ratio;

  /// @brief Resolution preset of generated images (e.g. `"1K"`, `"2K"`).
  Optional<std::string> image_size;
};

/// @brief Configuration parameters used by `GenerativeModel` to control content
/// generation.
///
/// Mirrors `Firebase.AI.GenerationConfig` in Unity and `GenerationConfig` in
/// Flutter.
struct GenerationConfig {
  /// @brief Default constructor with all fields unset.
  GenerationConfig() {}

  /// @brief Controls the degree of randomness in token selection.
  Optional<float> temperature;

  /// @brief Nucleus sampling probability threshold.
  Optional<float> top_p;

  /// @brief Top-k sampling token count threshold.
  Optional<int> top_k;

  /// @brief Number of response candidates to generate.
  Optional<int> candidate_count;

  /// @brief Maximum number of tokens to generate in the response.
  Optional<int> max_output_tokens;

  /// @brief Penalizes tokens that have already appeared in the generated text.
  Optional<float> presence_penalty;

  /// @brief Penalizes tokens proportional to how frequently they have appeared.
  Optional<float> frequency_penalty;

  /// @brief Character sequences that will stop output generation when
  /// encountered.
  std::vector<std::string> stop_sequences;

  /// @brief Output response MIME type (e.g. `"text/plain"` or
  /// `"application/json"`).
  Optional<std::string> response_mime_type;

  /// @brief OpenAPI `Schema` that the generated output must conform to (used
  /// with `response_mime_type = "application/json"`).
  Optional<Schema> response_schema;

  /// @brief Standard `JsonSchema` that the generated output must conform to
  /// (serialized as `responseJsonSchema`).
  Optional<JsonSchema> response_json_schema;

  /// @brief Requested output modalities (e.g., text, image).
  std::vector<ResponseModality> response_modalities;

  /// @brief Optional configuration for the model's thinking process.
  Optional<ThinkingConfig> thinking_config;

  /// @brief Optional configuration for image generation outputs.
  Optional<ImageConfig> image_config;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATION_CONFIG_H_
