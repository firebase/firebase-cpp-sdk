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

#ifndef FIREBASE_AI_SRC_COMMON_CHAT_INTERNAL_H_
#define FIREBASE_AI_SRC_COMMON_CHAT_INTERNAL_H_

#include <memory>
#include <string>
#include <vector>

#include "ai/src/common/generative_model_internal.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/ai/chat.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/model_content.h"
#include "firebase/future.h"

namespace firebase {
namespace ai {
namespace internal {

enum ChatFn {
  kChatFnSendMessage = 0,
  kChatFnSendMessageStream,
  kChatFnCompactHistory,
  kChatFnCount
};

class ChatInternal : public std::enable_shared_from_this<ChatInternal> {
 public:
  ChatInternal(const std::shared_ptr<GenerativeModelInternal>& model,
               const std::vector<ModelContent>& initial_history);
  ~ChatInternal();

  std::vector<ModelContent> history() const;
  void ClearHistory();
  Future<GenerateContentResponse> CompactHistory();

  Future<GenerateContentResponse> SendMessage(
      const std::vector<ModelContent>& content);
  Future<GenerateContentResponse> SendMessageLastResult() const;

  Future<void> SendMessageStream(const std::vector<ModelContent>& content,
                                 const GenerateContentStreamCallback& on_chunk);
  Future<void> SendMessageStreamLastResult() const;

  InferenceMode inference_mode() const {
    return model_ ? model_->inference_mode() : kInferenceModeOnlyInCloud;
  }
  void set_inference_mode(InferenceMode mode) {
    if (model_) model_->set_inference_mode(mode);
  }
  bool IsOnDeviceAvailable() const {
    return model_ ? model_->IsOnDeviceAvailable() : false;
  }

 private:
  void AppendHistoryTurn(const std::vector<ModelContent>& request_turns,
                         const ModelContent& response_turn);

  std::shared_ptr<GenerativeModelInternal> model_;
  mutable Mutex history_mutex_;
  std::vector<ModelContent> history_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_CHAT_INTERNAL_H_
