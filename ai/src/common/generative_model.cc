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

#include "firebase/ai/generative_model.h"

#include <thread>

#include "ai/src/common/chat_internal.h"
#include "ai/src/common/generative_model_internal.h"
#include "ai/src/common/http_client.h"
#include "ai/src/common/serialization.h"
#include "app/src/log.h"
#include "firebase/ai/chat.h"

namespace firebase {
namespace ai {
namespace internal {

GenerativeModelInternal::GenerativeModelInternal(
    ::firebase::App* app, const Backend& backend, const std::string& model_name,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction,
    const Optional<RequestOptions>& request_options,
    const Optional<HybridParams>& hybrid_params)
    : app_(app),
      backend_(backend),
      model_name_(model_name),
      generation_config_(generation_config),
      safety_settings_(safety_settings),
      tools_(tools),
      tool_config_(tool_config),
      system_instruction_(system_instruction),
      request_options_(request_options.value_or(RequestOptions())),
      inference_mode_(hybrid_params.has_value() ? hybrid_params->mode
                                                : kInferenceModeOnlyInCloud),
      future_impl_(new ReferenceCountedFutureImpl(kGenerativeModelFnCount)) {
  if (system_instruction_.has_value()) {
    system_instruction_.value().set_role("system");
  }
  if (hybrid_params.has_value() &&
      !hybrid_params->on_device_params.model_path.empty()) {
    litert_adapter_.reset(new LiteRtAdapter(hybrid_params->on_device_params));
  }
}

GenerativeModelInternal::~GenerativeModelInternal() {}

InferenceMode GenerativeModelInternal::inference_mode() const {
  MutexLock lock(mode_mutex_);
  return inference_mode_;
}

void GenerativeModelInternal::set_inference_mode(InferenceMode mode) {
  MutexLock lock(mode_mutex_);
  inference_mode_ = mode;
}

bool GenerativeModelInternal::IsOnDeviceAvailable() const {
  return litert_adapter_ && litert_adapter_->IsAvailable();
}

Future<void> GenerativeModelInternal::InitializeOnDeviceModel() {
  SafeFutureHandle<void> handle =
      future_impl_->SafeAlloc<void>(kGenerativeModelFnInitializeOnDeviceModel);
  if (!litert_adapter_) {
    future_impl_->Complete(
        handle, kErrorUnsupported,
        "No OnDeviceParams configured for this GenerativeModel.");
    return MakeFuture(future_impl_.get(), handle);
  }

  std::shared_ptr<LiteRtAdapter> adapter = litert_adapter_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
  std::thread([adapter, future_impl, handle]() {
    std::string err;
    if (!adapter->Initialize(&err)) {
      future_impl->Complete(handle, kErrorUnsupported, err.c_str());
      return;
    }
    future_impl->Complete(handle, kErrorNone, "");
  }).detach();

  return MakeFuture(future_impl_.get(), handle);
}

void GenerativeModelInternal::GenerateContentCloud(
    const std::vector<ModelContent>& content,
    SafeFutureHandle<GenerateContentResponse> handle,
    bool fallback_to_on_device_on_error) {
  if (!app_ || model_name_.empty()) {
    if (fallback_to_on_device_on_error && IsOnDeviceAvailable()) {
      std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
      litert_adapter_->GenerateContentAsync(
          content, generation_config_, system_instruction_,
          [future_impl, handle](Error err, const std::string& err_msg,
                                const GenerateContentResponse& resp) {
            if (err != kErrorNone) {
              future_impl->Complete(handle, err, err_msg.c_str());
              return;
            }
            future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
          });
      return;
    }
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Model name and Firebase App must not be empty.");
    return;
  }

  std::string url = AiHttpClient::ConstructModelUrl(app_, backend_, model_name_,
                                                    "generateContent");
  std::string body = BuildGenerateContentRequestJson(
      content, generation_config_, safety_settings_, tools_, tool_config_,
      system_instruction_, backend_.provider());

  std::shared_ptr<GenerativeModelInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
  BackendProvider provider = backend_.provider();

  AiHttpClient::SendUnaryJson(
      app_, request_options_, url, body,
      [self, content, future_impl, handle, provider,
       fallback_to_on_device_on_error](Error err, const std::string& err_msg,
                                       const std::string& response_body) {
        if (err != kErrorNone) {
          if (fallback_to_on_device_on_error && self->IsOnDeviceAvailable()) {
            LogWarning(
                "Cloud GenerateContent failed (%s); falling back to on-device "
                "LiteRT model.",
                err_msg.c_str());
            self->litert_adapter_->GenerateContentAsync(
                content, self->generation_config_, self->system_instruction_,
                [future_impl, handle](Error local_err,
                                      const std::string& local_msg,
                                      const GenerateContentResponse& resp) {
                  if (local_err != kErrorNone) {
                    future_impl->Complete(handle, local_err, local_msg.c_str());
                    return;
                  }
                  future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
                });
            return;
          }
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
        parsed.set_inference_source(kInferenceSourceInCloud);
        if (parsed.candidates().empty() &&
            parsed.prompt_feedback().has_value() &&
            parsed.prompt_feedback().value().block_reason !=
                kBlockReasonUnknown) {
          std::string block_msg =
              parsed.prompt_feedback().value().block_reason_message.empty()
                  ? "Prompt was blocked by safety settings."
                  : parsed.prompt_feedback().value().block_reason_message;
          future_impl->CompleteWithResult(handle, kErrorResponseBlocked,
                                          block_msg.c_str(), parsed);
          return;
        }
        future_impl->CompleteWithResult(handle, kErrorNone, "", parsed);
      });
}

Future<GenerateContentResponse> GenerativeModelInternal::GenerateContent(
    const std::vector<ModelContent>& content) {
  SafeFutureHandle<GenerateContentResponse> handle =
      future_impl_->SafeAlloc<GenerateContentResponse>(
          kGenerativeModelFnGenerateContent);

  if (content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Input content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  InferenceMode mode = inference_mode();
  std::shared_ptr<GenerativeModelInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  if (mode == kInferenceModeOnlyOnDevice) {
    if (!litert_adapter_) {
      future_impl_->Complete(
          handle, kErrorUnsupported,
          "ONLY_ON_DEVICE requested, but no OnDeviceParams were configured.");
      return MakeFuture(future_impl_.get(), handle);
    }
    litert_adapter_->GenerateContentAsync(
        content, generation_config_, system_instruction_,
        [future_impl, handle](Error err, const std::string& err_msg,
                              const GenerateContentResponse& resp) {
          if (err != kErrorNone) {
            future_impl->Complete(handle, err, err_msg.c_str());
            return;
          }
          future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
        });
    return MakeFuture(future_impl_.get(), handle);
  }

  if (mode == kInferenceModePreferOnDevice) {
    if (IsOnDeviceAvailable()) {
      litert_adapter_->GenerateContentAsync(
          content, generation_config_, system_instruction_,
          [self, content, future_impl, handle](
              Error err, const std::string& err_msg,
              const GenerateContentResponse& resp) {
            if (err == kErrorNone) {
              future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
              return;
            }
            LogWarning(
                "On-device LiteRT inference failed (%s); falling back to "
                "cloud Firebase AI.",
                err_msg.c_str());
            self->GenerateContentCloud(
                content, handle,
                /*fallback_to_on_device_on_error=*/false);
          });
      return MakeFuture(future_impl_.get(), handle);
    }
    LogWarning(
        "On-device LiteRT model is unavailable; falling back to cloud Firebase "
        "AI.");
    GenerateContentCloud(content, handle,
                         /*fallback_to_on_device_on_error=*/false);
    return MakeFuture(future_impl_.get(), handle);
  }

  // kInferenceModeOnlyInCloud or kInferenceModePreferInCloud
  GenerateContentCloud(
      content, handle,
      /*fallback_to_on_device_on_error=*/(mode == kInferenceModePreferInCloud));
  return MakeFuture(future_impl_.get(), handle);
}

Future<GenerateContentResponse>
GenerativeModelInternal::GenerateContentLastResult() const {
  return static_cast<const Future<GenerateContentResponse>&>(
      future_impl_->LastResult(kGenerativeModelFnGenerateContent));
}

void GenerativeModelInternal::GenerateContentStreamCloud(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk,
    SafeFutureHandle<void> handle, bool fallback_to_on_device_on_error) {
  if (!app_ || model_name_.empty()) {
    if (fallback_to_on_device_on_error && IsOnDeviceAvailable()) {
      std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
      litert_adapter_->GenerateContentStreamAsync(
          content, generation_config_, system_instruction_, on_chunk,
          [future_impl, handle](Error err, const std::string& err_msg,
                                const GenerateContentResponse& /*resp*/) {
            future_impl->Complete(handle, err, err_msg.c_str());
          });
      return;
    }
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Model name and Firebase App must not be empty.");
    return;
  }

  std::string url = AiHttpClient::ConstructModelUrl(
      app_, backend_, model_name_, "streamGenerateContent?alt=sse");
  std::string body = BuildGenerateContentRequestJson(
      content, generation_config_, safety_settings_, tools_, tool_config_,
      system_instruction_, backend_.provider());

  std::shared_ptr<GenerativeModelInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  AiHttpClient::SendStreamJson(
      app_, backend_, request_options_, url, body, on_chunk,
      [self, content, on_chunk, future_impl, handle,
       fallback_to_on_device_on_error](Error err, const std::string& err_msg,
                                       const std::string& /*response_body*/) {
        if (err != kErrorNone && fallback_to_on_device_on_error &&
            self->IsOnDeviceAvailable()) {
          LogWarning(
              "Cloud GenerateContentStream failed (%s); falling back to "
              "on-device LiteRT model.",
              err_msg.c_str());
          self->litert_adapter_->GenerateContentStreamAsync(
              content, self->generation_config_, self->system_instruction_,
              on_chunk,
              [future_impl, handle](Error local_err,
                                    const std::string& local_msg,
                                    const GenerateContentResponse& /*resp*/) {
                future_impl->Complete(handle, local_err, local_msg.c_str());
              });
          return;
        }
        future_impl->Complete(handle, err, err_msg.c_str());
      });
}

Future<void> GenerativeModelInternal::GenerateContentStream(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk) {
  SafeFutureHandle<void> handle =
      future_impl_->SafeAlloc<void>(kGenerativeModelFnGenerateContentStream);

  if (content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Input content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  InferenceMode mode = inference_mode();
  std::shared_ptr<GenerativeModelInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  if (mode == kInferenceModeOnlyOnDevice) {
    if (!litert_adapter_) {
      future_impl_->Complete(
          handle, kErrorUnsupported,
          "ONLY_ON_DEVICE requested, but no OnDeviceParams were configured.");
      return MakeFuture(future_impl_.get(), handle);
    }
    litert_adapter_->GenerateContentStreamAsync(
        content, generation_config_, system_instruction_, on_chunk,
        [future_impl, handle](Error err, const std::string& err_msg,
                              const GenerateContentResponse& /*resp*/) {
          future_impl->Complete(handle, err, err_msg.c_str());
        });
    return MakeFuture(future_impl_.get(), handle);
  }

  if (mode == kInferenceModePreferOnDevice) {
    if (IsOnDeviceAvailable()) {
      litert_adapter_->GenerateContentStreamAsync(
          content, generation_config_, system_instruction_, on_chunk,
          [self, content, on_chunk, future_impl, handle](
              Error err, const std::string& err_msg,
              const GenerateContentResponse& /*resp*/) {
            if (err == kErrorNone) {
              future_impl->Complete(handle, kErrorNone, "");
              return;
            }
            LogWarning(
                "On-device LiteRT streaming failed (%s); falling back to "
                "cloud Firebase AI.",
                err_msg.c_str());
            self->GenerateContentStreamCloud(
                content, on_chunk, handle,
                /*fallback_to_on_device_on_error=*/false);
          });
      return MakeFuture(future_impl_.get(), handle);
    }
    GenerateContentStreamCloud(content, on_chunk, handle,
                               /*fallback_to_on_device_on_error=*/false);
    return MakeFuture(future_impl_.get(), handle);
  }

  GenerateContentStreamCloud(
      content, on_chunk, handle,
      /*fallback_to_on_device_on_error=*/(mode == kInferenceModePreferInCloud));
  return MakeFuture(future_impl_.get(), handle);
}

Future<void> GenerativeModelInternal::GenerateContentStreamLastResult() const {
  return static_cast<const Future<void>&>(
      future_impl_->LastResult(kGenerativeModelFnGenerateContentStream));
}

void GenerativeModelInternal::CountTokensCloud(
    const std::vector<ModelContent>& content,
    SafeFutureHandle<CountTokensResponse> handle,
    bool fallback_to_on_device_on_error) {
  if (!app_ || model_name_.empty()) {
    if (fallback_to_on_device_on_error && IsOnDeviceAvailable()) {
      std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;
      litert_adapter_->CountTokensAsync(
          content, [future_impl, handle](Error err, const std::string& err_msg,
                                         const CountTokensResponse& resp) {
            if (err != kErrorNone) {
              future_impl->Complete(handle, err, err_msg.c_str());
              return;
            }
            future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
          });
      return;
    }
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Model name and Firebase App must not be empty.");
    return;
  }

  std::string url = AiHttpClient::ConstructModelUrl(app_, backend_, model_name_,
                                                    "countTokens");
  std::string body = BuildCountTokensRequestJson(
      model_name_, content, generation_config_, safety_settings_, tools_,
      tool_config_, system_instruction_, backend_.provider());

  std::shared_ptr<GenerativeModelInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  AiHttpClient::SendUnaryJson(
      app_, request_options_, url, body,
      [self, content, future_impl, handle, fallback_to_on_device_on_error](
          Error err, const std::string& err_msg,
          const std::string& response_body) {
        if (err != kErrorNone) {
          if (fallback_to_on_device_on_error && self->IsOnDeviceAvailable()) {
            self->litert_adapter_->CountTokensAsync(
                content, [future_impl, handle](
                             Error local_err, const std::string& local_msg,
                             const CountTokensResponse& resp) {
                  if (local_err != kErrorNone) {
                    future_impl->Complete(handle, local_err, local_msg.c_str());
                    return;
                  }
                  future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
                });
            return;
          }
          future_impl->Complete(handle, err, err_msg.c_str());
          return;
        }
        CountTokensResponse parsed;
        std::string parse_err;
        if (!ParseCountTokensResponseJson(response_body, &parsed, &parse_err)) {
          future_impl->Complete(handle, kErrorSerializationFailed,
                                parse_err.c_str());
          return;
        }
        future_impl->CompleteWithResult(handle, kErrorNone, "", parsed);
      });
}

Future<CountTokensResponse> GenerativeModelInternal::CountTokens(
    const std::vector<ModelContent>& content) {
  SafeFutureHandle<CountTokensResponse> handle =
      future_impl_->SafeAlloc<CountTokensResponse>(
          kGenerativeModelFnCountTokens);

  if (content.empty()) {
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           "Input content must not be empty.");
    return MakeFuture(future_impl_.get(), handle);
  }

  InferenceMode mode = inference_mode();
  std::shared_ptr<GenerativeModelInternal> self = shared_from_this();
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl = future_impl_;

  if (mode == kInferenceModeOnlyOnDevice) {
    if (!litert_adapter_) {
      future_impl_->Complete(
          handle, kErrorUnsupported,
          "ONLY_ON_DEVICE requested, but no OnDeviceParams were configured.");
      return MakeFuture(future_impl_.get(), handle);
    }
    litert_adapter_->CountTokensAsync(
        content, [future_impl, handle](Error err, const std::string& err_msg,
                                       const CountTokensResponse& resp) {
          if (err != kErrorNone) {
            future_impl->Complete(handle, err, err_msg.c_str());
            return;
          }
          future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
        });
    return MakeFuture(future_impl_.get(), handle);
  }

  if (mode == kInferenceModePreferOnDevice && IsOnDeviceAvailable()) {
    litert_adapter_->CountTokensAsync(
        content, [self, content, future_impl, handle](
                     Error err, const std::string& /*err_msg*/,
                     const CountTokensResponse& resp) {
          if (err == kErrorNone) {
            future_impl->CompleteWithResult(handle, kErrorNone, "", resp);
            return;
          }
          self->CountTokensCloud(content, handle,
                                 /*fallback_to_on_device_on_error=*/false);
        });
    return MakeFuture(future_impl_.get(), handle);
  }

  CountTokensCloud(
      content, handle,
      /*fallback_to_on_device_on_error=*/(mode == kInferenceModePreferInCloud));
  return MakeFuture(future_impl_.get(), handle);
}

Future<CountTokensResponse> GenerativeModelInternal::CountTokensLastResult()
    const {
  return static_cast<const Future<CountTokensResponse>&>(
      future_impl_->LastResult(kGenerativeModelFnCountTokens));
}

}  // namespace internal

// --- GenerativeModel public implementation ---

GenerativeModel::GenerativeModel() : internal_(nullptr) {}

GenerativeModel::GenerativeModel(
    const std::shared_ptr<internal::GenerativeModelInternal>& internal)
    : internal_(internal) {}

GenerativeModel::GenerativeModel(const GenerativeModel& other)
    : internal_(other.internal_) {}

GenerativeModel& GenerativeModel::operator=(const GenerativeModel& other) {
  if (this != &other) {
    internal_ = other.internal_;
  }
  return *this;
}

GenerativeModel::~GenerativeModel() {}

Future<GenerateContentResponse> GenerativeModel::GenerateContent(
    const std::string& prompt) {
  return GenerateContent(
      std::vector<ModelContent>(1, ModelContent::Text(prompt)));
}

Future<GenerateContentResponse> GenerativeModel::GenerateContent(
    const ModelContent& content) {
  return GenerateContent(std::vector<ModelContent>(1, content));
}

Future<GenerateContentResponse> GenerativeModel::GenerateContent(
    const std::vector<ModelContent>& content) {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->GenerateContent(content);
}

Future<GenerateContentResponse> GenerativeModel::GenerateContentLastResult()
    const {
  if (!internal_) return Future<GenerateContentResponse>();
  return internal_->GenerateContentLastResult();
}

Future<void> GenerativeModel::GenerateContentStream(
    const std::string& prompt, const GenerateContentStreamCallback& on_chunk) {
  return GenerateContentStream(
      std::vector<ModelContent>(1, ModelContent::Text(prompt)), on_chunk);
}

Future<void> GenerativeModel::GenerateContentStream(
    const ModelContent& content,
    const GenerateContentStreamCallback& on_chunk) {
  return GenerateContentStream(std::vector<ModelContent>(1, content), on_chunk);
}

Future<void> GenerativeModel::GenerateContentStream(
    const std::vector<ModelContent>& content,
    const GenerateContentStreamCallback& on_chunk) {
  if (!internal_) return Future<void>();
  return internal_->GenerateContentStream(content, on_chunk);
}

Future<void> GenerativeModel::GenerateContentStreamLastResult() const {
  if (!internal_) return Future<void>();
  return internal_->GenerateContentStreamLastResult();
}

Future<CountTokensResponse> GenerativeModel::CountTokens(
    const std::string& prompt) {
  return CountTokens(std::vector<ModelContent>(1, ModelContent::Text(prompt)));
}

Future<CountTokensResponse> GenerativeModel::CountTokens(
    const ModelContent& content) {
  return CountTokens(std::vector<ModelContent>(1, content));
}

Future<CountTokensResponse> GenerativeModel::CountTokens(
    const std::vector<ModelContent>& content) {
  if (!internal_) return Future<CountTokensResponse>();
  return internal_->CountTokens(content);
}

Future<CountTokensResponse> GenerativeModel::CountTokensLastResult() const {
  if (!internal_) return Future<CountTokensResponse>();
  return internal_->CountTokensLastResult();
}

Chat GenerativeModel::StartChat(
    const std::vector<ModelContent>& history) const {
  if (!internal_) return Chat();
  return Chat(std::shared_ptr<internal::ChatInternal>(
      new internal::ChatInternal(internal_, history)));
}

InferenceMode GenerativeModel::inference_mode() const {
  if (!internal_) return kInferenceModeOnlyInCloud;
  return internal_->inference_mode();
}

void GenerativeModel::set_inference_mode(InferenceMode mode) {
  if (internal_) {
    internal_->set_inference_mode(mode);
  }
}

bool GenerativeModel::IsOnDeviceAvailable() const {
  if (!internal_) return false;
  return internal_->IsOnDeviceAvailable();
}

Future<void> GenerativeModel::InitializeOnDeviceModel() {
  if (!internal_) return Future<void>();
  return internal_->InitializeOnDeviceModel();
}

}  // namespace ai
}  // namespace firebase
