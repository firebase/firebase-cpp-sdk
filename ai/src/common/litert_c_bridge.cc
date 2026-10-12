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

#include "ai/src/common/litert_c_bridge.h"

#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ai/src/common/litert_adapter.h"
#include "app/src/variant_util.h"
#include "firebase/variant.h"

namespace firebase {
namespace ai {
namespace internal {
namespace {

struct AdapterHolder {
  std::shared_ptr<LiteRtAdapter> adapter;
};

char* DuplicateCString(const std::string& str) {
  char* copy = static_cast<char*>(std::malloc(str.size() + 1));
  if (copy) {
    std::memcpy(copy, str.c_str(), str.size() + 1);
  }
  return copy;
}

ModelContent ParseModelContentVariant(const Variant& v) {
  if (!v.is_map()) return ModelContent::Text("");
  std::string role = "user";
  auto role_it = v.map().find(Variant("role"));
  if (role_it != v.map().end() && role_it->second.is_string()) {
    role = role_it->second.string_value();
  }
  std::vector<Part> parts;
  auto parts_it = v.map().find(Variant("parts"));
  if (parts_it != v.map().end() && parts_it->second.is_vector()) {
    for (const auto& p : parts_it->second.vector()) {
      if (!p.is_map()) continue;
      auto text_it = p.map().find(Variant("text"));
      if (text_it != p.map().end() && text_it->second.is_string()) {
        parts.push_back(Part(TextPart(text_it->second.string_value())));
      }
    }
  }
  return ModelContent(role, parts);
}

void ParseRequestJson(const char* request_json,
                      std::vector<ModelContent>* out_contents,
                      Optional<GenerationConfig>* out_gen_config,
                      Optional<ModelContent>* out_sys_instruction) {
  if (!request_json || request_json[0] == '\0') return;
  Variant root = util::JsonToVariant(request_json);
  if (!root.is_map()) return;

  // Support CountTokens `generateContentRequest` wrapper if present.
  const Variant* target = &root;
  auto wrap_it = root.map().find(Variant("generateContentRequest"));
  if (wrap_it != root.map().end() && wrap_it->second.is_map()) {
    target = &wrap_it->second;
  }

  auto contents_it = target->map().find(Variant("contents"));
  if (contents_it != target->map().end() && contents_it->second.is_vector()) {
    for (const auto& c : contents_it->second.vector()) {
      out_contents->push_back(ParseModelContentVariant(c));
    }
  }

  auto sys_it = target->map().find(Variant("systemInstruction"));
  if (sys_it != target->map().end() && sys_it->second.is_map()) {
    *out_sys_instruction = ParseModelContentVariant(sys_it->second);
  }

  auto gen_it = target->map().find(Variant("generationConfig"));
  if (gen_it != target->map().end() && gen_it->second.is_map()) {
    GenerationConfig cfg;
    const auto& gm = gen_it->second.map();
    auto temp_it = gm.find(Variant("temperature"));
    if (temp_it != gm.end() && temp_it->second.is_numeric()) {
      cfg.temperature =
          static_cast<float>(temp_it->second.AsDouble().double_value());
    }
    auto topk_it = gm.find(Variant("topK"));
    if (topk_it != gm.end() && topk_it->second.is_numeric()) {
      cfg.top_k = static_cast<int>(topk_it->second.AsInt64().int64_value());
    }
    auto topp_it = gm.find(Variant("topP"));
    if (topp_it != gm.end() && topp_it->second.is_numeric()) {
      cfg.top_p = static_cast<float>(topp_it->second.AsDouble().double_value());
    }
    auto max_it = gm.find(Variant("maxOutputTokens"));
    if (max_it != gm.end() && max_it->second.is_numeric()) {
      cfg.max_output_tokens =
          static_cast<int>(max_it->second.AsInt64().int64_value());
    }
    *out_gen_config = cfg;
  }
}

std::string ResponseToGeminiJson(const GenerateContentResponse& resp) {
  Variant root = Variant::EmptyMap();
  Variant candidates_arr = Variant::EmptyVector();

  for (const auto& cand : resp.candidates()) {
    Variant cand_map = Variant::EmptyMap();
    Variant content_map = Variant::EmptyMap();
    content_map.map()[Variant("role")] =
        Variant(cand.content.role().empty() ? "model" : cand.content.role());

    Variant parts_arr = Variant::EmptyVector();
    for (const auto& part : cand.content.parts()) {
      if (part.is_text()) {
        Variant p_map = Variant::EmptyMap();
        p_map.map()[Variant("text")] = Variant(part.text_part().text);
        if (part.is_thought()) {
          p_map.map()[Variant("thought")] = Variant(true);
        }
        parts_arr.vector().push_back(p_map);
      }
    }
    content_map.map()[Variant("parts")] = parts_arr;
    cand_map.map()[Variant("content")] = content_map;
    cand_map.map()[Variant("finishReason")] = Variant("STOP");
    candidates_arr.vector().push_back(cand_map);
  }
  root.map()[Variant("candidates")] = candidates_arr;

  if (resp.usage_metadata().has_value()) {
    Variant usage_map = Variant::EmptyMap();
    usage_map.map()[Variant("promptTokenCount")] =
        Variant(resp.usage_metadata()->prompt_token_count);
    usage_map.map()[Variant("candidatesTokenCount")] =
        Variant(resp.usage_metadata()->candidates_token_count);
    usage_map.map()[Variant("totalTokenCount")] =
        Variant(resp.usage_metadata()->total_token_count);
    root.map()[Variant("usageMetadata")] = usage_map;
  }

  root.map()[Variant("inferenceSource")] = Variant("ON_DEVICE");
  return util::VariantToJson(root);
}

}  // namespace
}  // namespace internal
}  // namespace ai
}  // namespace firebase

