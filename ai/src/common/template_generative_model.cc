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

#include "firebase/ai/template_generative_model.h"

#include "ai/src/common/http_client.h"
#include "ai/src/common/serialization.h"
#include "ai/src/common/template_generative_model_internal.h"
#include "firebase/ai/template_chat_session.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

struct TemplateStreamTurnAggregator {
  std::vector<Part> accumulated_parts;

  void AddChunk(const GenerateContentResponse& chunk) {
    if (chunk.candidates().empty()) return;
    const ModelContent& content = chunk.candidates()[0].content;
    for (const auto& part : content.parts()) {
      if (part.is_text() && !accumulated_parts.empty()) {
        Part& last = accumulated_parts.back();
        if (last.is_text() && last.is_thought() == part.is_thought() &&
            !last.thought_signature().has_value() &&
            !part.thought_signature().has_value()) {
          std::string combined = last.text_part().text + part.text_part().text;
          last = Part(TextPart(combined), last.is_thought(),
                      last.thought_signature());
          continue;
        }
      }
      accumulated_parts.push_back(part);
    }
  }

  ModelContent BuildModelTurn() const {
    return ModelContent("model", accumulated_parts);
  }
};

}  // namespace

TemplateGenerativeModelInternal::TemplateGenerativeModelInternal(
    ::firebase::App* app, const Backend& backend,
    const Optional<RequestOptions>& request_options)
    : app_(app),
      backend_(backend),
      request_options_(request_options.value_or(RequestOptions())),
      future_impl_(
          new ReferenceCountedFutureImpl(kTemplateGenerativeModelFnCount)) {}

TemplateGenerativeModelInternal::~TemplateGenerativeModelInternal() {}

Future<GenerateContentResponse>
TemplateGenerativeModelInternal::GenerateContent(
    const std::string& template_id,
    const std::map<std::string, Variant>& inputs,
    const std::vector<ModelContent>& history) {
  std::string body = BuildTemplateGenerateContentRequestJson(inputs, history);
  return ExecuteGenerateContentWithBody(template_id, body);
}

Future<GenerateContentResponse>
TemplateGenerativeModelInternal::GenerateContentJson(
    const std::string& template_id, const std::string& json_inputs,
    const std::vector<ModelContent>& history) {
  std::string body =
      BuildTemplateGenerateContentRequestFromRawJson(json_inputs, history);
  return ExecuteGenerateContentWithBody(template_id, body);
}

