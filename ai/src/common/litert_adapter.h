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

#ifndef FIREBASE_AI_SRC_COMMON_LITERT_ADAPTER_H_
#define FIREBASE_AI_SRC_COMMON_LITERT_ADAPTER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/src/include/firebase/internal/mutex.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generation_config.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/types.h"

namespace firebase {
namespace ai {
namespace internal {

/// @brief Callback invoked when local LiteRT / LiteRT-LM inference completes.
typedef std::function<void(Error error, const std::string& error_message,
                           const GenerateContentResponse& response)>
    LiteRtCompletionCallback;

/// @brief Callback invoked when local LiteRT / LiteRT-LM token counting
/// completes.
typedef std::function<void(Error error, const std::string& error_message,
                           const CountTokensResponse& response)>
    LiteRtCountTokensCallback;

/// @brief Adapter for on-device inference using Google AI Edge LiteRT
/// (`litert::CompiledModel` / `libLiteRt`) and LiteRT-LM (`libCLiteRTLM` for
/// Gemma `.litertlm` models).
class LiteRtAdapter : public std::enable_shared_from_this<LiteRtAdapter> {
 public:
  explicit LiteRtAdapter(const OnDeviceParams& params);
  ~LiteRtAdapter();

  /// @brief Returns true if the configured on-device model is available and the
  /// corresponding LiteRT / LiteRT-LM runtime can be loaded.
  bool IsAvailable() const;

  /// @brief Initializes the LiteRT `CompiledModel` or LiteRT-LM `Engine` if not
  /// already initialized.
  bool Initialize(std::string* out_error);

  /// @brief Generates content locally on-device using LiteRT / LiteRT-LM.
  void GenerateContentAsync(const std::vector<ModelContent>& content,
                            const Optional<GenerationConfig>& generation_config,
                            const Optional<ModelContent>& system_instruction,
                            const LiteRtCompletionCallback& callback);

  /// @brief Streams content locally on-device using LiteRT / LiteRT-LM.
  void GenerateContentStreamAsync(
      const std::vector<ModelContent>& content,
      const Optional<GenerationConfig>& generation_config,
      const Optional<ModelContent>& system_instruction,
      const GenerateContentStreamCallback& on_chunk,
      const LiteRtCompletionCallback& on_complete);

  /// @brief Counts tokens locally using the LiteRT-LM tokenizer (or heuristic
  /// fallback for raw `.tflite` / simulated models).
  void CountTokensAsync(const std::vector<ModelContent>& content,
                        const LiteRtCountTokensCallback& callback);

  const OnDeviceParams& params() const { return params_; }

 private:
  struct RuntimeState;

  bool GenerateContentSync(const std::vector<ModelContent>& content,
                           const Optional<GenerationConfig>& generation_config,
                           const Optional<ModelContent>& system_instruction,
                           const GenerateContentStreamCallback& on_chunk,
                           GenerateContentResponse* out_response,
                           std::string* out_error);

  bool CountTokensSync(const std::vector<ModelContent>& content,
                       CountTokensResponse* out_response,
                       std::string* out_error);

  OnDeviceParams params_;
  mutable Mutex mutex_;
  std::unique_ptr<RuntimeState> state_;
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_LITERT_ADAPTER_H_
