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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TEMPLATE_GENERATIVE_MODEL_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TEMPLATE_GENERATIVE_MODEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/types.h"
#include "firebase/future.h"
#include "firebase/variant.h"

namespace firebase {
namespace ai {

class FirebaseAI;
class TemplateChatSession;

namespace internal {
class TemplateGenerativeModelInternal;
class FirebaseAIInternal;
}  // namespace internal

/// @brief A type that represents a remote server-prompt-template model with the
/// ability to generate content and stream content by supplying template IDs and
/// input variables.
///
/// Mirrors `Firebase.AI.TemplateGenerativeModel` in Unity and
/// `TemplateGenerativeModel` in Flutter.
class TemplateGenerativeModel {
 public:
  /// @brief Default constructor creates an invalid `TemplateGenerativeModel`.
  TemplateGenerativeModel();

  /// @brief Copy constructor.
  TemplateGenerativeModel(const TemplateGenerativeModel& other);

  /// @brief Copy assignment operator.
  TemplateGenerativeModel& operator=(const TemplateGenerativeModel& other);

  /// @brief Destructor.
  ~TemplateGenerativeModel();

  /// @brief Returns true if this `TemplateGenerativeModel` is valid.
  bool is_valid() const { return internal_ != nullptr; }

  /// @brief Generates content from a server prompt template and a map of
  /// template input variables.
  ///
  /// @param template_id The ID of the server prompt template.
  /// @param inputs Key-value map of template variables.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> GenerateContent(
      const std::string& template_id,
      const std::map<std::string, Variant>& inputs);

  /// @brief Generates content from a server prompt template and a raw JSON
  /// object string of template input variables.
  ///
  /// @param template_id The ID of the server prompt template.
  /// @param json_inputs A JSON object string of template variables.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> GenerateContentJson(
      const std::string& template_id, const std::string& json_inputs);

  /// @brief Gets the result of the most recent `GenerateContent` call.
  Future<GenerateContentResponse> GenerateContentLastResult() const;

  /// @brief Generates a streaming response from a server prompt template and a
  /// map of template input variables.
  ///
  /// @param template_id The ID of the server prompt template.
  /// @param inputs Key-value map of template variables.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> GenerateContentStream(
      const std::string& template_id,
      const std::map<std::string, Variant>& inputs,
      const GenerateContentStreamCallback& on_chunk);

  /// @brief Generates a streaming response from a server prompt template and a
  /// raw JSON object string of template input variables.
  ///
  /// @param template_id The ID of the server prompt template.
  /// @param json_inputs A JSON object string of template variables.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> GenerateContentStreamJson(
      const std::string& template_id, const std::string& json_inputs,
      const GenerateContentStreamCallback& on_chunk);

  /// @brief Gets the result of the most recent `GenerateContentStream` call.
  Future<void> GenerateContentStreamLastResult() const;

  /// @brief Starts a multi-turn `TemplateChatSession` bound to `template_id`.
  ///
  /// @param template_id The ID of the server prompt template.
  /// @param inputs Optional template input variables.
  /// @param history Optional existing conversation history.
  /// @return A new `TemplateChatSession`.
  TemplateChatSession StartChat(const std::string& template_id,
                                const std::map<std::string, Variant>& inputs =
                                    std::map<std::string, Variant>(),
                                const std::vector<ModelContent>& history =
                                    std::vector<ModelContent>()) const;

 private:
  friend class FirebaseAI;
  friend class TemplateChatSession;
  friend class internal::FirebaseAIInternal;

  explicit TemplateGenerativeModel(
      const std::shared_ptr<internal::TemplateGenerativeModelInternal>&
          internal);

  std::shared_ptr<internal::TemplateGenerativeModelInternal> internal_;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TEMPLATE_GENERATIVE_MODEL_H_
