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

#ifndef FIREBASE_AI_SRC_COMMON_GENERATIVE_MODEL_INTERNAL_H_
#define FIREBASE_AI_SRC_COMMON_GENERATIVE_MODEL_INTERNAL_H_

#include <memory>
#include <string>
#include <vector>

#include "ai/src/common/litert_adapter.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/ai/function_calling.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generation_config.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/safety.h"
#include "firebase/ai/types.h"
#include "firebase/app.h"
#include "firebase/future.h"

namespace firebase {
namespace ai {
namespace internal {

enum GenerativeModelFn {
  kGenerativeModelFnGenerateContent = 0,
  kGenerativeModelFnGenerateContentStream,
  kGenerativeModelFnCountTokens,
  kGenerativeModelFnInitializeOnDeviceModel,
  kGenerativeModelFnCount
};

class GenerativeModelInternal
    : public std::enable_shared_from_this<GenerativeModelInternal> {
 public:
  GenerativeModelInternal(
      ::firebase::App* app, const Backend& backend,
      const std::string& model_name,
      const Optional<GenerationConfig>& generation_config,
      const std::vector<SafetySetting>& safety_settings,
      const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
      const Optional<ModelContent>& system_instruction,
      const Optional<RequestOptions>& request_options,
      const Optional<HybridParams>& hybrid_params = Optional<HybridParams>());

  ~GenerativeModelInternal();

  Future<GenerateContentResponse> GenerateContent(
      const std::vector<ModelContent>& content);
  Future<GenerateContentResponse> GenerateContentLastResult() const;

  Future<void> GenerateContentStream(
      const std::vector<ModelContent>& content,
      const GenerateContentStreamCallback& on_chunk);
  Future<void> GenerateContentStreamLastResult() const;

  Future<CountTokensResponse> CountTokens(
      const std::vector<ModelContent>& content);
  Future<CountTokensResponse> CountTokensLastResult() const;

  InferenceMode inference_mode() const;
  void set_inference_mode(InferenceMode mode);
  bool IsOnDeviceAvailable() const;
  Future<void> InitializeOnDeviceModel();

  ::firebase::App* app() const { return app_; }
  const Backend& backend() const { return backend_; }
  const std::string& model_name() const { return model_name_; }

 private:
  void GenerateContentCloud(const std::vector<ModelContent>& content,
                            SafeFutureHandle<GenerateContentResponse> handle,
                            bool fallback_to_on_device_on_error);

  void GenerateContentStreamCloud(const std::vector<ModelContent>& content,
                                  const GenerateContentStreamCallback& on_chunk,
                                  SafeFutureHandle<void> handle,
                                  bool fallback_to_on_device_on_error);

  void CountTokensCloud(const std::vector<ModelContent>& content,
                        SafeFutureHandle<CountTokensResponse> handle,
                        bool fallback_to_on_device_on_error);

  ::firebase::App* app_;
  Backend backend_;
  std::string model_name_;
  Optional<GenerationConfig> generation_config_;
  std::vector<SafetySetting> safety_settings_;
  std::vector<Tool> tools_;
  Optional<ToolConfig> tool_config_;
  Optional<ModelContent> system_instruction_;
  RequestOptions request_options_;
  mutable Mutex mode_mutex_;
  InferenceMode inference_mode_;
  std::shared_ptr<LiteRtAdapter> litert_adapter_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_GENERATIVE_MODEL_INTERNAL_H_
