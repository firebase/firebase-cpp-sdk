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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATIVE_MODEL_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATIVE_MODEL_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "firebase/ai/function_calling.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generation_config.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/safety.h"
#include "firebase/ai/types.h"
#include "firebase/future.h"

namespace firebase {
namespace ai {

class Chat;
class FirebaseAI;

namespace internal {
class GenerativeModelInternal;
class FirebaseAIInternal;
}  // namespace internal

/// @brief Callback invoked for each incremental `GenerateContentResponse` chunk
/// received during a `GenerateContentStream` or `SendMessageStream` call.
typedef std::function<void(const GenerateContentResponse&)>
    GenerateContentStreamCallback;

/// @brief A type that represents a remote multimodal model (like Gemini) with
/// the ability to generate content, stream content, count tokens, and conduct
/// multi-turn chat sessions.
///
/// Mirrors `Firebase.AI.GenerativeModel` in Unity and `GenerativeModel` in
/// Flutter.
class GenerativeModel {
 public:
  /// @brief Default constructor creates an invalid `GenerativeModel`.
  GenerativeModel();

  /// @brief Copy constructor.
  GenerativeModel(const GenerativeModel& other);

  /// @brief Copy assignment operator.
  GenerativeModel& operator=(const GenerativeModel& other);

  /// @brief Destructor.
  ~GenerativeModel();

  /// @brief Returns true if this `GenerativeModel` is valid.
  bool is_valid() const { return internal_ != nullptr; }

  /// @brief Generates content from a single text prompt.
  ///
  /// @param prompt The input text prompt.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> GenerateContent(const std::string& prompt);

  /// @brief Generates content from a single `ModelContent` message.
  ///
  /// @param content The input `ModelContent`.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> GenerateContent(const ModelContent& content);

  /// @brief Generates content from a sequence of `ModelContent` messages.
  ///
  /// @param content The input `ModelContent` messages.
  /// @return A `Future` containing the `GenerateContentResponse`.
  Future<GenerateContentResponse> GenerateContent(
      const std::vector<ModelContent>& content);

  /// @brief Gets the result of the most recent `GenerateContent` call.
  ///
  /// @return A `Future` from the most recent `GenerateContent` call.
  Future<GenerateContentResponse> GenerateContentLastResult() const;

  /// @brief Generates a streaming response from a single text prompt, invoking
  /// `on_chunk` as each `GenerateContentResponse` chunk arrives over SSE.
  ///
  /// @param prompt The input text prompt.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> GenerateContentStream(
      const std::string& prompt, const GenerateContentStreamCallback& on_chunk);

  /// @brief Generates a streaming response from a single `ModelContent`
  /// message, invoking `on_chunk` as each chunk arrives over SSE.
  ///
  /// @param content The input `ModelContent`.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> GenerateContentStream(
      const ModelContent& content,
      const GenerateContentStreamCallback& on_chunk);

  /// @brief Generates a streaming response from a sequence of `ModelContent`
  /// messages, invoking `on_chunk` as each chunk arrives over SSE.
  ///
  /// @param content The input `ModelContent` messages.
  /// @param on_chunk Callback invoked for each response chunk.
  /// @return A `Future<void>` that completes when the stream finishes or fails.
  Future<void> GenerateContentStream(
      const std::vector<ModelContent>& content,
      const GenerateContentStreamCallback& on_chunk);

  /// @brief Gets the result of the most recent `GenerateContentStream` call.
  ///
  /// @return A `Future<void>` from the most recent `GenerateContentStream`
  /// call.
  Future<void> GenerateContentStreamLastResult() const;

  /// @brief Counts the number of tokens in a single text prompt.
  ///
  /// @param prompt The input text prompt.
  /// @return A `Future` containing the `CountTokensResponse`.
  Future<CountTokensResponse> CountTokens(const std::string& prompt);

  /// @brief Counts the number of tokens in a single `ModelContent` message.
  ///
  /// @param content The input `ModelContent`.
  /// @return A `Future` containing the `CountTokensResponse`.
  Future<CountTokensResponse> CountTokens(const ModelContent& content);

  /// @brief Counts the number of tokens in a sequence of `ModelContent`
  /// messages.
  ///
  /// @param content The input `ModelContent` messages.
  /// @return A `Future` containing the `CountTokensResponse`.
  Future<CountTokensResponse> CountTokens(
      const std::vector<ModelContent>& content);

  /// @brief Gets the result of the most recent `CountTokens` call.
  ///
  /// @return A `Future` from the most recent `CountTokens` call.
  Future<CountTokensResponse> CountTokensLastResult() const;

  /// @brief Creates a multi-turn `Chat` session using this `GenerativeModel`,
  /// optionally initialized with prior conversation history.
  ///
  /// @param history Optional existing conversation turns.
  /// @return A new `Chat` session instance.
  Chat StartChat(const std::vector<ModelContent>& history =
                     std::vector<ModelContent>()) const;

  /// @brief Returns the active `InferenceMode` (`ONLY_IN_CLOUD`,
  /// `ONLY_ON_DEVICE`, `PREFER_ON_DEVICE`, or `PREFER_IN_CLOUD`).
  InferenceMode inference_mode() const;

  /// @brief Dynamically updates the `InferenceMode` at runtime (for example,
  /// toggling a live `Chat` session between cloud Firebase AI and local
  /// LiteRT inference).
  void set_inference_mode(InferenceMode mode);

  /// @brief Checks whether the configured on-device LiteRT / LiteRT-LM model is
  /// available and can be loaded on the current device.
  bool IsOnDeviceAvailable() const;

  /// @brief Eagerly initializes the on-device LiteRT `CompiledModel` or
  /// LiteRT-LM engine in the background.
  Future<void> InitializeOnDeviceModel();

 private:
  friend class Chat;
  friend class FirebaseAI;
  friend class internal::FirebaseAIInternal;

  explicit GenerativeModel(
      const std::shared_ptr<internal::GenerativeModelInternal>& internal);

  std::shared_ptr<internal::GenerativeModelInternal> internal_;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATIVE_MODEL_H_
