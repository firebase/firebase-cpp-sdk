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

#ifndef FIREBASE_AI_SRC_COMMON_HTTP_CLIENT_H_
#define FIREBASE_AI_SRC_COMMON_HTTP_CLIENT_H_

#include <functional>
#include <string>

#include "ai/src/common/http_sender.h"
#include "firebase/ai/generate_content_response.h"
#include "firebase/ai/generative_model.h"
#include "firebase/ai/types.h"
#include "firebase/app.h"

namespace firebase {
namespace ai {
namespace internal {

/// @brief Callback invoked when a Firebase AI HTTP call finishes.
typedef std::function<void(Error error, const std::string& error_message,
                           const std::string& response_body)>
    AiHttpCallback;

/// @brief Stateful SSE (`text/event-stream`) line-buffered parser that extracts
/// `data:` JSON payloads and parses them into `GenerateContentResponse`
/// chunks.
class SseStreamParser {
 public:
  explicit SseStreamParser(BackendProvider provider,
                           const GenerateContentStreamCallback& on_chunk)
      : provider_(provider), on_chunk_(on_chunk) {}

  /// @brief Feeds raw bytes received from the HTTP stream into the SSE line
  /// buffer, invoking `on_chunk_` for every complete `data:` JSON line.
  bool Feed(const char* data, size_t length);

  /// @brief Flushes any trailing line remaining in the buffer when the stream
  /// closes.
  void Flush();

 private:
  void ProcessLine(const std::string& line);

  BackendProvider provider_;
  GenerateContentStreamCallback on_chunk_;
  std::string buffer_;
};

/// @brief Common C++ HTTP client that constructs Firebase AI endpoint URLs,
/// attaches Auth and App Check tokens, and dispatches requests through the slim
/// platform `HttpSender`.
class AiHttpClient {
 public:
  /// @brief Constructs the full endpoint URL for a model task
  /// (`generateContent`, `streamGenerateContent?alt=sse`, `countTokens`).
  static std::string ConstructModelUrl(const ::firebase::App* app,
                                       const Backend& backend,
                                       const std::string& model_name,
                                       const std::string& task);

  /// @brief Constructs the full endpoint URL for a prompt template task
  /// (`templateGenerateContent`, `templateStreamGenerateContent?alt=sse`).
  static std::string ConstructTemplateUrl(const ::firebase::App* app,
                                          const Backend& backend,
                                          const std::string& template_id,
                                          const std::string& task);

  /// @brief Resolves Firebase headers (API key, App ID, Auth token, App Check
  /// token) and sends a unary JSON POST request.
  static void SendUnaryJson(::firebase::App* app, const RequestOptions& options,
                            const std::string& url,
                            const std::string& json_body,
                            const AiHttpCallback& callback);

  /// @brief Resolves Firebase headers (API key, App ID, Auth token, App Check
  /// token) and sends a streaming SSE JSON POST request.
  static void SendStreamJson(::firebase::App* app, const Backend& backend,
                             const RequestOptions& options,
                             const std::string& url,
                             const std::string& json_body,
                             const GenerateContentStreamCallback& on_chunk,
                             const AiHttpCallback& on_complete);
};

}  // namespace internal
}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_COMMON_HTTP_CLIENT_H_
