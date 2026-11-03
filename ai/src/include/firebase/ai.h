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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_H_

#include <string>
#include <vector>

#include "firebase/ai/chat.h"
#include "firebase/ai/function_calling.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generation_config.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/safety.h"
#include "firebase/ai/schema.h"
#include "firebase/ai/template_chat_session.h"
#include "firebase/ai/template_generative_model.h"
#include "firebase/ai/types.h"
#include "firebase/app.h"
#include "firebase/internal/common.h"

FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(ai)

namespace firebase {

/// @brief Namespace for the Firebase AI Logic C++ SDK.
namespace ai {

namespace internal {
class FirebaseAIInternal;
}  // namespace internal

/// @brief Entry point for all Firebase AI Logic functionality.
///
/// Mirrors `Firebase.AI.FirebaseAI` in the Unity SDK and `FirebaseAI` in the
/// Flutter SDK.
class FirebaseAI {
 public:
  /// @brief Destructor. You may delete an instance of `FirebaseAI` when
  /// finished using it; all instances are also automatically cleaned up when
  /// the owning `firebase::App` is destroyed.
  ~FirebaseAI();

  /// @brief Gets the `FirebaseAI` instance for the default `firebase::App` and
  /// `Backend` (defaults to `Backend::GoogleAI()`).
  ///
  /// @param backend The backend provider to use (`Backend::GoogleAI()` or
  /// `Backend::Enterprise(location)` / `Backend::VertexAI(location)`).
  /// @return Pointer to the `FirebaseAI` instance, or `nullptr` if the default
  /// `App` does not exist.
  static FirebaseAI* GetInstance(const Backend& backend = Backend::GoogleAI());

  /// @brief Gets the `FirebaseAI` instance for the specified `firebase::App`
  /// and `Backend`.
  ///
  /// @param app The `firebase::App` instance to use.
  /// @param backend The backend provider to use (`Backend::GoogleAI()` or
  /// `Backend::Enterprise(location)` / `Backend::VertexAI(location)`).
  /// @return Pointer to the `FirebaseAI` instance, or `nullptr` if `app` is
  /// null.
  static FirebaseAI* GetInstance(::firebase::App* app,
                                 const Backend& backend = Backend::GoogleAI());

  /// @brief Returns the `firebase::App` that this `FirebaseAI` instance is
  /// associated with.
  ::firebase::App* app();

  /// @brief Returns the `firebase::App` that this `FirebaseAI` instance is
  /// associated with (const overload).
  const ::firebase::App* app() const;

  /// @brief Returns the `Backend` configuration for this `FirebaseAI` instance.
  const Backend& backend() const;

  /// @brief Initializes a `GenerativeModel` with the given parameters.
  ///
  /// @param model_name The name of the Gemini model to use (for example,
  /// `"gemini-2.5-flash"`).
  /// @param generation_config Optional content generation configuration.
  /// @param safety_settings Optional safety filtering thresholds.
  /// @param tools Optional list of tools (`FunctionDeclaration`,
  /// `GoogleSearch`, `CodeExecution`, `GoogleMaps`, `UrlContext`) the model may
  /// use.
  /// @param tool_config Optional tool configuration (`FunctionCallingConfig`,
  /// `RetrievalConfig`).
  /// @param system_instruction Optional system instruction (`ModelContent`)
  /// guiding the model's behavior.
  /// @param request_options Optional per-request options (such as timeout and
  /// limited-use App Check tokens).
  /// @param hybrid_params Optional hybrid on-device + cloud LiteRT
  /// configuration (`InferenceMode` and `OnDeviceParams`).
  /// @return The initialized `GenerativeModel` instance.
  GenerativeModel GetGenerativeModel(
      const std::string& model_name,
      const Optional<GenerationConfig>& generation_config =
          Optional<GenerationConfig>(),
      const std::vector<SafetySetting>& safety_settings =
          std::vector<SafetySetting>(),
      const std::vector<Tool>& tools = std::vector<Tool>(),
      const Optional<ToolConfig>& tool_config = Optional<ToolConfig>(),
      const Optional<ModelContent>& system_instruction =
          Optional<ModelContent>(),
      const Optional<RequestOptions>& request_options =
          Optional<RequestOptions>(),
      const Optional<HybridParams>& hybrid_params = Optional<HybridParams>());

  /// @brief Convenience overload to initialize a hybrid `GenerativeModel` with
  /// `HybridParams` (LiteRT on-device + cloud inference).
  GenerativeModel GetGenerativeModel(
      const std::string& model_name, const HybridParams& hybrid_params,
      const Optional<GenerationConfig>& generation_config =
          Optional<GenerationConfig>(),
      const Optional<ModelContent>& system_instruction =
          Optional<ModelContent>());

  /// @brief Initializes a `TemplateGenerativeModel` for executing server prompt
  /// templates.
  ///
  /// @param request_options Optional per-request options (such as timeout and
  /// limited-use App Check tokens).
  /// @return The initialized `TemplateGenerativeModel` instance.
  TemplateGenerativeModel GetTemplateGenerativeModel(
      const Optional<RequestOptions>& request_options =
          Optional<RequestOptions>());

 private:
  FirebaseAI(::firebase::App* app, const Backend& backend);
  FirebaseAI(const FirebaseAI&) = delete;
  FirebaseAI& operator=(const FirebaseAI&) = delete;

  void DeleteInternal();

  internal::FirebaseAIInternal* internal_;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_H_