Future<GenerateContentResponse>
TemplateGenerativeModelInternal::ExecuteGenerateContentWithBody(
    const std::string& template_id, const std::string& body) {
  SafeFutureHandle<GenerateContentResponse> handle =
      future_impl_->SafeAlloc<GenerateContentResponse>(
          kTemplateGenerativeModelFnGenerateContent);

  if (!app_ || template_id.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Template ID must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::string url = AiHttpClient::ConstructTemplateUrl(
      app_, backend_, template_id, "templateGenerateContent");
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
  BackendProvider provider = backend_.provider();

  AiHttpClient::SendUnaryJson(
      app_, request_options_, url, body,
      [future_impl, handle, provider](Error err, const std::string& err_msg,
                                      const std::string& response_body) {
        if (err != kErrorNone) {
          future_impl->Complete(handle, err, err_msg.c_str());
          return;
        }
        GenerateContentResponse parsed;
        std::string parse_err;
        if (!ParseGenerateContentResponseJson(response_body, provider, &parsed,
                                              &parse_err)) {
          future_impl->Complete(handle, kErrorSerializationFailed,
                                parse_err.c_str());
          return;
        }
        future_impl->CompleteWithResult(handle, kErrorNone, "", parsed);
      });

  return MakeFuture(future_impl_.get(), handle);
}

Future<GenerateContentResponse>
TemplateGenerativeModelInternal::GenerateContentLastResult() const {
  return static_cast<const Future<GenerateContentResponse>&>(
      future_impl_->LastResult(kTemplateGenerativeModelFnGenerateContent));
}

Future<void> TemplateGenerativeModelInternal::GenerateContentStream(
    const std::string& template_id,
    const std::map<std::string, Variant>& inputs,
    const GenerateContentStreamCallback& on_chunk,
    const std::vector<ModelContent>& history) {
  std::string body = BuildTemplateGenerateContentRequestJson(inputs, history);
  return ExecuteGenerateContentStreamWithBody(template_id, body, on_chunk);
}

Future<void> TemplateGenerativeModelInternal::GenerateContentStreamJson(
    const std::string& template_id, const std::string& json_inputs,
    const GenerateContentStreamCallback& on_chunk,
    const std::vector<ModelContent>& history) {
  std::string body =
      BuildTemplateGenerateContentRequestFromRawJson(json_inputs, history);
  return ExecuteGenerateContentStreamWithBody(template_id, body, on_chunk);
}

Future<void>
TemplateGenerativeModelInternal::ExecuteGenerateContentStreamWithBody(
    const std::string& template_id, const std::string& body,
    const GenerateContentStreamCallback& on_chunk) {
  SafeFutureHandle<void> handle = future_impl_->SafeAlloc<void>(
      kTemplateGenerativeModelFnGenerateContentStream);

  if (!app_ || template_id.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Template ID must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::string url = AiHttpClient::ConstructTemplateUrl(
      app_, backend_, template_id, "templateStreamGenerateContent?alt=sse");
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  AiHttpClient::SendStreamJson(
      app_, backend_, request_options_, url, body, on_chunk,
      [future_impl, handle](Error err, const std::string& err_msg,
                            const std::string& /*response_body*/) {
        future_impl->Complete(handle, err, err_msg.c_str());
      });

  return MakeFuture(future_impl_.get(), handle);
}

Future<void> TemplateGenerativeModelInternal::GenerateContentStreamLastResult()
    const {
  return static_cast<const Future<void>&>(future_impl_->LastResult(
      kTemplateGenerativeModelFnGenerateContentStream));
}

// --- TemplateChatSessionInternal ---

TemplateChatSessionInternal::TemplateChatSessionInternal(
    const std::shared_ptr<TemplateGenerativeModelInternal>& model,
    const std::string& template_id,
    const std::map<std::string, Variant>& inputs,
    const std::vector<ModelContent>& initial_history)
    : model_(model),
      template_id_(template_id),
      inputs_(inputs),
      history_(initial_history),
      future_impl_(
          new ReferenceCountedFutureImpl(kTemplateChatSessionFnCount)) {}

TemplateChatSessionInternal::~TemplateChatSessionInternal() {}

std::vector<ModelContent> TemplateChatSessionInternal::history() const {
  MutexLock lock(history_mutex_);
  return history_;
}

void TemplateChatSessionInternal::AppendHistoryTurn(
    const std::vector<ModelContent>& request_turns,
    const ModelContent& response_turn) {
  MutexLock lock(history_mutex_);
  history_.insert(history_.end(), request_turns.begin(), request_turns.end());
  history_.push_back(response_turn);
}

Future<GenerateContentResponse> TemplateChatSessionInternal::SendMessage(
    const std::vector<ModelContent>& content) {
  SafeFutureHandle<GenerateContentResponse> handle =
      future_impl_->SafeAlloc<GenerateContentResponse>(
          kTemplateChatSessionFnSendMessage);

  if (!model_ || content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Chat message content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::vector<ModelContent> full_history;
  {
    MutexLock lock(history_mutex_);
    full_history = history_;
  }
  full_history.insert(full_history.end(), content.begin(), content.end());

  std::shared_ptr<TemplateChatSessionInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  Future<GenerateContentResponse> inner =
      model_->GenerateContent(template_id_, inputs_, full_history);
  inner.OnCompletion([self, future_impl, handle, content](
                         const Future<GenerateContentResponse>& completed) {
    if (completed.error() != kErrorNone || completed.result() == nullptr) {
      future_impl->Complete(
          handle, completed.error(),
          completed.error_message() ? completed.error_message() : "");
      return;
    }
    const GenerateContentResponse& resp = *completed.result();
    if (!resp.candidates().empty()) {
      ModelContent model_turn = resp.candidates()[0].content;
      if (model_turn.role().empty()) model_turn.set_role("model");
      self->AppendHistoryTurn(content, model_turn);
    }
    future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
  });

  return MakeFuture(future_impl_.get(), handle);
}

Future<GenerateContentResponse>
TemplateChatSessionInternal::SendMessageLastResult() const {
  return static_cast<const Future<GenerateContentResponse>&>(
      future_impl_->LastResult(kTemplateChatSessionFnSendMessage));
}

Future<void> TemplateChatSessionInternal::SendMessageStream(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk) {
  SafeFutureHandle<void> handle =
      future_impl_->SafeAlloc<void>(kTemplateChatSessionFnSendMessageStream);

  if (!model_ || content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Chat message content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::vector<ModelContent> full_history;
  {
    MutexLock lock(history_mutex_);
    full_history = history_;
  }
  full_history.insert(full_history.end(), content.begin(), content.end());

  std::shared_ptr<TemplateChatSessionInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
  std::shared_ptr<TemplateStreamTurnAggregator> aggregator(
      new TemplateStreamTurnAggregator());

  Future<void> inner = model_->GenerateContentStream(
      template_id_, inputs_,
      [aggregator, on_chunk](const GenerateContentResponse& chunk) {
        aggregator->AddChunk(chunk);
        if (on_chunk) on_chunk(chunk);
      },
      full_history);

  inner.OnCompletion([self, future_impl, handle, content,
                      aggregator](const Future<void>& completed) {
    if (completed.error() == kErrorNone &&
        !aggregator->accumulated_parts.empty()) {
      self->AppendHistoryTurn(content, aggregator->BuildModelTurn());
    }
    future_impl->Complete(
        handle, completed.error(),
        completed.error_message() ? completed.error_message() : "");
  });

  return MakeFuture(future_impl_.get(), handle);
}

Future<void> TemplateChatSessionInternal::SendMessageStreamLastResult() const {
  return static_cast<const Future<void>&>(
      future_impl_->LastResult(kTemplateChatSessionFnSendMessageStream));
}

}  // namespace internal

// --- TemplateGenerativeModel public implementation ---

TemplateGenerativeModel::TemplateGenerativeModel() : internal_(nullptr) {}

TemplateGenerativeModel::TemplateGenerativeModel(
    const std::shared_ptr<internal::TemplateGenerativeModelInternal>& internal)
    : internal_(internal) {}

TemplateGenerativeModel::TemplateGenerativeModel(
    const TemplateGenerativeModel& other)
    : internal_(other.internal_) {}

TemplateGenerativeModel& TemplateGenerativeModel::operator=(
    const TemplateGenerativeModel& other) {
  if (this != &other) {
    internal_ = other.internal_;
  }
  return *this;
}

TemplateGenerativeModel::~TemplateGenerativeModel() {}

Future<GenerateContentResponse> TemplateGenerativeModel::GenerateContent(
    const std::string& template_id,
    const std::map<std::string, Variant>& inputs) {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->GenerateContent(template_id, inputs);
}

Future<GenerateContentResponse> TemplateGenerativeModel::GenerateContentJson(
    const std::string& template_id, const std::string& json_inputs) {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->GenerateContentJson(template_id, json_inputs);
}

Future<GenerateContentResponse>
TemplateGenerativeModel::GenerateContentLastResult() const {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->GenerateContentLastResult();
}

Future<void> TemplateGenerativeModel::GenerateContentStream(
    const std::string& template_id,
    const std::map<std::string, Variant>& inputs,
    const GenerateContentStreamCallback& on_chunk) {
  if (!internal_) return Future<void>();
  return internal_->GenerateContentStream(template_id, inputs, on_chunk);
}

Future<void> TemplateGenerativeModel::GenerateContentStreamJson(
    const std::string& template_id, const std::string& json_inputs,
    const GenerateContentStreamCallback& on_chunk) {
  if (!internal_) return Future<void>();
  return internal_->GenerateContentStreamJson(template_id, json_inputs,
                                              on_chunk);
}

Future<void> TemplateGenerativeModel::GenerateContentStreamLastResult() const {
  if (!internal_) return Future<void>();
  return internal_->GenerateContentStreamLastResult();
}

TemplateChatSession TemplateGenerativeModel::StartChat(
    const std::string& template_id,
    const std::map<std::string, Variant>& inputs,
    const std::vector<ModelContent>& history) const {
  if (!internal_) return TemplateChatSession();
  return TemplateChatSession(
      std::shared_ptr<internal::TemplateChatSessionInternal>(
          new internal::TemplateChatSessionInternal(internal_, template_id,
                                                    inputs, history)));
}

// --- TemplateChatSession public implementation ---

TemplateChatSession::TemplateChatSession() : internal_(nullptr) {}

TemplateChatSession::TemplateChatSession(
    const std::shared_ptr<internal::TemplateChatSessionInternal>& internal)
    : internal_(internal) {}

TemplateChatSession::TemplateChatSession(const TemplateChatSession& other)
    : internal_(other.internal_) {}

TemplateChatSession& TemplateChatSession::operator=(
    const TemplateChatSession& other) {
  if (this != &other) {
    internal_ = other.internal_;
  }
  return *this;
}

TemplateChatSession::~TemplateChatSession() {}

std::vector<ModelContent> TemplateChatSession::history() const {
  if (!internal_) return std::vector<ModelContent>();
  return internal_->history();
}

Future<GenerateContentResponse> TemplateChatSession::SendMessage(
    const std::string& prompt) {
  return SendMessage(std::vector<ModelContent>(1, ModelContent::Text(prompt)));
}

Future<GenerateContentResponse> TemplateChatSession::SendMessage(
    const ModelContent& content) {
  return SendMessage(std::vector<ModelContent>(1, content));
}

Future<GenerateContentResponse> TemplateChatSession::SendMessage(
    const std::vector<ModelContent>& content) {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->SendMessage(content);
}

Future<GenerateContentResponse> TemplateChatSession::SendMessageLastResult()
    const {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->SendMessageLastResult();
}

Future<void> TemplateChatSession::SendMessageStream(
    const std::string& prompt, const GenerateContentStreamCallback& on_chunk) {
  return SendMessageStream(
      std::vector<ModelContent>(1, ModelContent::Text(prompt)), on_chunk);
}

Future<void> TemplateChatSession::SendMessageStream(
    const ModelContent& content,
    const GenerateContentStreamCallback& on_chunk) {
  return SendMessageStream(std::vector<ModelContent>(1, content), on_chunk);
}

Future<void> TemplateChatSession::SendMessageStream(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk) {
  if (!internal_) return Future<void>();
  return internal_->SendMessageStream(content, on_chunk);
}

Future<void> TemplateChatSession::SendMessageStreamLastResult() const {
  if (!internal_) return Future<void>();
  return internal_->SendMessageStreamLastResult();
}

}  // namespace ai
}  // namespace firebase
