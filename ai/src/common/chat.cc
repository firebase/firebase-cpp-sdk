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

#include "firebase/ai/chat.h"

#include "ai/src/common/chat_internal.h"
#include "firebase/ai/types.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

// Aggregates streamed response chunks into a single model turn for chat
// history, merging consecutive non-thought text parts and consecutive thought
// text parts while preserving thought signatures and non-text parts (such as
// function calls and inline data).
struct StreamTurnAggregator {
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

std::vector<ModelContent> NormalizeUserTurns(
    const std::vector<ModelContent>& content) {
  std::vector<ModelContent> normalized;
  normalized.reserve(content.size());
  for (const auto& c : content) {
    if (c.role() == "user" || c.role() == "model" || c.role() == "function") {
      normalized.push_back(c);
    } else {
      normalized.push_back(ModelContent("user", c.parts()));
    }
  }
  return normalized;
}

}  // namespace

ChatInternal::ChatInternal(
    const std::shared_ptr<GenerativeModelInternal>& model,
    const std::vector<ModelContent>& initial_history)
    : model_(model),
      history_(initial_history),
      future_impl_(new ReferenceCountedFutureImpl(kChatFnCount)) {}

ChatInternal::~ChatInternal() {}

std::vector<ModelContent> ChatInternal::history() const {
  MutexLock lock(history_mutex_);
  return history_;
}

void ChatInternal::ClearHistory() {
  MutexLock lock(history_mutex_);
  history_.clear();
}

Future<GenerateContentResponse> ChatInternal::CompactHistory() {
  SafeFutureHandle<GenerateContentResponse> handle =
      future_impl_->SafeAlloc<GenerateContentResponse>(kChatFnCompactHistory);

  std::vector<ModelContent> snapshot;
  {
    MutexLock lock(history_mutex_);
    snapshot = history_;
  }
  if (!model_ || snapshot.empty()) {
    Candidate cand;
    cand.content = ModelContent::Model("Conversation history is empty.");
    cand.finish_reason = kFinishReasonStop;
    std::vector<Candidate> cands(1, cand);
    GenerateContentResponse empty_resp(cands, Optional<PromptFeedback>(),
                                       Optional<UsageMetadata>(),
                                       kInferenceSourceOnDevice);
    future_impl_->CompleteWithResult(handle, kErrorNone, "", empty_resp);
    return MakeFuture(future_impl_.get(), handle);
  }

  std::string transcript =
      "Summarize the facts and context from the following conversation in "
      "concise bullet points. Include all names, numbers, secret code words, "
      "user preferences, and key topics. Output ONLY the factual bullet "
      "points:\n\n";
  for (size_t i = 0; i < snapshot.size(); ++i) {
    transcript += snapshot[i].role() + ": ";
    for (const auto& part : snapshot[i].parts()) {
      if (part.is_text() && !part.is_thought()) {
        transcript += part.text_part().text;
      }
    }
    transcript += "\n";
  }

  std::vector<ModelContent> compact_req;
  compact_req.push_back(ModelContent::Text(transcript));

  std::shared_ptr<ChatInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  Future<GenerateContentResponse> inner_future =
      model_->GenerateContent(compact_req);
  inner_future.OnCompletion(
      [self, future_impl,
       handle](const Future<GenerateContentResponse>& completed) {
        if (completed.error() != kErrorNone || completed.result() == nullptr) {
          future_impl->Complete(
              handle, completed.error(),
              completed.error_message() ? completed.error_message() : "");
          return;
        }
        const GenerateContentResponse& resp = *completed.result();
        std::string summary_text = resp.text();
        {
          MutexLock lock(self->history_mutex_);
          self->history_.clear();
          self->history_.push_back(ModelContent::Text(
              "[Compacted Conversation Context]\n" + summary_text));
          self->history_.push_back(ModelContent::Model(
              "Understood. I have the compacted conversation context above."));
        }
        future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
      });

  return MakeFuture(future_impl_.get(), handle);
}

void ChatInternal::AppendHistoryTurn(
    const std::vector<ModelContent>& request_turns,
    const ModelContent& response_turn) {
  MutexLock lock(history_mutex_);
  history_.insert(history_.end(), request_turns.begin(), request_turns.end());
  history_.push_back(response_turn);
}

