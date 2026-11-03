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

#ifndef FIREBASE_AI_SRC_COMMON_FIREBASE_AI_INTERNAL_H_
#define FIREBASE_AI_SRC_COMMON_FIREBASE_AI_INTERNAL_H_

#include <string>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "firebase/ai/function_calling.h"
#include "firebase/ai/generation_config.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/safety.h"
#include "firebase/ai/template_generative_model.h"
#include "firebase/ai/types.h"
#include "firebase/app.h"

namespace firebase {
namespace ai {
namespace internal {

class FirebaseAIInternal {
 public:
  FirebaseAIInternal(::firebase::App* app, const Backend& backend);
  ~FirebaseAIInternal();

  ::firebase::App* app() { return app_; }
  const ::firebase::App* app() const { return app_; }
  const Backend& backend() const { return backend_; }

  GenerativeModel GetGenerativeModel(
      const std::string& model_name,
      const Optional<GenerationConfig>& generation_config,
      const std::vector<SafetySetting>& safety_settings,
      const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
      const Optional<ModelContent>& system_instruction,
      const Optional<RequestOptions>& request_options,
      const Optional<HybridParams>& hybrid_params = Optional<HybridParams>());

  TemplateGenerativeModel GetTemplateGenerativeModel(
      const Optional<RequestOptions>& request_options);

 private:
  ::firebase::App* app_;
  Backend backend_;
  CleanupNotifier cleanup_;
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_FIREBASE_AI_INTERNAL_H_
