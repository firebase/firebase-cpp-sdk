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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TEMPLATE_CHAT_SESSION_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TEMPLATE_CHAT_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/future.h"

namespace firebase {
namespace ai {

class TemplateGenerativeModel;

namespace internal {
class TemplateChatSessionInternal;
}  // namespace internal

/// @brief Multi-turn chat session backed by a server prompt template on
/// `TemplateGenerativeModel`.
class TemplateChatSession {
 public:
  /// @brief Default constructor creates an invalid `TemplateChatSession`.
  TemplateChatSession();

  /// @brief Copy constructor.
  TemplateChatSession(const TemplateChatSession& other);

  /// @brief Copy assignment operator.
  TemplateChatSession& operator=(const TemplateChatSession& other);

  /// @brief Destructor.
  ~TemplateChatSession();

  /// @brief Returns true if this `TemplateChatSession` is valid.
  bool is_valid() const { return internal_ != nullptr; }

  /// @brief Returns the conversation history accumulated in this session.
  std::vector<ModelContent> history() const;

  /// @brief Sends a text message in this template chat session.
  Future<GenerateContentResponse> SendMessage(const std::string& prompt);

  /// @brief Sends a `ModelContent` message in this template chat session.
  Future<GenerateContentResponse> SendMessage(const ModelContent& content);

  /// @brief Sends multiple `ModelContent` messages in this template chat
  /// session.
  Future<GenerateContentResponse> SendMessage(
      const std::vector<ModelContent>& content);

  /// @brief Gets the result of the most recent `SendMessage` call.
  Future<GenerateContentResponse> SendMessageLastResult() const;

  /// @brief Sends a text message and streams back response chunks via
  /// `on_chunk`.
  Future<void> SendMessageStream(const std::string& prompt,
                                 const GenerateContentStreamCallback& on_chunk);

  /// @brief Sends a `ModelContent` message and streams back response chunks via
  /// `on_chunk`.
  Future<void> SendMessageStream(const ModelContent& content,
                                 const GenerateContentStreamCallback& on_chunk);

  /// @brief Sends multiple `ModelContent` messages and streams back response
  /// chunks via `on_chunk`.
  Future<void> SendMessageStream(const std::vector<ModelContent>& content,
                                 const GenerateContentStreamCallback& on_chunk);

  /// @brief Gets the result of the most recent `SendMessageStream` call.
  Future<void> SendMessageStreamLastResult() const;

 private:
  friend class TemplateGenerativeModel;

  explicit TemplateChatSession(
      const std::shared_ptr<internal::TemplateChatSessionInternal>& internal);

  std::shared_ptr<internal::TemplateChatSessionInternal> internal_;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TEMPLATE_CHAT_SESSION_H_
