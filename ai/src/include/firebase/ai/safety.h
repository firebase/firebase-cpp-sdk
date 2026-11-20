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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_SAFETY_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_SAFETY_H_

#include "firebase/ai/types.h"

namespace firebase {
namespace ai {

/// @brief A type used to specify a threshold for blocking harmful content for a
/// given `HarmCategory`.
///
/// Mirrors `Firebase.AI.SafetySetting` in Unity and `SafetySetting` in Flutter.
class SafetySetting {
 public:
  /// @brief Default constructor.
  SafetySetting()
      : category_(kHarmCategoryUnknown),
        threshold_(kHarmBlockThresholdUnknown),
        method_(Optional<HarmBlockMethod>()) {}

  /// @brief Construct a `SafetySetting` with a category, threshold, and
  /// optional evaluation method.
  ///
  /// @param category The category of harm to configure.
  /// @param threshold The threshold at and above which content is blocked.
  /// @param method Optional method (probability vs. severity) used to evaluate
  /// the threshold (only supported on the Enterprise / VertexAI backend).
  SafetySetting(
      HarmCategory category, HarmBlockThreshold threshold,
      const Optional<HarmBlockMethod>& method = Optional<HarmBlockMethod>())
      : category_(category), threshold_(threshold), method_(method) {}

  /// @brief Returns the harm category.
  HarmCategory category() const { return category_; }
  /// @brief Sets the harm category.
  void set_category(HarmCategory category) { category_ = category; }

  /// @brief Returns the block threshold.
  HarmBlockThreshold threshold() const { return threshold_; }
  /// @brief Sets the block threshold.
  void set_threshold(HarmBlockThreshold threshold) { threshold_ = threshold; }

  /// @brief Returns the optional block evaluation method.
  const Optional<HarmBlockMethod>& method() const { return method_; }
  /// @brief Sets the block evaluation method.
  void set_method(HarmBlockMethod method) { method_ = method; }

 private:
  HarmCategory category_;
  HarmBlockThreshold threshold_;
  Optional<HarmBlockMethod> method_;
};

/// @brief A type defining safety attributes of a `Candidate` or prompt.
///
/// Mirrors `Firebase.AI.SafetyRating` in Unity and `SafetyRating` in Flutter.
struct SafetyRating {
  /// @brief Default constructor.
  SafetyRating()
      : category(kHarmCategoryUnknown),
        probability(kHarmProbabilityUnknown),
        blocked(false),
        probability_score(0.0f),
        severity(kHarmSeverityUnknown),
        severity_score(0.0f) {}

  /// @brief The category for this rating.
  HarmCategory category;

  /// @brief The probability of harm for this content.
  HarmProbability probability;

  /// @brief Indicates whether the content was blocked because of this rating.
  bool blocked;

  /// @brief The probability score of harm (0.0 to 1.0, VertexAI/Enterprise).
  float probability_score;

  /// @brief The severity of harm for this content (VertexAI/Enterprise).
  HarmSeverity severity;

  /// @brief The severity score of harm (0.0 to 1.0, VertexAI/Enterprise).
  float severity_score;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_SAFETY_H_
