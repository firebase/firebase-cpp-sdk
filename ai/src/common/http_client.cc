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

#include "ai/src/common/http_client.h"

#include <cstring>
#include <memory>
#include <sstream>

#include "ai/src/common/serialization.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "firebase/future.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

const char kBaseUrlPrefix[] =
    "https://firebasevertexai.googleapis.com/v1beta/projects/";
const char kStreamPrefix[] = "data:";

std::string NormalizeModelName(const std::string& model_name) {
  const char kPrefix[] = "models/";
  if (model_name.compare(0, sizeof(kPrefix) - 1, kPrefix) == 0) {
    return model_name;
  }
  return std::string(kPrefix) + model_name;
}

std::string NormalizeTemplateId(const std::string& template_id) {
  const char kPrefix[] = "templates/";
  if (template_id.compare(0, sizeof(kPrefix) - 1, kPrefix) == 0) {
    return template_id;
  }
  return std::string(kPrefix) + template_id;
}

std::string GetAuthToken(::firebase::App* app) {
  if (!app || !app->function_registry()) return "";
  std::string auth_token;
  app->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentToken, app, nullptr, &auth_token);
  return auth_token;
}

void PopulateBaseHeaders(::firebase::App* app, HttpRequest* request) {
  request->headers["Content-Type"] = "application/json";
  if (app) {
    const char* api_key = app->options().api_key();
    if (api_key && api_key[0] != '\0') {
      request->headers["x-goog-api-key"] = api_key;
    }
    if (app->IsDataCollectionDefaultEnabled()) {
      const char* app_id = app->options().app_id();
      if (app_id && app_id[0] != '\0') {
        request->headers["X-Firebase-AppId"] = app_id;
      }
    }
    std::string auth_token = GetAuthToken(app);
    if (!auth_token.empty()) {
      request->headers["Authorization"] = "Firebase " + auth_token;
    }
  }
  std::string version_str = FIREBASE_VERSION_NUMBER_STRING;
  request->headers["x-goog-api-client"] =
      "gl-cpp/" + version_str + " fire/" + version_str;
}

void PrepareRequestWithTokens(
    ::firebase::App* app, const RequestOptions& options, const std::string& url,
    const std::string& json_body,
    const std::function<void(const HttpRequest&)>& on_ready) {
  HttpRequest req;
  req.url = url;
  req.method = "POST";
  req.body = json_body;
  req.timeout_ms = options.timeout_ms > 0 ? options.timeout_ms
                                          : RequestOptions::kDefaultTimeoutMs;
  PopulateBaseHeaders(app, &req);

  if (app && app->function_registry()) {
    Future<std::string> app_check_future;
    ::firebase::internal::FunctionId fn_id =
        options.limited_use_app_check_token
            ? ::firebase::internal::FnAppCheckGetLimitedUseTokenAsync
            : ::firebase::internal::FnAppCheckGetTokenAsync;
    bool called = app->function_registry()->CallFunction(fn_id, app, nullptr,
                                                         &app_check_future);
    if (called && app_check_future.status() != kFutureStatusInvalid) {
      app_check_future.OnCompletion(
          [req, on_ready](const Future<std::string>& token_future) mutable {
            if (token_future.error() == 0 && token_future.result() != nullptr &&
                !token_future.result()->empty()) {
              req.headers["X-Firebase-AppCheck"] = *token_future.result();
            }
            on_ready(req);
          });
      return;
    }
  }
  on_ready(req);
}

void MapHttpCompletion(int status_code, const std::string& response_body,
                       const std::string& transport_error,
                       const AiHttpCallback& callback) {
  if (!transport_error.empty() || status_code == 0) {
    Error err = kErrorNetworkFailed;
    if (status_code == 408 ||
        transport_error.find("timeout") != std::string::npos ||
        transport_error.find("timed out") != std::string::npos ||
        transport_error.find("Timed out") != std::string::npos) {
      err = kErrorTimeout;
    }
    std::string msg = transport_error.empty()
                          ? "Network request failed with no HTTP status."
                          : transport_error;
    callback(err, msg, response_body);
    return;
  }

  if (status_code < 200 || status_code >= 300) {
    std::string parsed_error = ParseHttpErrorJson(status_code, response_body);
    callback(kErrorHttpError, parsed_error, response_body);
    return;
  }

  callback(kErrorNone, "", response_body);
}

}  // namespace

bool SseStreamParser::Feed(const char* data, size_t length) {
  if (!data || length == 0) return true;
  buffer_.append(data, length);

  size_t pos = 0;
  while (true) {
    size_t newline_pos = buffer_.find('\n', pos);
    if (newline_pos == std::string::npos) {
      break;
    }
    std::string line = buffer_.substr(pos, newline_pos - pos);
    if (!line.empty() && line[line.size() - 1] == '\r') {
      line.erase(line.size() - 1);
    }
    ProcessLine(line);
    pos = newline_pos + 1;
  }

  if (pos > 0) {
    buffer_.erase(0, pos);
  }
  return true;
}

