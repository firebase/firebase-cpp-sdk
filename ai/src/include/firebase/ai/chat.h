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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_CHAT_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_CHAT_H_

#include <memory>
#include <string>
#include <vector>

#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/future.h"

namespace firebase {
namespace ai {

namespace internal {
class ChatInternal;
}  // namespace internal

/// @brief An object that represents a back-and-forth multi-turn conversation
/// with a `GenerativeModel`, capturing the history of messages sent and
/// received.
///
/// Mirrors `Firebase.AI.Chat` in Unity and `ChatSession` in Flutter.
class Chat {
 public:
  /// @brief Default constructor creates an invalid `Chat`.
  Chat();

  /// @brief Copy constructor.
  Chat(const Chat& other);

  /// @brief Copy assignment operator.
  Chat& operator=(const Chat& other);

  /// @brief Destructor.
  ~Chat();

  /// @brief Returns true if this `Chat` instance is valid.
  bool is_valid() const { return internal_ != nullptr; }

  /// @brief Returns the conversation history accumulated in this `Chat`
  /// session.
  std::vector<ModelContent> history() const;

  /// @brief Sends a text message to the model within the conversation context
  /// and appends both the user turn and the model's response turn to
  /// `history()`.
  ///
  /// @param prompt The user text message.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> SendMessage(const std::string& prompt);

  /// @brief Sends a single `ModelContent` message within the conversation
  /// context and updates `history()`.
  ///
  /// @param content The user `ModelContent` turn.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> SendMessage(const ModelContent& content);

  /// @brief Sends multiple `ModelContent` messages within the conversation
  /// context and updates `history()`.
  ///
  /// @param content The user `ModelContent` turns.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> SendMessage(
      const std::vector<ModelContent>& content);

  /// @brief Gets the result of the most recent `SendMessage` call.
  ///
  /// @return A `Future` from the most recent `SendMessage` call.
  Future<GenerateContentResponse> SendMessageLastResult() const;

  /// @brief Sends a text message to the model and streams back response chunks
  /// via `on_chunk`, appending the aggregated response turn to `history()` upon
  /// completion.
  ///
  /// @param prompt The user text message.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> SendMessageStream(const std::string& prompt,
                                 const GenerateContentStreamCallback& on_chunk);

  /// @brief Sends a single `ModelContent` message and streams back response
  /// chunks via `on_chunk`, appending the aggregated response turn to
  /// `history()` upon completion.
  ///
  /// @param content The user `ModelContent` turn.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> SendMessageStream(const ModelContent& content,
                                 const GenerateContentStreamCallback& on_chunk);

  /// @brief Sends multiple `ModelContent` messages and streams back response
  /// chunks via `on_chunk`, appending the aggregated response turn to
  /// `history()` upon completion.
  ///
  /// @param content The user `ModelContent` turns.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> SendMessageStream(const std::vector<ModelContent>& content,
                                 const GenerateContentStreamCallback& on_chunk);

  /// @brief Gets the result of the most recent `SendMessageStream` call.
  ///
  /// @return A `Future<void>` from the most recent `SendMessageStream` call.
  Future<void> SendMessageStreamLastResult() const;

  /// @brief Returns the active `InferenceMode` of the underlying
  /// `GenerativeModel`.
  InferenceMode inference_mode() const;

  /// @brief Dynamically updates the `InferenceMode` of the underlying
  /// `GenerativeModel` mid-conversation, preserving `history()` across cloud
  /// and on-device turns.
  void set_inference_mode(InferenceMode mode);

  /// @brief Returns true if the underlying on-device LiteRT model is available.
  bool IsOnDeviceAvailable() const;

  /// @brief Clears all accumulated conversation turns from `history()`.
  void ClearHistory();

  /// @brief Summarizes and compacts the accumulated `history()` using the
  /// underlying `GenerativeModel` (in its active `InferenceMode`), replacing
  /// older turns with a concise summary turn so multi-turn conversations never
  /// exhaust the context window.
  ///
  /// @return A `Future` containing the summary `GenerateContentResponse`.
  Future<GenerateContentResponse> CompactHistory();

 private:
  friend class GenerativeModel;

  explicit Chat(const std::shared_ptr<internal::ChatInternal>& internal);

  std::shared_ptr<internal::ChatInternal> internal_;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_CHAT_H_