Future<GenerateContentResponse> ChatInternal::SendMessage(
    const std::vector<ModelContent>& content) {
  SafeFutureHandle<GenerateContentResponse> handle =
      future_impl_->SafeAlloc<GenerateContentResponse>(kChatFnSendMessage);

  if (!model_ || content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Chat message content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::vector<ModelContent> request_turns = NormalizeUserTurns(content);
  std::vector<ModelContent> full_request;
  {
    MutexLock lock(history_mutex_);
    full_request = history_;
  }
  full_request.insert(full_request.end(), request_turns.begin(),
                      request_turns.end());

  std::shared_ptr<ChatInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  Future<GenerateContentResponse> inner_future =
      model_->GenerateContent(full_request);
  inner_future.OnCompletion(
      [self, future_impl, handle,
       request_turns](const Future<GenerateContentResponse>& completed) {
        if (completed.error() != kErrorNone || completed.result() == nullptr) {
          future_impl->Complete(
              handle, completed.error(),
              completed.error_message() ? completed.error_message() : "");
          return;
        }
        const GenerateContentResponse& resp = *completed.result();
        if (!resp.candidates().empty()) {
          ModelContent model_turn = resp.candidates()[0].content;
          if (model_turn.role().empty()) {
            model_turn.set_role("model");
          }
          self->AppendHistoryTurn(request_turns, model_turn);
        }
        future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
      });

  return MakeFuture(future_impl_.get(), handle);
}

Future<GenerateContentResponse> ChatInternal::SendMessageLastResult() const {
  return static_cast<const Future<GenerateContentResponse>&>(
      future_impl_->LastResult(kChatFnSendMessage));
}

Future<void> ChatInternal::SendMessageStream(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk) {
  SafeFutureHandle<void> handle =
      future_impl_->SafeAlloc<void>(kChatFnSendMessageStream);

  if (!model_ || content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Chat message content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::vector<ModelContent> request_turns = NormalizeUserTurns(content);
  std::vector<ModelContent> full_request;
  {
    MutexLock lock(history_mutex_);
    full_request = history_;
  }
  full_request.insert(full_request.end(), request_turns.begin(),
                      request_turns.end());

  std::shared_ptr<ChatInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
  std::shared_ptr<StreamTurnAggregator> aggregator(new StreamTurnAggregator());

  Future<void> inner_future = model_->GenerateContentStream(
      full_request,
      [aggregator, on_chunk](const GenerateContentResponse& chunk) {
        aggregator->AddChunk(chunk);
        if (on_chunk) {
          on_chunk(chunk);
        }
      });

  inner_future.OnCompletion([self, future_impl, handle, request_turns,
                             aggregator](const Future<void>& completed) {
    if (completed.error() == kErrorNone &&
        !aggregator->accumulated_parts.empty()) {
      self->AppendHistoryTurn(request_turns, aggregator->BuildModelTurn());
    }
    future_impl->Complete(
        handle, completed.error(),
        completed.error_message() ? completed.error_message() : "");
  });

  return MakeFuture(future_impl_.get(), handle);
}

Future<void> ChatInternal::SendMessageStreamLastResult() const {
  return static_cast<const Future<void>&>(
      future_impl_->LastResult(kChatFnSendMessageStream));
}

}  // namespace internal

// --- Chat public implementation ---

Chat::Chat() : internal_(nullptr) {}

Chat::Chat(const std::shared_ptr<internal::ChatInternal>& internal)
    : internal_(internal) {}

Chat::Chat(const Chat& other) : internal_(other.internal_) {}

Chat& Chat::operator=(const Chat& other) {
  if (this != &other) {
    internal_ = other.internal_;
  }
  return *this;
}

Chat::~Chat() {}

std::vector<ModelContent> Chat::history() const {
  if (!internal_) return std::vector<ModelContent>();
  return internal_->history();
}

Future<GenerateContentResponse> Chat::SendMessage(const std::string& prompt) {
  return SendMessage(std::vector<ModelContent>(1, ModelContent::Text(prompt)));
}

Future<GenerateContentResponse> Chat::SendMessage(const ModelContent& content) {
  return SendMessage(std::vector<ModelContent>(1, content));
}

Future<GenerateContentResponse> Chat::SendMessage(
    const std::vector<ModelContent>& content) {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->SendMessage(content);
}

Future<GenerateContentResponse> Chat::SendMessageLastResult() const {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->SendMessageLastResult();
}

Future<void> Chat::SendMessageStream(
    const std::string& prompt, const GenerateContentStreamCallback& on_chunk) {
  return SendMessageStream(
      std::vector<ModelContent>(1, ModelContent::Text(prompt)), on_chunk);
}

Future<void> Chat::SendMessageStream(
    const ModelContent& content,
    const GenerateContentStreamCallback& on_chunk) {
  return SendMessageStream(std::vector<ModelContent>(1, content), on_chunk);
}

Future<void> Chat::SendMessageStream(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk) {
  if (!internal_) return Future<void>();
  return internal_->SendMessageStream(content, on_chunk);
}

Future<void> Chat::SendMessageStreamLastResult() const {
  if (!internal_) return Future<void>();
  return internal_->SendMessageStreamLastResult();
}

InferenceMode Chat::inference_mode() const {
  if (!internal_) return kInferenceModeOnlyInCloud;
  return internal_->inference_mode();
}

void Chat::set_inference_mode(InferenceMode mode) {
  if (internal_) {
    internal_->set_inference_mode(mode);
  }
}

bool Chat::IsOnDeviceAvailable() const {
  if (!internal_) return false;
  return internal_->IsOnDeviceAvailable();
}

void Chat::ClearHistory() {
  if (internal_) {
    internal_->ClearHistory();
  }
}

Future<GenerateContentResponse> Chat::CompactHistory() {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->CompactHistory();
}

}  // namespace ai
}  // namespace firebase