void SseStreamParser::Flush() {
  if (!buffer_.empty()) {
    std::string line = buffer_;
    buffer_.clear();
    if (!line.empty() && line[line.size() - 1] == '\r') {
      line.erase(line.size() - 1);
    }
    ProcessLine(line);
  }
}

void SseStreamParser::ProcessLine(const std::string& line) {
  if (line.compare(0, sizeof(kStreamPrefix) - 1, kStreamPrefix) != 0) {
    return;
  }
  std::string json_str = line.substr(sizeof(kStreamPrefix) - 1);
  // Trim leading/trailing whitespace.
  size_t first = json_str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return;
  size_t last = json_str.find_last_not_of(" \t\r\n");
  json_str = json_str.substr(first, last - first + 1);
  if (json_str.empty() || json_str == "[DONE]") return;

  GenerateContentResponse chunk_response;
  std::string error_msg;
  if (ParseGenerateContentResponseJson(json_str, provider_, &chunk_response,
                                       &error_msg)) {
    if (on_chunk_) {
      on_chunk_(chunk_response);
    }
  } else {
    LogWarning("FirebaseAI: Failed to parse SSE stream JSON chunk: %s",
               error_msg.c_str());
  }
}

std::string AiHttpClient::ConstructModelUrl(const ::firebase::App* app,
                                            const Backend& backend,
                                            const std::string& model_name,
                                            const std::string& task) {
  std::string project_id =
      (app && app->options().project_id()) ? app->options().project_id() : "";
  std::string normalized_model = NormalizeModelName(model_name);
  std::ostringstream oss;
  oss << kBaseUrlPrefix << project_id;
  if (backend.provider() == kBackendProviderGoogleAI) {
    oss << "/" << normalized_model << ":" << task;
  } else {
    std::string loc =
        backend.location().empty() ? "global" : backend.location();
    oss << "/locations/" << loc << "/publishers/google/" << normalized_model
        << ":" << task;
  }
  return oss.str();
}

std::string AiHttpClient::ConstructTemplateUrl(const ::firebase::App* app,
                                               const Backend& backend,
                                               const std::string& template_id,
                                               const std::string& task) {
  std::string project_id =
      (app && app->options().project_id()) ? app->options().project_id() : "";
  std::string normalized_template = NormalizeTemplateId(template_id);
  std::ostringstream oss;
  oss << kBaseUrlPrefix << project_id;
  if (backend.provider() == kBackendProviderGoogleAI) {
    oss << "/" << normalized_template << ":" << task;
  } else {
    std::string loc =
        backend.location().empty() ? "global" : backend.location();
    oss << "/locations/" << loc << "/" << normalized_template << ":" << task;
  }
  return oss.str();
}

void AiHttpClient::SendUnaryJson(::firebase::App* app,
                                 const RequestOptions& options,
                                 const std::string& url,
                                 const std::string& json_body,
                                 const AiHttpCallback& callback) {
  PrepareRequestWithTokens(
      app, options, url, json_body, [app, callback](const HttpRequest& req) {
        HttpSender::SendUnary(
            app, req,
            [callback](int status_code, const std::string& response_body,
                       const std::string& transport_error) {
              MapHttpCompletion(status_code, response_body, transport_error,
                                callback);
            });
      });
}

void AiHttpClient::SendStreamJson(::firebase::App* app, const Backend& backend,
                                  const RequestOptions& options,
                                  const std::string& url,
                                  const std::string& json_body,
                                  const GenerateContentStreamCallback& on_chunk,
                                  const AiHttpCallback& on_complete) {
  BackendProvider provider = backend.provider();
  PrepareRequestWithTokens(
      app, options, url, json_body,
      [app, provider, on_chunk, on_complete](const HttpRequest& req) {
        std::shared_ptr<SseStreamParser> parser(
            new SseStreamParser(provider, on_chunk));
        HttpSender::SendStream(
            app, req,
            [parser](const char* data, size_t length) -> bool {
              return parser->Feed(data, length);
            },
            [parser, on_complete](int status_code,
                                  const std::string& response_body,
                                  const std::string& transport_error) {
              if (status_code >= 200 && status_code < 300 &&
                  transport_error.empty()) {
                parser->Flush();
              }
              MapHttpCompletion(status_code, response_body, transport_error,
                                on_complete);
            });
      });
}

}  // namespace internal
}  // namespace ai
}  // namespace firebase