extern "C" {

FirebaseAiLiteRtHandle firebase_ai_litert_create(
    const char* model_path, const char* runtime_library_path,
    const char* cache_dir, int32_t accelerator, int32_t max_num_tokens,
    int32_t num_threads, float temperature, int32_t top_k, float top_p) {
  firebase::ai::OnDeviceParams params;
  if (model_path) params.model_path = model_path;
  if (runtime_library_path) params.runtime_library_path = runtime_library_path;
  if (cache_dir) params.cache_dir = cache_dir;
  params.accelerator =
      static_cast<firebase::ai::LiteRtAccelerator>(accelerator);
  if (max_num_tokens > 0) params.max_num_tokens = max_num_tokens;
  if (num_threads > 0) params.num_threads = num_threads;
  params.temperature = temperature;
  if (top_k > 0) params.top_k = top_k;
  params.top_p = top_p;

  firebase::ai::internal::AdapterHolder* holder =
      new firebase::ai::internal::AdapterHolder();
  holder->adapter.reset(new firebase::ai::internal::LiteRtAdapter(params));
  return reinterpret_cast<FirebaseAiLiteRtHandle>(holder);
}

void firebase_ai_litert_destroy(FirebaseAiLiteRtHandle handle) {
  if (!handle) return;
  firebase::ai::internal::AdapterHolder* holder =
      reinterpret_cast<firebase::ai::internal::AdapterHolder*>(handle);
  delete holder;
}

int32_t firebase_ai_litert_is_available(FirebaseAiLiteRtHandle handle) {
  if (!handle) return 0;
  firebase::ai::internal::AdapterHolder* holder =
      reinterpret_cast<firebase::ai::internal::AdapterHolder*>(handle);
  return holder->adapter->IsAvailable() ? 1 : 0;
}

int32_t firebase_ai_litert_initialize(FirebaseAiLiteRtHandle handle,
                                      char** out_error) {
  if (out_error) *out_error = nullptr;
  if (!handle) {
    if (out_error) {
      *out_error =
          firebase::ai::internal::DuplicateCString("Invalid LiteRT handle.");
    }
    return static_cast<int32_t>(firebase::ai::kErrorInvalidArgument);
  }
  firebase::ai::internal::AdapterHolder* holder =
      reinterpret_cast<firebase::ai::internal::AdapterHolder*>(handle);
  std::string err;
  if (!holder->adapter->Initialize(&err)) {
    if (out_error) {
      *out_error = firebase::ai::internal::DuplicateCString(err);
    }
    return static_cast<int32_t>(firebase::ai::kErrorUnsupported);
  }
  return static_cast<int32_t>(firebase::ai::kErrorNone);
}

int32_t firebase_ai_litert_generate_content(FirebaseAiLiteRtHandle handle,
                                            const char* request_json,
                                            char** out_response_json,
                                            char** out_error) {
  if (out_response_json) *out_response_json = nullptr;
  if (out_error) *out_error = nullptr;
  if (!handle) {
    if (out_error) {
      *out_error =
          firebase::ai::internal::DuplicateCString("Invalid LiteRT handle.");
    }
    return static_cast<int32_t>(firebase::ai::kErrorInvalidArgument);
  }

  firebase::ai::internal::AdapterHolder* holder =
      reinterpret_cast<firebase::ai::internal::AdapterHolder*>(handle);

  std::vector<firebase::ai::ModelContent> contents;
  firebase::ai::Optional<firebase::ai::GenerationConfig> gen_config;
  firebase::ai::Optional<firebase::ai::ModelContent> sys_instruction;
  firebase::ai::internal::ParseRequestJson(request_json, &contents, &gen_config,
                                           &sys_instruction);
  if (contents.empty()) {
    if (out_error) {
      *out_error = firebase::ai::internal::DuplicateCString(
          "Request contents must not be empty.");
    }
    return static_cast<int32_t>(firebase::ai::kErrorInvalidArgument);
  }

  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  firebase::ai::Error result_err = firebase::ai::kErrorNone;
  std::string result_err_msg;
  firebase::ai::GenerateContentResponse result_resp;

  holder->adapter->GenerateContentAsync(
      contents, gen_config, sys_instruction,
      [&](firebase::ai::Error err, const std::string& err_msg,
          const firebase::ai::GenerateContentResponse& resp) {
        std::lock_guard<std::mutex> lk(mu);
        result_err = err;
        result_err_msg = err_msg;
        result_resp = resp;
        done = true;
        cv.notify_all();
      });

  std::unique_lock<std::mutex> lk(mu);
  cv.wait(lk, [&]() { return done; });

  if (result_err != firebase::ai::kErrorNone) {
    if (out_error) {
      *out_error = firebase::ai::internal::DuplicateCString(result_err_msg);
    }
    return static_cast<int32_t>(result_err);
  }

  if (out_response_json) {
    *out_response_json = firebase::ai::internal::DuplicateCString(
        firebase::ai::internal::ResponseToGeminiJson(result_resp));
  }
  return static_cast<int32_t>(firebase::ai::kErrorNone);
}

int32_t firebase_ai_litert_generate_content_stream(
    FirebaseAiLiteRtHandle handle, const char* request_json,
    FirebaseAiLiteRtStreamChunkCallback chunk_callback, void* user_data,
    char** out_error) {
  if (out_error) *out_error = nullptr;
  if (!handle) {
    if (out_error) {
      *out_error =
          firebase::ai::internal::DuplicateCString("Invalid LiteRT handle.");
    }
    return static_cast<int32_t>(firebase::ai::kErrorInvalidArgument);
  }

  firebase::ai::internal::AdapterHolder* holder =
      reinterpret_cast<firebase::ai::internal::AdapterHolder*>(handle);

  std::vector<firebase::ai::ModelContent> contents;
  firebase::ai::Optional<firebase::ai::GenerationConfig> gen_config;
  firebase::ai::Optional<firebase::ai::ModelContent> sys_instruction;
  firebase::ai::internal::ParseRequestJson(request_json, &contents, &gen_config,
                                           &sys_instruction);
  if (contents.empty()) {
    if (out_error) {
      *out_error = firebase::ai::internal::DuplicateCString(
          "Request contents must not be empty.");
    }
    return static_cast<int32_t>(firebase::ai::kErrorInvalidArgument);
  }

  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  firebase::ai::Error result_err = firebase::ai::kErrorNone;
  std::string result_err_msg;

  holder->adapter->GenerateContentStreamAsync(
      contents, gen_config, sys_instruction,
      [chunk_callback,
       user_data](const firebase::ai::GenerateContentResponse& chunk) {
        if (chunk_callback) {
          std::string chunk_json =
              firebase::ai::internal::ResponseToGeminiJson(chunk);
          chunk_callback(chunk_json.c_str(), user_data);
        }
      },
      [&](firebase::ai::Error err, const std::string& err_msg,
          const firebase::ai::GenerateContentResponse& /*resp*/) {
        std::lock_guard<std::mutex> lk(mu);
        result_err = err;
        result_err_msg = err_msg;
        done = true;
        cv.notify_all();
      });

  std::unique_lock<std::mutex> lk(mu);
  cv.wait(lk, [&]() { return done; });

  if (result_err != firebase::ai::kErrorNone) {
    if (out_error) {
      *out_error = firebase::ai::internal::DuplicateCString(result_err_msg);
    }
    return static_cast<int32_t>(result_err);
  }
  return static_cast<int32_t>(firebase::ai::kErrorNone);
}

int32_t firebase_ai_litert_count_tokens(FirebaseAiLiteRtHandle handle,
                                        const char* request_json,
                                        int32_t* out_total_tokens,
                                        char** out_error) {
  if (out_total_tokens) *out_total_tokens = 0;
  if (out_error) *out_error = nullptr;
  if (!handle) {
    if (out_error) {
      *out_error =
          firebase::ai::internal::DuplicateCString("Invalid LiteRT handle.");
    }
    return static_cast<int32_t>(firebase::ai::kErrorInvalidArgument);
  }

  firebase::ai::internal::AdapterHolder* holder =
      reinterpret_cast<firebase::ai::internal::AdapterHolder*>(handle);

  std::vector<firebase::ai::ModelContent> contents;
  firebase::ai::Optional<firebase::ai::GenerationConfig> gen_config;
  firebase::ai::Optional<firebase::ai::ModelContent> sys_instruction;
  firebase::ai::internal::ParseRequestJson(request_json, &contents, &gen_config,
                                           &sys_instruction);

  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  firebase::ai::Error result_err = firebase::ai::kErrorNone;
  std::string result_err_msg;
  firebase::ai::CountTokensResponse result_resp;

  holder->adapter->CountTokensAsync(
      contents, [&](firebase::ai::Error err, const std::string& err_msg,
                    const firebase::ai::CountTokensResponse& resp) {
        std::lock_guard<std::mutex> lk(mu);
        result_err = err;
        result_err_msg = err_msg;
        result_resp = resp;
        done = true;
        cv.notify_all();
      });

  std::unique_lock<std::mutex> lk(mu);
  cv.wait(lk, [&]() { return done; });

  if (result_err != firebase::ai::kErrorNone) {
    if (out_error) {
      *out_error = firebase::ai::internal::DuplicateCString(result_err_msg);
    }
    return static_cast<int32_t>(result_err);
  }
  if (out_total_tokens) {
    *out_total_tokens = result_resp.total_tokens;
  }
  return static_cast<int32_t>(firebase::ai::kErrorNone);
}

void firebase_ai_litert_free_string(char* str) {
  if (str) {
    std::free(str);
  }
}

}  // extern "C"
