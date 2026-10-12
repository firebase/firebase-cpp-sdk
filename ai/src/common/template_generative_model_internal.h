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

#ifndef FIREBASE_AI_SRC_COMMON_TEMPLATE_GENERATIVE_MODEL_INTERNAL_H_
#define FIREBASE_AI_SRC_COMMON_TEMPLATE_GENERATIVE_MODEL_INTERNAL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/ai/template_chat_session.h"
#include "firebase/ai/template_generative_model.h"
#include "firebase/ai/types.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/variant.h"

namespace firebase {
namespace ai {
namespace internal {

enum TemplateGenerativeModelFn {
  kTemplateGenerativeModelFnGenerateContent = 0,
  kTemplateGenerativeModelFnGenerateContentStream,
  kTemplateGenerativeModelFnCount
};

enum TemplateChatSessionFn {
  kTemplateChatSessionFnSendMessage = 0,
  kTemplateChatSessionFnSendMessageStream,
  kTemplateChatSessionFnCount
};

class TemplateGenerativeModelInternal {
 public:
  TemplateGenerativeModelInternal(
      ::firebase::App* app, const Backend& backend,
      const Optional<RequestOptions>& request_options);
  ~TemplateGenerativeModelInternal();

  Future<GenerateContentResponse> GenerateContent(
      const std::string& template_id,
      const std::map<std::string, Variant>& inputs,
      const std::vector<ModelContent>& history = std::vector<ModelContent>());

  Future<GenerateContentResponse> GenerateContentJson(
      const std::string& template_id, const std::string& json_inputs,
      const std::vector<ModelContent>& history = std::vector<ModelContent>());

  Future<GenerateContentResponse> GenerateContentLastResult() const;

  Future<void> GenerateContentStream(
      const std::string& template_id,
      const std::map<std::string, Variant>& inputs,
      const GenerateContentStreamCallback& on_chunk,
      const std::vector<ModelContent>& history = std::vector<ModelContent>());

  Future<void> GenerateContentStreamJson(
      const std::string& template_id, const std::string& json_inputs,
      const GenerateContentStreamCallback& on_chunk,
      const std::vector<ModelContent>& history = std::vector<ModelContent>());

  Future<void> GenerateContentStreamLastResult() const;

 private:
  Future<GenerateContentResponse> ExecuteGenerateContentWithBody(
      const std::string& template_id, const std::string& body);
  Future<void> ExecuteGenerateContentStreamWithBody(
      const std::string& template_id, const std::string& body,
      const GenerateContentStreamCallback& on_chunk);

  ::firebase::App* app_;
  Backend backend_;
  RequestOptions request_options_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

class TemplateChatSessionInternal
    : public std::enable_shared_from_this<TemplateChatSessionInternal> {
 public:
  TemplateChatSessionInternal(
      const std::shared_ptr<TemplateGenerativeModelInternal>& model,
      const std::string& template_id,
      const std::map<std::string, Variant>& inputs,
      const std::vector<ModelContent>& initial_history);
  ~TemplateChatSessionInternal();

  std::vector<ModelContent> history() const;

  Future<GenerateContentResponse> SendMessage(
      const std::vector<ModelContent>& content);
  Future<GenerateContentResponse> SendMessageLastResult() const;

  Future<void> SendMessageStream(const std::vector<ModelContent>& content,
                                 const GenerateContentStreamCallback& on_chunk);
  Future<void> SendMessageStreamLastResult() const;

 private:
  void AppendHistoryTurn(const std::vector<ModelContent>& request_turns,
                         const ModelContent& response_turn);

  std::shared_ptr<TemplateGenerativeModelInternal> model_;
  std::string template_id_;
  std::map<std::string, Variant> inputs_;
  mutable Mutex history_mutex_;
  std::vector<ModelContent> history_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_TEMPLATE_GENERATIVE_MODEL_INTERNAL_H_
