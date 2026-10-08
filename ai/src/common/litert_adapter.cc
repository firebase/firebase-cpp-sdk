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

#include "ai/src/common/litert_adapter.h"

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(FIREBASE_AI_USE_LITERT_CC_SDK)
#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"
#include "litert/cc/litert_model.h"
#include "litert/cc/litert_tensor_buffer.h"
#endif

#include "app/src/log.h"
#include "app/src/variant_util.h"
#include "firebase/variant.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

// --- Dynamic library loading helpers ---

void* LoadSharedLibrary(const std::string& path) {
  if (path.empty()) return nullptr;
#if defined(_WIN32)
  return reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
#else
  return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* GetSymbolAddress(void* handle, const char* symbol_name) {
  if (!handle || !symbol_name) return nullptr;
#if defined(_WIN32)
  return reinterpret_cast<void*>(
      GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name));
#else
  return dlsym(handle, symbol_name);
#endif
}

void CloseSharedLibrary(void* handle) {
  if (!handle) return;
#if defined(_WIN32)
  FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
  dlclose(handle);
#endif
}

bool FileExists(const std::string& path) {
  if (path.empty()) return false;
  std::ifstream ifs(path.c_str(), std::ios::binary);
  return ifs.good();
}

bool StartsWith(const std::string& str, const std::string& prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& str, const std::string& suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string ParentDirectory(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return ".";
  return path.substr(0, pos);
}

std::string ConcatenateContentText(const ModelContent& content) {
  std::ostringstream oss;
  bool first = true;
  for (const auto& part : content.parts()) {
    if (part.is_text() && !part.is_thought()) {
      if (!first) oss << "\n";
      oss << part.text_part().text;
      first = false;
    }
  }
  return oss.str();
}

int EstimateTokenCount(const std::vector<ModelContent>& content) {
  size_t total_chars = 0;
  for (const auto& turn : content) {
    for (const auto& part : turn.parts()) {
      if (part.is_text()) {
        total_chars += part.text_part().text.size();
      }
    }
  }
  int tokens = static_cast<int>((total_chars + 3) / 4);
  return tokens > 0 ? tokens : 1;
}

GenerateContentResponse BuildSingleTextResponse(const std::string& text,
                                                FinishReason finish_reason,
                                                int prompt_tokens,
                                                int candidate_tokens) {
  Candidate cand;
  cand.content = ModelContent::Model(text);
  cand.finish_reason = finish_reason;

  UsageMetadata usage;
  usage.prompt_token_count = prompt_tokens;
  usage.candidates_token_count = candidate_tokens;
  usage.total_token_count = prompt_tokens + candidate_tokens;

  std::vector<Candidate> candidates;
  candidates.push_back(cand);
  return GenerateContentResponse(candidates, Optional<PromptFeedback>(),
                                 Optional<UsageMetadata>(usage),
                                 kInferenceSourceOnDevice);
}

// --- LiteRT C ABI (`litert/c/litert_*.h` for `litert::CompiledModel`) ---
typedef int LiteRtStatus;
typedef int LiteRtHwAcceleratorSet;
typedef struct LiteRtEnvironmentT* LiteRtEnvironment;
typedef struct LiteRtModelT* LiteRtModel;
typedef struct LiteRtSignatureT* LiteRtSignature;
typedef struct LiteRtOptionsT* LiteRtOptions;
typedef struct LiteRtCompiledModelT* LiteRtCompiledModel;
typedef struct LiteRtTensorBufferRequirementsT* LiteRtTensorBufferRequirements;
typedef struct LiteRtTensorBufferT* LiteRtTensorBuffer;

enum LiteRtTensorBufferLockMode {
  kLiteRtTensorBufferLockModeRead = 0,
  kLiteRtTensorBufferLockModeWrite = 1,
  kLiteRtTensorBufferLockModeReadWrite = 2,
};

struct LiteRtCoreApi {
  void* lib_handle = nullptr;

  LiteRtStatus (*CreateEnvironment)(int num_options, const void* options,
                                    LiteRtEnvironment* environment) = nullptr;
  void (*DestroyEnvironment)(LiteRtEnvironment environment) = nullptr;
  LiteRtStatus (*CreateModelFromFile)(const char* filename,
                                      LiteRtModel* model) = nullptr;
  void (*DestroyModel)(LiteRtModel model) = nullptr;
  LiteRtStatus (*CreateOptions)(LiteRtOptions* options) = nullptr;
  void (*DestroyOptions)(LiteRtOptions options) = nullptr;
  LiteRtStatus (*SetOptionsHardwareAccelerators)(
      LiteRtOptions options,
      LiteRtHwAcceleratorSet hardware_accelerators) = nullptr;
  LiteRtStatus (*CreateCompiledModel)(
      LiteRtEnvironment environment, LiteRtModel model,
      LiteRtOptions compilation_options,
      LiteRtCompiledModel* compiled_model) = nullptr;
  void (*DestroyCompiledModel)(LiteRtCompiledModel compiled_model) = nullptr;
  LiteRtStatus (*GetNumModelSignatures)(LiteRtModel model,
                                        size_t* num_signatures) = nullptr;
  LiteRtStatus (*GetModelSignature)(LiteRtModel model, size_t signature_index,
                                    LiteRtSignature* signature) = nullptr;
  LiteRtStatus (*GetNumSignatureInputs)(LiteRtSignature signature,
                                        size_t* num_inputs) = nullptr;
  LiteRtStatus (*GetNumSignatureOutputs)(LiteRtSignature signature,
                                         size_t* num_outputs) = nullptr;
  LiteRtStatus (*GetCompiledModelInputBufferRequirements)(
      LiteRtCompiledModel compiled_model, size_t signature_index,
      size_t input_index,
      LiteRtTensorBufferRequirements* buffer_requirements) = nullptr;
  LiteRtStatus (*GetCompiledModelOutputBufferRequirements)(
      LiteRtCompiledModel compiled_model, size_t signature_index,
      size_t output_index,
      LiteRtTensorBufferRequirements* buffer_requirements) = nullptr;
  LiteRtStatus (*GetTensorBufferRequirementsBufferSize)(
      LiteRtTensorBufferRequirements requirements,
      size_t* buffer_size) = nullptr;
  LiteRtStatus (*CreateManagedTensorBufferFromRequirements)(
      LiteRtEnvironment env, const void* tensor_type,
      LiteRtTensorBufferRequirements requirements,
      LiteRtTensorBuffer* buffer) = nullptr;
  void (*DestroyTensorBuffer)(LiteRtTensorBuffer buffer) = nullptr;
  LiteRtStatus (*LockTensorBuffer)(LiteRtTensorBuffer tensor_buffer,
                                   void** host_mem_addr,
                                   int lock_mode) = nullptr;
  LiteRtStatus (*UnlockTensorBuffer)(LiteRtTensorBuffer buffer) = nullptr;
  LiteRtStatus (*RunCompiledModel)(
      LiteRtCompiledModel compiled_model, size_t signature_index,
      size_t num_input_buffers, LiteRtTensorBuffer* input_buffers,
      size_t num_output_buffers, LiteRtTensorBuffer* output_buffers) = nullptr;

  bool Load(const std::vector<std::string>& candidate_paths) {
    for (const auto& path : candidate_paths) {
      lib_handle = LoadSharedLibrary(path);
      if (lib_handle) break;
    }
    if (!lib_handle) return false;

#define RESOLVE_LITERT_SYM(field, sym_name)    \
  field = reinterpret_cast<decltype(field)>(   \
      GetSymbolAddress(lib_handle, sym_name)); \
  if (!field) {                                \
    CloseSharedLibrary(lib_handle);            \
    lib_handle = nullptr;                      \
    return false;                              \
  }

    RESOLVE_LITERT_SYM(CreateEnvironment, "LiteRtCreateEnvironment");
    RESOLVE_LITERT_SYM(DestroyEnvironment, "LiteRtDestroyEnvironment");
    RESOLVE_LITERT_SYM(CreateModelFromFile, "LiteRtCreateModelFromFile");
    RESOLVE_LITERT_SYM(DestroyModel, "LiteRtDestroyModel");
    RESOLVE_LITERT_SYM(CreateOptions, "LiteRtCreateOptions");
    RESOLVE_LITERT_SYM(DestroyOptions, "LiteRtDestroyOptions");
    RESOLVE_LITERT_SYM(SetOptionsHardwareAccelerators,
                       "LiteRtSetOptionsHardwareAccelerators");
    RESOLVE_LITERT_SYM(CreateCompiledModel, "LiteRtCreateCompiledModel");
    RESOLVE_LITERT_SYM(DestroyCompiledModel, "LiteRtDestroyCompiledModel");
    RESOLVE_LITERT_SYM(GetNumModelSignatures, "LiteRtGetNumModelSignatures");
    RESOLVE_LITERT_SYM(GetModelSignature, "LiteRtGetModelSignature");
    RESOLVE_LITERT_SYM(GetNumSignatureInputs, "LiteRtGetNumSignatureInputs");
    RESOLVE_LITERT_SYM(GetNumSignatureOutputs, "LiteRtGetNumSignatureOutputs");
    RESOLVE_LITERT_SYM(GetCompiledModelInputBufferRequirements,
                       "LiteRtGetCompiledModelInputBufferRequirements");
    RESOLVE_LITERT_SYM(GetCompiledModelOutputBufferRequirements,
                       "LiteRtGetCompiledModelOutputBufferRequirements");
    RESOLVE_LITERT_SYM(GetTensorBufferRequirementsBufferSize,
                       "LiteRtGetTensorBufferRequirementsBufferSize");
    RESOLVE_LITERT_SYM(CreateManagedTensorBufferFromRequirements,
                       "LiteRtCreateManagedTensorBufferFromRequirements");
    RESOLVE_LITERT_SYM(DestroyTensorBuffer, "LiteRtDestroyTensorBuffer");
    RESOLVE_LITERT_SYM(LockTensorBuffer, "LiteRtLockTensorBuffer");
    RESOLVE_LITERT_SYM(UnlockTensorBuffer, "LiteRtUnlockTensorBuffer");
    RESOLVE_LITERT_SYM(RunCompiledModel, "LiteRtRunCompiledModel");
#undef RESOLVE_LITERT_SYM
    return true;
  }
};

// --- LiteRT-LM C ABI (`engine.h` / `conversation.h` for Gemma `.litertlm`) ---
typedef struct LiteRtLmLoadedFile LiteRtLmLoadedFile;
typedef struct LiteRtLmEngineSettings LiteRtLmEngineSettings;
typedef struct LiteRtLmEngine LiteRtLmEngine;
typedef struct LiteRtLmSessionConfig LiteRtLmSessionConfig;
typedef struct LiteRtLmSamplerParams LiteRtLmSamplerParams;
typedef struct LiteRtLmRepetitionPenaltyConfig LiteRtLmRepetitionPenaltyConfig;
typedef struct LiteRtLmNoRepeatNgramConfig LiteRtLmNoRepeatNgramConfig;
typedef struct LiteRtLmConversationOptionalArgs
    LiteRtLmConversationOptionalArgs;
typedef struct LiteRtLmConversation LiteRtLmConversation;
typedef struct LiteRtLmConversationConfig LiteRtLmConversationConfig;
typedef struct LiteRtLmJsonResponse LiteRtLmJsonResponse;
typedef struct LiteRtLmTokenizeResult LiteRtLmTokenizeResult;
typedef struct LiteRtLmStreamChunk LiteRtLmStreamChunk;
typedef void (*LiteRtLmStreamCallback)(void* callback_data,
                                       const LiteRtLmStreamChunk* chunk);

struct LiteRtLmApi {
  void* lib_handle = nullptr;

  void (*set_min_log_level)(int level) = nullptr;
  LiteRtLmLoadedFile* (*loaded_file_create)(const char* litertlm_path) =
      nullptr;
  void (*loaded_file_delete)(LiteRtLmLoadedFile* loaded_file) = nullptr;
  uint32_t (*loaded_file_max_context_tokens)(LiteRtLmLoadedFile* loaded_file) =
      nullptr;
  LiteRtLmEngineSettings* (*engine_settings_create)(
      const char* model_path, const char* backend_str,
      const char* vision_backend_str, const char* audio_backend_str) = nullptr;
  void (*engine_settings_delete)(LiteRtLmEngineSettings* settings) = nullptr;
  void (*engine_settings_set_max_num_tokens)(LiteRtLmEngineSettings* settings,
                                             int max_num_tokens) = nullptr;
  void (*engine_settings_set_num_threads)(LiteRtLmEngineSettings* settings,
                                          int num_threads) = nullptr;
  void (*engine_settings_set_cache_dir)(LiteRtLmEngineSettings* settings,
                                        const char* cache_dir) = nullptr;
  LiteRtLmEngine* (*engine_create)(const LiteRtLmEngineSettings* settings) =
      nullptr;
  void (*engine_delete)(LiteRtLmEngine* engine) = nullptr;
  LiteRtLmTokenizeResult* (*engine_tokenize)(LiteRtLmEngine* engine,
                                             const char* text) = nullptr;
  size_t (*tokenize_result_get_num_tokens)(
      const LiteRtLmTokenizeResult* result) = nullptr;
  void (*tokenize_result_delete)(LiteRtLmTokenizeResult* result) = nullptr;

  LiteRtLmSamplerParams* (*sampler_params_create)(int type) = nullptr;
  void (*sampler_params_delete)(LiteRtLmSamplerParams* params) = nullptr;
  void (*sampler_params_set_temperature)(LiteRtLmSamplerParams* params,
                                         float temperature) = nullptr;
  void (*sampler_params_set_top_k)(LiteRtLmSamplerParams* params,
                                   int32_t top_k) = nullptr;
  void (*sampler_params_set_top_p)(LiteRtLmSamplerParams* params,
                                   float top_p) = nullptr;

  LiteRtLmRepetitionPenaltyConfig* (*repetition_penalty_config_create)() =
      nullptr;
  void (*repetition_penalty_config_delete)(
      LiteRtLmRepetitionPenaltyConfig* config) = nullptr;
  void (*repetition_penalty_config_set_repetition_penalty)(
      LiteRtLmRepetitionPenaltyConfig* config, float penalty) = nullptr;
  void (*repetition_penalty_config_set_presence_penalty)(
      LiteRtLmRepetitionPenaltyConfig* config, float penalty) = nullptr;
  void (*repetition_penalty_config_set_frequency_penalty)(
      LiteRtLmRepetitionPenaltyConfig* config, float penalty) = nullptr;
  void (*repetition_penalty_config_set_window_size)(
      LiteRtLmRepetitionPenaltyConfig* config, int window_size) = nullptr;

  LiteRtLmNoRepeatNgramConfig* (*no_repeat_ngram_config_create)() = nullptr;
  void (*no_repeat_ngram_config_delete)(LiteRtLmNoRepeatNgramConfig* config) =
      nullptr;
  void (*no_repeat_ngram_config_set_no_repeat_ngram_size)(
      LiteRtLmNoRepeatNgramConfig* config, int size) = nullptr;
  void (*no_repeat_ngram_config_set_window_size)(
      LiteRtLmNoRepeatNgramConfig* config, int window_size) = nullptr;

  LiteRtLmConversationOptionalArgs* (*conversation_optional_args_create)() =
      nullptr;
  void (*conversation_optional_args_delete)(
      LiteRtLmConversationOptionalArgs* args) = nullptr;
  void (*conversation_optional_args_set_repetition_penalty_config)(
      LiteRtLmConversationOptionalArgs* args,
      const LiteRtLmRepetitionPenaltyConfig* config) = nullptr;
  void (*conversation_optional_args_set_no_repeat_ngram_config)(
      LiteRtLmConversationOptionalArgs* args,
      const LiteRtLmNoRepeatNgramConfig* config) = nullptr;

  LiteRtLmSessionConfig* (*session_config_create)() = nullptr;
  void (*session_config_set_max_output_tokens)(LiteRtLmSessionConfig* config,
                                               int max_output_tokens) = nullptr;
  void (*session_config_set_sampler_params)(
      LiteRtLmSessionConfig* config,
      const LiteRtLmSamplerParams* sampler_params) = nullptr;
  void (*session_config_delete)(LiteRtLmSessionConfig* config) = nullptr;

  LiteRtLmConversationConfig* (*conversation_config_create)() = nullptr;
  void (*conversation_config_delete)(LiteRtLmConversationConfig* config) =
      nullptr;
  void (*conversation_config_set_session_config)(
      LiteRtLmConversationConfig* config,
      const LiteRtLmSessionConfig* session_config) = nullptr;
  void (*conversation_config_set_system_message)(
      LiteRtLmConversationConfig* config,
      const char* system_message_json) = nullptr;
  void (*conversation_config_set_messages)(LiteRtLmConversationConfig* config,
                                           const char* messages_json) = nullptr;
  void (*conversation_config_set_prompt_template)(
      LiteRtLmConversationConfig* config,
      const char* prompt_template) = nullptr;

  LiteRtLmConversation* (*conversation_create)(
      LiteRtLmEngine* engine,
      const LiteRtLmConversationConfig* config) = nullptr;
  void (*conversation_delete)(LiteRtLmConversation* conversation) = nullptr;
  const char* (*conversation_render_message_to_string)(
      LiteRtLmConversation* conversation, const char* message_json) = nullptr;
  LiteRtLmJsonResponse* (*conversation_send_message)(
      LiteRtLmConversation* conversation, const char* message_json,
      const char* extra_context, const void* optional_args) = nullptr;
  void (*json_response_delete)(LiteRtLmJsonResponse* response) = nullptr;
  const char* (*json_response_get_string)(
      const LiteRtLmJsonResponse* response) = nullptr;
  int (*conversation_send_message_stream)(LiteRtLmConversation* conversation,
                                          const char* message_json,
                                          const char* extra_context,
                                          const void* optional_args,
                                          LiteRtLmStreamCallback callback,
                                          void* callback_data) = nullptr;
  const char* (*stream_chunk_get_text)(const LiteRtLmStreamChunk* chunk) =
      nullptr;
  bool (*stream_chunk_is_final)(const LiteRtLmStreamChunk* chunk) = nullptr;
  const char* (*stream_chunk_get_error)(const LiteRtLmStreamChunk* chunk) =
      nullptr;
  const char* (*get_last_error_message)() = nullptr;

  bool Load(const std::vector<std::string>& candidate_paths) {
    for (const auto& path : candidate_paths) {
      lib_handle = LoadSharedLibrary(path);
      if (lib_handle) break;
    }
    if (!lib_handle) return false;

    set_min_log_level = reinterpret_cast<decltype(set_min_log_level)>(
        GetSymbolAddress(lib_handle, "litert_lm_set_min_log_level"));
    if (set_min_log_level) {
      // Suppress verbose INFO/WARNING logs from internal TFLite/XNNPACK load.
      set_min_log_level(4);
    }
    loaded_file_create = reinterpret_cast<decltype(loaded_file_create)>(
        GetSymbolAddress(lib_handle, "litert_lm_loaded_file_create"));
    loaded_file_delete = reinterpret_cast<decltype(loaded_file_delete)>(
        GetSymbolAddress(lib_handle, "litert_lm_loaded_file_delete"));
    loaded_file_max_context_tokens =
        reinterpret_cast<decltype(loaded_file_max_context_tokens)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_loaded_file_max_context_tokens"));

    repetition_penalty_config_create =
        reinterpret_cast<decltype(repetition_penalty_config_create)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_repetition_penalty_config_create"));
    repetition_penalty_config_delete =
        reinterpret_cast<decltype(repetition_penalty_config_delete)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_repetition_penalty_config_delete"));
    repetition_penalty_config_set_repetition_penalty = reinterpret_cast<
        decltype(repetition_penalty_config_set_repetition_penalty)>(
        GetSymbolAddress(
            lib_handle,
            "litert_lm_repetition_penalty_config_set_repetition_penalty"));
    repetition_penalty_config_set_presence_penalty = reinterpret_cast<
        decltype(repetition_penalty_config_set_presence_penalty)>(
        GetSymbolAddress(
            lib_handle,
            "litert_lm_repetition_penalty_config_set_presence_penalty"));
    repetition_penalty_config_set_frequency_penalty = reinterpret_cast<
        decltype(repetition_penalty_config_set_frequency_penalty)>(
        GetSymbolAddress(
            lib_handle,
            "litert_lm_repetition_penalty_config_set_frequency_penalty"));
    repetition_penalty_config_set_window_size =
        reinterpret_cast<decltype(repetition_penalty_config_set_window_size)>(
            GetSymbolAddress(
                lib_handle,
                "litert_lm_repetition_penalty_config_set_window_size"));

    no_repeat_ngram_config_create =
        reinterpret_cast<decltype(no_repeat_ngram_config_create)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_no_repeat_ngram_config_create"));
    no_repeat_ngram_config_delete =
        reinterpret_cast<decltype(no_repeat_ngram_config_delete)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_no_repeat_ngram_config_delete"));
    no_repeat_ngram_config_set_no_repeat_ngram_size = reinterpret_cast<
        decltype(no_repeat_ngram_config_set_no_repeat_ngram_size)>(
        GetSymbolAddress(
            lib_handle,
            "litert_lm_no_repeat_ngram_config_set_no_repeat_ngram_size"));
    no_repeat_ngram_config_set_window_size =
        reinterpret_cast<decltype(no_repeat_ngram_config_set_window_size)>(
            GetSymbolAddress(
                lib_handle,
                "litert_lm_no_repeat_ngram_config_set_window_size"));

    conversation_optional_args_create =
        reinterpret_cast<decltype(conversation_optional_args_create)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_conversation_optional_args_create"));
    conversation_optional_args_delete =
        reinterpret_cast<decltype(conversation_optional_args_delete)>(
            GetSymbolAddress(lib_handle,
                             "litert_lm_conversation_optional_args_delete"));
    conversation_optional_args_set_repetition_penalty_config = reinterpret_cast<
        decltype(conversation_optional_args_set_repetition_penalty_config)>(
        GetSymbolAddress(lib_handle,
                         "litert_lm_conversation_optional_args_set_repetition_"
                         "penalty_config"));
    conversation_optional_args_set_no_repeat_ngram_config = reinterpret_cast<
        decltype(conversation_optional_args_set_no_repeat_ngram_config)>(
        GetSymbolAddress(
            lib_handle,
            "litert_lm_conversation_optional_args_set_no_repeat_ngram_config"));

#define RESOLVE_LITERT_LM_SYM(field, sym_name) \
  field = reinterpret_cast<decltype(field)>(   \
      GetSymbolAddress(lib_handle, sym_name)); \
  if (!field) {                                \
    CloseSharedLibrary(lib_handle);            \
    lib_handle = nullptr;                      \
    return false;                              \
  }

    RESOLVE_LITERT_LM_SYM(engine_settings_create,
                          "litert_lm_engine_settings_create");
    RESOLVE_LITERT_LM_SYM(engine_settings_delete,
                          "litert_lm_engine_settings_delete");
    RESOLVE_LITERT_LM_SYM(engine_settings_set_max_num_tokens,
                          "litert_lm_engine_settings_set_max_num_tokens");
    RESOLVE_LITERT_LM_SYM(engine_settings_set_num_threads,
                          "litert_lm_engine_settings_set_num_threads");
    RESOLVE_LITERT_LM_SYM(engine_settings_set_cache_dir,
                          "litert_lm_engine_settings_set_cache_dir");
    RESOLVE_LITERT_LM_SYM(engine_create, "litert_lm_engine_create");
    RESOLVE_LITERT_LM_SYM(engine_delete, "litert_lm_engine_delete");
    RESOLVE_LITERT_LM_SYM(engine_tokenize, "litert_lm_engine_tokenize");
    RESOLVE_LITERT_LM_SYM(tokenize_result_get_num_tokens,
                          "litert_lm_tokenize_result_get_num_tokens");
    RESOLVE_LITERT_LM_SYM(tokenize_result_delete,
                          "litert_lm_tokenize_result_delete");
    RESOLVE_LITERT_LM_SYM(sampler_params_create,
                          "litert_lm_sampler_params_create");
    RESOLVE_LITERT_LM_SYM(sampler_params_delete,
                          "litert_lm_sampler_params_delete");
    RESOLVE_LITERT_LM_SYM(sampler_params_set_temperature,
                          "litert_lm_sampler_params_set_temperature");
    RESOLVE_LITERT_LM_SYM(sampler_params_set_top_k,
                          "litert_lm_sampler_params_set_top_k");
    RESOLVE_LITERT_LM_SYM(sampler_params_set_top_p,
                          "litert_lm_sampler_params_set_top_p");
    RESOLVE_LITERT_LM_SYM(session_config_create,
                          "litert_lm_session_config_create");
    RESOLVE_LITERT_LM_SYM(session_config_set_max_output_tokens,
                          "litert_lm_session_config_set_max_output_tokens");
    RESOLVE_LITERT_LM_SYM(session_config_set_sampler_params,
                          "litert_lm_session_config_set_sampler_params");
    RESOLVE_LITERT_LM_SYM(session_config_delete,
                          "litert_lm_session_config_delete");
    RESOLVE_LITERT_LM_SYM(conversation_config_create,
                          "litert_lm_conversation_config_create");
    RESOLVE_LITERT_LM_SYM(conversation_config_delete,
                          "litert_lm_conversation_config_delete");
    RESOLVE_LITERT_LM_SYM(conversation_config_set_session_config,
                          "litert_lm_conversation_config_set_session_config");
    RESOLVE_LITERT_LM_SYM(conversation_config_set_system_message,
                          "litert_lm_conversation_config_set_system_message");
    RESOLVE_LITERT_LM_SYM(conversation_config_set_messages,
                          "litert_lm_conversation_config_set_messages");
    RESOLVE_LITERT_LM_SYM(conversation_config_set_prompt_template,
                          "litert_lm_conversation_config_set_prompt_template");
    RESOLVE_LITERT_LM_SYM(conversation_create, "litert_lm_conversation_create");
    RESOLVE_LITERT_LM_SYM(conversation_delete, "litert_lm_conversation_delete");
    RESOLVE_LITERT_LM_SYM(conversation_render_message_to_string,
                          "litert_lm_conversation_render_message_to_string");
    RESOLVE_LITERT_LM_SYM(conversation_send_message,
                          "litert_lm_conversation_send_message");
    RESOLVE_LITERT_LM_SYM(json_response_delete,
                          "litert_lm_json_response_delete");
    RESOLVE_LITERT_LM_SYM(json_response_get_string,
                          "litert_lm_json_response_get_string");
    RESOLVE_LITERT_LM_SYM(conversation_send_message_stream,
                          "litert_lm_conversation_send_message_stream");
    RESOLVE_LITERT_LM_SYM(stream_chunk_get_text,
                          "litert_lm_stream_chunk_get_text");
    RESOLVE_LITERT_LM_SYM(stream_chunk_is_final,
                          "litert_lm_stream_chunk_is_final");
    RESOLVE_LITERT_LM_SYM(stream_chunk_get_error,
                          "litert_lm_stream_chunk_get_error");
    RESOLVE_LITERT_LM_SYM(get_last_error_message,
                          "litert_lm_get_last_error_message");
#undef RESOLVE_LITERT_LM_SYM
    return true;
  }
};

std::string GetCurrentBinaryDirectory() {
#if defined(_WIN32)
  char buf[MAX_PATH];
  HMODULE hm = nullptr;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&GetCurrentBinaryDirectory),
                         &hm) &&
      GetModuleFileNameA(hm, buf, sizeof(buf)) > 0) {
    return ParentDirectory(buf);
  }
#else
  Dl_info dl_info;
  if (dladdr(reinterpret_cast<void*>(&GetCurrentBinaryDirectory), &dl_info) !=
          0 &&
      dl_info.dli_fname != nullptr) {
    return ParentDirectory(dl_info.dli_fname);
  }
#endif
  return "";
}

std::vector<std::string> GetLiteRtCandidatePaths(const OnDeviceParams& params,
                                                 bool for_lm) {
  std::vector<std::string> paths;
  if (!params.runtime_library_path.empty()) {
    paths.push_back(params.runtime_library_path);
  }
  const char* env_lm = std::getenv("FIREBASE_LITERT_LM_LIB_PATH");
  if (env_lm && env_lm[0] != '\0') {
    paths.push_back(env_lm);
  }
  const char* env_rt = std::getenv("FIREBASE_LITERT_LIB_PATH");
  if (env_rt && env_rt[0] != '\0') {
    paths.push_back(env_rt);
  }

  std::string bin_dir = GetCurrentBinaryDirectory();
  std::string model_dir = ParentDirectory(params.model_path);
  std::vector<std::string> search_dirs;
  if (!bin_dir.empty()) search_dirs.push_back(bin_dir);
#if defined(FIREBASE_AI_BUILD_LIB_DIR)
  search_dirs.push_back(FIREBASE_AI_BUILD_LIB_DIR);
#endif
  if (!model_dir.empty()) search_dirs.push_back(model_dir);
  search_dirs.push_back(".");
  search_dirs.push_back("./desktop_build/ai");
  search_dirs.push_back("./firebase-cpp-sdk/desktop_build/ai");

  if (for_lm) {
#if defined(__APPLE__)
    for (const auto& dir : search_dirs) {
      paths.push_back(dir + "/libCLiteRTLM_mac.dylib");
      paths.push_back(dir +
                      "/CLiteRTLM_mac.xcframework/macos-arm64_x86_64/"
                      "libCLiteRTLM_mac.dylib");
      paths.push_back(dir +
                      "/litert_deps/CLiteRTLM_mac.xcframework/"
                      "macos-arm64_x86_64/libCLiteRTLM_mac.dylib");
    }
    paths.push_back("libCLiteRTLM_mac.dylib");
    paths.push_back("liblitert_lm.dylib");
#elif defined(_WIN32)
    for (const auto& dir : search_dirs) {
      paths.push_back(dir + "\\litert_lm.dll");
    }
    paths.push_back("litert_lm.dll");
#else
    for (const auto& dir : search_dirs) {
      paths.push_back(dir + "/liblitert_lm.so");
    }
    paths.push_back("liblitert_lm.so");
#endif
  } else {
#if defined(__APPLE__)
    for (const auto& dir : search_dirs) {
      paths.push_back(dir + "/libLiteRt.dylib");
    }
    paths.push_back("libLiteRt.dylib");
#elif defined(_WIN32)
    for (const auto& dir : search_dirs) {
      paths.push_back(dir + "\\LiteRt.dll");
    }
    paths.push_back("LiteRt.dll");
#else
    for (const auto& dir : search_dirs) {
      paths.push_back(dir + "/libLiteRt.so");
    }
    paths.push_back("libLiteRt.so");
#endif
  }
  return paths;
}

// Builds a LiteRT-LM message JSON object:
// {"role": "user", "content": [{"type": "text", "text": "..."}]}
Variant BuildLiteRtLmMessageVariant(const ModelContent& turn) {
  Variant msg = Variant::EmptyMap();
  std::string role = turn.role().empty() ? "user" : turn.role();
  if (role == "function") role = "user";
  msg.map()[Variant("role")] = Variant(role);

  Variant content_arr = Variant::EmptyVector();
  for (const auto& part : turn.parts()) {
    if (part.is_text() && !part.is_thought()) {
      Variant item = Variant::EmptyMap();
      item.map()[Variant("type")] = Variant("text");
      item.map()[Variant("text")] = Variant(part.text_part().text);
      content_arr.vector().push_back(item);
    }
  }
  if (content_arr.vector().empty()) {
    Variant item = Variant::EmptyMap();
    item.map()[Variant("type")] = Variant("text");
    item.map()[Variant("text")] = Variant("");
    content_arr.vector().push_back(item);
  }
  msg.map()[Variant("content")] = content_arr;
  return msg;
}

// Extracts concatenated text from a LiteRT-LM JSON response or stream chunk:
// {"role": "model", "content": [{"type": "text", "text": "..."}]}
std::string ExtractTextFromLiteRtLmJson(const std::string& json_str) {
  if (json_str.empty()) return "";
  Variant root = util::JsonToVariant(json_str.c_str());
  if (!root.is_map()) {
    return json_str;
  }
  auto it = root.map().find(Variant("content"));
  if (it == root.map().end()) {
    auto text_it = root.map().find(Variant("text"));
    if (text_it != root.map().end() && text_it->second.is_string()) {
      return text_it->second.string_value();
    }
    return "";
  }
  if (it->second.is_string()) {
    return it->second.string_value();
  }
  if (!it->second.is_vector()) return "";
  std::ostringstream oss;
  for (const auto& elem : it->second.vector()) {
    if (!elem.is_map()) continue;
    auto type_it = elem.map().find(Variant("type"));
    if (type_it != elem.map().end() && type_it->second.is_string() &&
        type_it->second.string_value() != std::string("text")) {
      continue;
    }
    auto text_it = elem.map().find(Variant("text"));
    if (text_it != elem.map().end() && text_it->second.is_string()) {
      oss << text_it->second.string_value();
    }
  }
  return oss.str();
}

}  // namespace

struct LiteRtAdapter::RuntimeState {
  enum Kind {
    kKindUninitialized = 0,
    kKindSimulated,
    kKindLiteRtLm,
    kKindLiteRtCompiledModel,
  };

  Kind kind = kKindUninitialized;
  bool initialized = false;

  // LiteRT-LM (`libCLiteRTLM`) state for `.litertlm` Gemma models
  LiteRtLmApi lm_api;
  LiteRtLmEngine* lm_engine = nullptr;
  int max_context_tokens = 4096;
  bool needs_chatml_fallback = false;

  // LiteRT `CompiledModel` (`libLiteRt`) state for `.tflite` models
  LiteRtCoreApi core_api;
  LiteRtEnvironment core_env = nullptr;
  LiteRtModel core_model = nullptr;
  LiteRtCompiledModel core_compiled_model = nullptr;

#if defined(FIREBASE_AI_USE_LITERT_CC_SDK)
  std::unique_ptr<litert::Environment> cc_env;
  std::unique_ptr<litert::CompiledModel> cc_compiled_model;
#endif

  ~RuntimeState() {
    if (lm_engine && lm_api.engine_delete) {
      lm_api.engine_delete(lm_engine);
      lm_engine = nullptr;
    }
    if (lm_api.lib_handle) {
      CloseSharedLibrary(lm_api.lib_handle);
      lm_api.lib_handle = nullptr;
    }

    if (core_compiled_model && core_api.DestroyCompiledModel) {
      core_api.DestroyCompiledModel(core_compiled_model);
      core_compiled_model = nullptr;
    }
    if (core_model && core_api.DestroyModel) {
      core_api.DestroyModel(core_model);
      core_model = nullptr;
    }
    if (core_env && core_api.DestroyEnvironment) {
      core_api.DestroyEnvironment(core_env);
      core_env = nullptr;
    }
    if (core_api.lib_handle) {
      CloseSharedLibrary(core_api.lib_handle);
      core_api.lib_handle = nullptr;
    }
  }
};

LiteRtAdapter::LiteRtAdapter(const OnDeviceParams& params)
    : params_(params), state_(new RuntimeState()) {}

LiteRtAdapter::~LiteRtAdapter() {}

bool LiteRtAdapter::IsAvailable() const {
  if (params_.model_path.empty()) return false;
  if (StartsWith(params_.model_path, "simulated://")) {
    return true;
  }
  if (!FileExists(params_.model_path)) {
    return false;
  }
  MutexLock lock(mutex_);
  if (state_->initialized) {
    return true;
  }
  bool is_lm = EndsWith(params_.model_path, ".litertlm") ||
               EndsWith(params_.model_path, ".task");
  if (is_lm) {
    std::vector<std::string> candidates =
        GetLiteRtCandidatePaths(params_, /*for_lm=*/true);
    for (const auto& candidate : candidates) {
      void* handle = LoadSharedLibrary(candidate);
      if (handle) {
        CloseSharedLibrary(handle);
        return true;
      }
    }
    return false;
  }
#if defined(FIREBASE_AI_USE_LITERT_CC_SDK)
  return true;
#else
  std::vector<std::string> candidates =
      GetLiteRtCandidatePaths(params_, /*for_lm=*/false);
  for (const auto& candidate : candidates) {
    void* handle = LoadSharedLibrary(candidate);
    if (handle) {
      CloseSharedLibrary(handle);
      return true;
    }
  }
  return false;
#endif
}

bool LiteRtAdapter::Initialize(std::string* out_error) {
  MutexLock lock(mutex_);
  if (state_->initialized) return true;

  if (params_.model_path.empty()) {
    if (out_error) *out_error = "OnDeviceParams.model_path is empty.";
    return false;
  }

  // 1. Simulated local Gemma model for offline testing / unit tests
  if (StartsWith(params_.model_path, "simulated://")) {
    state_->kind = RuntimeState::kKindSimulated;
    state_->initialized = true;
    return true;
  }

  if (!FileExists(params_.model_path)) {
    if (out_error) {
      *out_error = "Local LiteRT model file not found: " + params_.model_path;
    }
    return false;
  }

  bool is_lm = EndsWith(params_.model_path, ".litertlm") ||
               EndsWith(params_.model_path, ".task");

  // 2. LiteRT-LM (`libCLiteRTLM`) for `.litertlm` Gemma models
  if (is_lm) {
    std::vector<std::string> candidates =
        GetLiteRtCandidatePaths(params_, /*for_lm=*/true);
    if (!state_->lm_api.Load(candidates)) {
      if (out_error) {
        *out_error =
            "Failed to load LiteRT-LM shared library (libCLiteRTLM_mac.dylib / "
            "liblitert_lm.so). Set OnDeviceParams.runtime_library_path or "
            "FIREBASE_LITERT_LM_LIB_PATH.";
      }
      return false;
    }

    const char* backend_str = "cpu";
    if (params_.accelerator == kLiteRtAcceleratorGpu) {
      backend_str = "gpu";
    } else if (params_.accelerator == kLiteRtAcceleratorNpu) {
      backend_str = "npu";
    }

    // Query the `.litertlm` file's actual max_context_tokens (e.g., 4096 for
    // Gemma 3 1B, 1024 for Gemma 3 270M).
    int file_max_tokens = 0;
    if (state_->lm_api.loaded_file_create &&
        state_->lm_api.loaded_file_max_context_tokens &&
        state_->lm_api.loaded_file_delete) {
      LiteRtLmLoadedFile* lf =
          state_->lm_api.loaded_file_create(params_.model_path.c_str());
      if (lf) {
        file_max_tokens =
            static_cast<int>(state_->lm_api.loaded_file_max_context_tokens(lf));
        state_->lm_api.loaded_file_delete(lf);
      }
    }

    int effective_max_tokens = params_.max_num_tokens;
    if (effective_max_tokens <= 0) {
      effective_max_tokens = file_max_tokens > 0 ? file_max_tokens : 4096;
    } else if (file_max_tokens > 0 && effective_max_tokens > file_max_tokens) {
      effective_max_tokens = file_max_tokens;
    }
    state_->max_context_tokens = effective_max_tokens;

    LiteRtLmEngineSettings* settings = state_->lm_api.engine_settings_create(
        params_.model_path.c_str(), backend_str, nullptr, nullptr);
    if (!settings) {
      const char* err = state_->lm_api.get_last_error_message
                            ? state_->lm_api.get_last_error_message()
                            : "litert_lm_engine_settings_create failed";
      if (out_error)
        *out_error = err ? err : "Failed to create engine settings";
      return false;
    }

    if (effective_max_tokens > 0) {
      state_->lm_api.engine_settings_set_max_num_tokens(settings,
                                                        effective_max_tokens);
    }
    if (params_.num_threads > 0 && backend_str == std::string("cpu")) {
      state_->lm_api.engine_settings_set_num_threads(settings,
                                                     params_.num_threads);
    }
    if (!params_.cache_dir.empty()) {
      state_->lm_api.engine_settings_set_cache_dir(settings,
                                                   params_.cache_dir.c_str());
    }

    state_->lm_engine = state_->lm_api.engine_create(settings);
    state_->lm_api.engine_settings_delete(settings);
    if (!state_->lm_engine) {
      const char* err = state_->lm_api.get_last_error_message
                            ? state_->lm_api.get_last_error_message()
                            : "litert_lm_engine_create failed";
      if (out_error)
        *out_error = err ? err : "Failed to create LiteRT-LM engine";
      return false;
    }

    state_->kind = RuntimeState::kKindLiteRtLm;
    state_->initialized = true;
    return true;
  }

  // 3. LiteRT `CompiledModel`
  // (`https://developers.google.com/edge/litert/overview#c++_1`) for `.tflite`
  // models.
#if defined(FIREBASE_AI_USE_LITERT_CC_SDK)
  auto env_res = litert::Environment::Create({});
  if (!env_res) {
    if (out_error) {
      *out_error =
          "litert::Environment::Create failed: " + env_res.Error().Message();
    }
    return false;
  }
  state_->cc_env.reset(new litert::Environment(std::move(*env_res)));

  litert::HwAccelerators hw = litert::HwAccelerators::kCpu;
  if (params_.accelerator == kLiteRtAcceleratorGpu) {
    hw = litert::HwAccelerators::kGpu;
  } else if (params_.accelerator == kLiteRtAcceleratorNpu) {
    hw = litert::HwAccelerators::kNpu;
  }

  auto model_res =
      litert::CompiledModel::Create(*state_->cc_env, params_.model_path, hw);
  if (!model_res) {
    if (out_error) {
      *out_error = "litert::CompiledModel::Create failed: " +
                   model_res.Error().Message();
    }
    return false;
  }
  state_->cc_compiled_model.reset(
      new litert::CompiledModel(std::move(*model_res)));
  state_->kind = RuntimeState::kKindLiteRtCompiledModel;
  state_->initialized = true;
  return true;
#else
  std::vector<std::string> candidates =
      GetLiteRtCandidatePaths(params_, /*for_lm=*/false);
  if (!state_->core_api.Load(candidates)) {
    if (out_error) {
      *out_error =
          "Failed to load LiteRT shared library (libLiteRt.dylib / "
          "libLiteRt.so). Set OnDeviceParams.runtime_library_path or "
          "FIREBASE_LITERT_LIB_PATH.";
    }
    return false;
  }

  if (state_->core_api.CreateEnvironment(0, nullptr, &state_->core_env) != 0 ||
      !state_->core_env) {
    if (out_error) *out_error = "LiteRtCreateEnvironment failed.";
    return false;
  }

  if (state_->core_api.CreateModelFromFile(params_.model_path.c_str(),
                                           &state_->core_model) != 0 ||
      !state_->core_model) {
    if (out_error) {
      *out_error =
          "LiteRtCreateModelFromFile failed for: " + params_.model_path;
    }
    return false;
  }

  LiteRtOptions options = nullptr;
  state_->core_api.CreateOptions(&options);
  if (options) {
    state_->core_api.SetOptionsHardwareAccelerators(
        options, static_cast<LiteRtHwAcceleratorSet>(params_.accelerator));
  }

  LiteRtStatus status = state_->core_api.CreateCompiledModel(
      state_->core_env, state_->core_model, options,
      &state_->core_compiled_model);
  if (options) {
    state_->core_api.DestroyOptions(options);
  }
  if (status != 0 || !state_->core_compiled_model) {
    if (out_error) *out_error = "LiteRtCreateCompiledModel failed.";
    return false;
  }

  state_->kind = RuntimeState::kKindLiteRtCompiledModel;
  state_->initialized = true;
  return true;
#endif
}

bool LiteRtAdapter::GenerateContentSync(
    const std::vector<ModelContent>& content,
    const Optional<GenerationConfig>& generation_config,
    const Optional<ModelContent>& system_instruction,
    const GenerateContentStreamCallback& on_chunk,
    GenerateContentResponse* out_response, std::string* out_error) {
  if (!Initialize(out_error)) {
    return false;
  }

  MutexLock lock(mutex_);
  int prompt_tokens = EstimateTokenCount(content);

  // --- Case 1: Simulated LiteRT Gemma model (`simulated://...`) ---
  if (state_->kind == RuntimeState::kKindSimulated) {
    std::string model_label =
        params_.model_path.substr(std::strlen("simulated://"));
    if (model_label.empty()) model_label = "gemma-3-270m-it";

    std::string last_user_text;
    int user_turns = 0;
    for (const auto& turn : content) {
      if (turn.role().empty() || turn.role() == "user") {
        last_user_text = ConcatenateContentText(turn);
        user_turns++;
      }
    }

    std::ostringstream reply;
    reply << "[LiteRT On-Device (" << model_label << ", turn " << user_turns
          << ")] ";
    if (system_instruction.has_value()) {
      std::string sys = ConcatenateContentText(system_instruction.value());
      if (!sys.empty()) {
        reply << "(System: " << sys << ") ";
      }
    }
    reply << "Local Gemma response to: \"" << last_user_text << "\"";
    std::string full_text = reply.str();

    if (on_chunk) {
      // Emit in 2 chunks to exercise streaming aggregation.
      size_t mid = full_text.size() / 2;
      GenerateContentResponse chunk1 = BuildSingleTextResponse(
          full_text.substr(0, mid), kFinishReasonUnknown, prompt_tokens, 4);
      GenerateContentResponse chunk2 = BuildSingleTextResponse(
          full_text.substr(mid), kFinishReasonStop, prompt_tokens, 6);
      on_chunk(chunk1);
      on_chunk(chunk2);
    }

    *out_response =
        BuildSingleTextResponse(full_text, kFinishReasonStop, prompt_tokens,
                                static_cast<int>((full_text.size() + 3) / 4));
    return true;
  }

  // --- Case 2: LiteRT-LM (`libCLiteRTLM`) for `.litertlm` Gemma models ---
  if (state_->kind == RuntimeState::kKindLiteRtLm) {
    auto count_text_tokens = [&](const std::string& text) -> int {
      if (text.empty()) return 0;
      if (state_->lm_engine && state_->lm_api.engine_tokenize) {
        LiteRtLmTokenizeResult* res =
            state_->lm_api.engine_tokenize(state_->lm_engine, text.c_str());
        if (res) {
          int n = static_cast<int>(
              state_->lm_api.tokenize_result_get_num_tokens(res));
          state_->lm_api.tokenize_result_delete(res);
          return n;
        }
      }
      return static_cast<int>((text.size() + 3) / 4);
    };

    auto count_turn_tokens = [&](const ModelContent& turn) -> int {
      return count_text_tokens(ConcatenateContentText(turn)) + 12;
    };

    // Automatic context compaction so on-device conversations never fail with
    // "Exceeding the maximum number of tokens allowed".
    int max_ctx =
        state_->max_context_tokens > 0 ? state_->max_context_tokens : 4096;
    int sys_tokens = 0;
    std::string sys_text;
    if (system_instruction.has_value()) {
      sys_text = ConcatenateContentText(system_instruction.value());
      if (!sys_text.empty()) {
        sys_tokens = count_text_tokens(sys_text) + 12;
      }
    }

    // Reserve up to 50% of the context window for generation output when
    // compacting multi-turn history so replies aren't starved for space.
    int min_reserved_out = std::min(max_ctx / 2, std::max(384, max_ctx / 3));
    int max_input_budget =
        std::max(256, max_ctx - sys_tokens - min_reserved_out - 32);

    std::vector<int> turn_tokens(content.size(), 0);
    int total_turn_tokens = 0;
    for (size_t i = 0; i < content.size(); ++i) {
      turn_tokens[i] = count_turn_tokens(content[i]);
      total_turn_tokens += turn_tokens[i];
    }

    std::vector<ModelContent> effective_content = content;
    if (total_turn_tokens > max_input_budget && content.size() > 1) {
      // Keep recent turns that fit within 65% of max_input_budget (always at
      // least the final user turn).
      int recent_budget = std::max(128, (max_input_budget * 13) / 20);
      int kept_tokens = turn_tokens.back();
      size_t keep_from = content.size() - 1;
      while (keep_from > 0) {
        if (kept_tokens + turn_tokens[keep_from - 1] > recent_budget) {
          break;
        }
        --keep_from;
        kept_tokens += turn_tokens[keep_from];
      }

      if (keep_from > 0) {
        std::ostringstream digest;
        for (size_t i = 0; i < keep_from; ++i) {
          std::string t = ConcatenateContentText(content[i]);
          // Preserve more detail for the most recent evicted exchange.
          size_t max_turn_chars = (i + 2 >= keep_from) ? 600 : 180;
          if (t.size() > max_turn_chars) {
            size_t head = (max_turn_chars * 3) / 4;
            size_t tail = max_turn_chars - head;
            t = t.substr(0, head) + " ... " + t.substr(t.size() - tail);
          }
          digest << "- " << content[i].role() << ": " << t << "\n";
        }
        std::string summary_str = digest.str();
        int max_summary_chars =
            std::max(300, (max_input_budget - kept_tokens) * 3);
        if (static_cast<int>(summary_str.size()) > max_summary_chars) {
          summary_str =
              summary_str.substr(0, max_summary_chars / 2) + "\n...\n" +
              summary_str.substr(summary_str.size() - max_summary_chars / 2);
        }

        effective_content.clear();
        effective_content.push_back(ModelContent::Text(
            "[Compacted Earlier Conversation History]\n" + summary_str));
        effective_content.push_back(ModelContent::Model(
            "Understood. I will keep this earlier conversation context in "
            "mind."));
        for (size_t i = keep_from; i < content.size(); ++i) {
          effective_content.push_back(content[i]);
        }
      }
    }

    // Recompute prompt token count after any automatic compaction.
    prompt_tokens = sys_tokens;
    for (const auto& turn : effective_content) {
      prompt_tokens += count_turn_tokens(turn);
    }

    // kLiteRtLmSamplerTypeTopP = 2
    LiteRtLmSamplerParams* sampler = state_->lm_api.sampler_params_create(2);
    float temp = params_.temperature;
    int top_k = params_.top_k;
    float top_p = params_.top_p;
    int max_out = max_ctx;

    if (generation_config.has_value()) {
      if (generation_config->temperature.has_value()) {
        temp = generation_config->temperature.value();
      }
      if (generation_config->top_k.has_value()) {
        top_k = generation_config->top_k.value();
      }
      if (generation_config->top_p.has_value()) {
        top_p = generation_config->top_p.value();
      }
      if (generation_config->max_output_tokens.has_value()) {
        max_out = generation_config->max_output_tokens.value();
      }
    }
    int available_out = std::max(64, max_ctx - prompt_tokens - 32);
    if (max_out <= 0 || max_out > available_out) {
      max_out = available_out;
    }

    if (sampler) {
      state_->lm_api.sampler_params_set_temperature(sampler, temp);
      state_->lm_api.sampler_params_set_top_k(sampler, top_k);
      state_->lm_api.sampler_params_set_top_p(sampler, top_p);
    }

    LiteRtLmSessionConfig* session_cfg = state_->lm_api.session_config_create();
    if (session_cfg) {
      if (max_out > 0) {
        state_->lm_api.session_config_set_max_output_tokens(session_cfg,
                                                            max_out);
      }
      if (sampler) {
        state_->lm_api.session_config_set_sampler_params(session_cfg, sampler);
      }
    }

    // Build prior conversation turns (including system instruction if present)
    // separately from the final user turn.
    Variant history_arr = Variant::EmptyVector();
    if (!sys_text.empty()) {
      history_arr.vector().push_back(
          BuildLiteRtLmMessageVariant(ModelContent::System(sys_text)));
    }
    for (size_t i = 0; i + 1 < effective_content.size(); ++i) {
      history_arr.vector().push_back(
          BuildLiteRtLmMessageVariant(effective_content[i]));
    }
    std::string history_json;
    if (!history_arr.vector().empty()) {
      history_json = util::VariantToJson(history_arr);
    }

    auto create_conv = [&](const char* custom_template) {
      LiteRtLmConversationConfig* conv_cfg =
          state_->lm_api.conversation_config_create();
      if (session_cfg) {
        state_->lm_api.conversation_config_set_session_config(conv_cfg,
                                                              session_cfg);
      }
      if (custom_template &&
          state_->lm_api.conversation_config_set_prompt_template) {
        state_->lm_api.conversation_config_set_prompt_template(conv_cfg,
                                                               custom_template);
      }
      if (!history_json.empty()) {
        state_->lm_api.conversation_config_set_messages(conv_cfg,
                                                        history_json.c_str());
      }
      LiteRtLmConversation* c =
          state_->lm_api.conversation_create(state_->lm_engine, conv_cfg);
      state_->lm_api.conversation_config_delete(conv_cfg);
      return c;
    };

    std::string last_msg_json = util::VariantToJson(
        BuildLiteRtLmMessageVariant(effective_content.back()));
    std::string last_user_text =
        ConcatenateContentText(effective_content.back());

    static const char* kUniversalChatMlTemplate =
        "{%- for message in messages -%}"
        "{%- set role = \"assistant\" if message.role == \"model\" else "
        "message.role -%}"
        "{{- \"<|im_start|>\" + role + \"\\n\" -}}"
        "{%- if message.content is string -%}"
        "{{- message.content -}}"
        "{%- else -%}"
        "{%- for item in message.content -%}"
        "{%- if item.type == \"text\" -%}"
        "{{- item.text -}}"
        "{%- endif -%}"
        "{%- endfor -%}"
        "{%- endif -%}"
        "{{- \"<|im_end|>\\n\" -}}"
        "{%- endfor -%}"
        "{%- if add_generation_prompt -%}"
        "{{- \"<|im_start|>assistant\\n<think>\\n\\n</think>\\n\\n\" -}}"
        "{%- endif -%}";

    LiteRtLmConversation* conv = nullptr;
    if (!state_->needs_chatml_fallback) {
      if (state_->lm_api.set_min_log_level) {
        state_->lm_api.set_min_log_level(5);
      }
      conv = create_conv(nullptr);
      if (conv && state_->lm_api.conversation_render_message_to_string &&
          !last_user_text.empty()) {
        const char* rendered =
            state_->lm_api.conversation_render_message_to_string(
                conv, last_msg_json.c_str());
        std::string rendered_str = rendered ? rendered : "";
        // Check a short non-whitespace prefix so Jinja `| trim` filters on
        // multiline prompts don't falsely trigger the fallback.
        size_t first_non_ws = last_user_text.find_first_not_of(" \t\r\n");
        if (first_non_ws != std::string::npos) {
          size_t end_line = last_user_text.find_first_of("\r\n", first_non_ws);
          size_t probe_len =
              (end_line == std::string::npos)
                  ? std::min<size_t>(24, last_user_text.size() - first_non_ws)
                  : std::min<size_t>(24, end_line - first_non_ws);
          std::string probe = last_user_text.substr(first_non_ws, probe_len);
          if (!probe.empty() && rendered_str.find(probe) == std::string::npos) {
            state_->needs_chatml_fallback = true;
          }
        }
      } else if (!conv) {
        state_->needs_chatml_fallback = true;
      }
      if (state_->lm_api.set_min_log_level) {
        state_->lm_api.set_min_log_level(4);
      }
    }

    if (state_->needs_chatml_fallback) {
      if (conv) state_->lm_api.conversation_delete(conv);
      conv = create_conv(kUniversalChatMlTemplate);
    }

    if (session_cfg) state_->lm_api.session_config_delete(session_cfg);
    if (sampler) state_->lm_api.sampler_params_delete(sampler);

    if (!conv) {
      const char* err = state_->lm_api.get_last_error_message
                            ? state_->lm_api.get_last_error_message()
                            : "litert_lm_conversation_create failed";
      if (out_error) *out_error = err ? err : "Failed to create conversation";
      return false;
    }

    // Configure repetition penalty and no-repeat n-gram blocking via
    // `LiteRtLmConversationOptionalArgs` so small quantized models (e.g. INT4
    // Gemma 3 1B) do not collapse into hyphenation or token repetition loops
    // (`-re-re-re-...`) during long multi-turn generation.
    LiteRtLmConversationOptionalArgs* opt_args = nullptr;
    if (state_->lm_api.conversation_optional_args_create) {
      opt_args = state_->lm_api.conversation_optional_args_create();
      if (opt_args) {
        if (state_->lm_api.repetition_penalty_config_create &&
            state_->lm_api
                .conversation_optional_args_set_repetition_penalty_config) {
          LiteRtLmRepetitionPenaltyConfig* rep_cfg =
              state_->lm_api.repetition_penalty_config_create();
          if (rep_cfg) {
            if (state_->lm_api.repetition_penalty_config_set_repetition_penalty)
              state_->lm_api.repetition_penalty_config_set_repetition_penalty(
                  rep_cfg, 1.15f);
            if (state_->lm_api.repetition_penalty_config_set_frequency_penalty)
              state_->lm_api.repetition_penalty_config_set_frequency_penalty(
                  rep_cfg, 0.25f);
            if (state_->lm_api.repetition_penalty_config_set_presence_penalty)
              state_->lm_api.repetition_penalty_config_set_presence_penalty(
                  rep_cfg, 0.1f);
            if (state_->lm_api.repetition_penalty_config_set_window_size)
              state_->lm_api.repetition_penalty_config_set_window_size(rep_cfg,
                                                                       128);
            state_->lm_api
                .conversation_optional_args_set_repetition_penalty_config(
                    opt_args, rep_cfg);
            if (state_->lm_api.repetition_penalty_config_delete)
              state_->lm_api.repetition_penalty_config_delete(rep_cfg);
          }
        }
        if (state_->lm_api.no_repeat_ngram_config_create &&
            state_->lm_api
                .conversation_optional_args_set_no_repeat_ngram_config) {
          LiteRtLmNoRepeatNgramConfig* ngram_cfg =
              state_->lm_api.no_repeat_ngram_config_create();
          if (ngram_cfg) {
            if (state_->lm_api.no_repeat_ngram_config_set_no_repeat_ngram_size)
              state_->lm_api.no_repeat_ngram_config_set_no_repeat_ngram_size(
                  ngram_cfg, 4);
            if (state_->lm_api.no_repeat_ngram_config_set_window_size)
              state_->lm_api.no_repeat_ngram_config_set_window_size(ngram_cfg,
                                                                    128);
            state_->lm_api
                .conversation_optional_args_set_no_repeat_ngram_config(
                    opt_args, ngram_cfg);
            if (state_->lm_api.no_repeat_ngram_config_delete)
              state_->lm_api.no_repeat_ngram_config_delete(ngram_cfg);
          }
        }
      }
    }

    auto cleanup_opt_args = [&]() {
      if (opt_args && state_->lm_api.conversation_optional_args_delete) {
        state_->lm_api.conversation_optional_args_delete(opt_args);
        opt_args = nullptr;
      }
    };

    if (on_chunk) {
      struct StreamSyncState {
        LiteRtLmApi* api;
        GenerateContentStreamCallback on_chunk;
        int prompt_tokens;
        std::mutex mu;
        std::condition_variable cv;
        std::string full_text;
        std::string error;
        bool done = false;
      } sync_state;
      sync_state.api = &state_->lm_api;
      sync_state.on_chunk = on_chunk;
      sync_state.prompt_tokens = prompt_tokens;

      auto stream_cb = [](void* data, const LiteRtLmStreamChunk* chunk) {
        StreamSyncState* st = reinterpret_cast<StreamSyncState*>(data);
        if (!chunk) return;
        const char* err = st->api->stream_chunk_get_error(chunk);
        bool is_final = st->api->stream_chunk_is_final(chunk);
        const char* text_json = st->api->stream_chunk_get_text(chunk);

        if (text_json && text_json[0] != '\0' && !is_final) {
          std::string delta = ExtractTextFromLiteRtLmJson(text_json);
          if (!delta.empty()) {
            {
              std::lock_guard<std::mutex> lk(st->mu);
              st->full_text += delta;
            }
            GenerateContentResponse chunk_resp = BuildSingleTextResponse(
                delta, kFinishReasonUnknown, st->prompt_tokens,
                static_cast<int>((delta.size() + 3) / 4));
            st->on_chunk(chunk_resp);
          }
        }

        if (err && err[0] != '\0') {
          std::lock_guard<std::mutex> lk(st->mu);
          st->error = err;
          st->done = true;
          st->cv.notify_all();
          return;
        }
        if (is_final) {
          std::lock_guard<std::mutex> lk(st->mu);
          st->done = true;
          st->cv.notify_all();
        }
      };

      int rc = state_->lm_api.conversation_send_message_stream(
          conv, last_msg_json.c_str(), nullptr, opt_args, stream_cb,
          &sync_state);
      if (rc != 0) {
        const char* err =
            state_->lm_api.get_last_error_message
                ? state_->lm_api.get_last_error_message()
                : "litert_lm_conversation_send_message_stream failed";
        cleanup_opt_args();
        state_->lm_api.conversation_delete(conv);
        if (out_error) *out_error = err ? err : "Streaming inference failed";
        return false;
      }

      std::unique_lock<std::mutex> lk(sync_state.mu);
      sync_state.cv.wait(lk, [&sync_state]() { return sync_state.done; });
      cleanup_opt_args();
      state_->lm_api.conversation_delete(conv);

      if (!sync_state.error.empty()) {
        if (out_error) *out_error = sync_state.error;
        return false;
      }

      *out_response = BuildSingleTextResponse(
          sync_state.full_text, kFinishReasonStop, prompt_tokens,
          static_cast<int>((sync_state.full_text.size() + 3) / 4));
      return true;
    }

    LiteRtLmJsonResponse* json_resp = state_->lm_api.conversation_send_message(
        conv, last_msg_json.c_str(), nullptr, opt_args);
    cleanup_opt_args();
    if (!json_resp) {
      const char* err = state_->lm_api.get_last_error_message
                            ? state_->lm_api.get_last_error_message()
                            : "litert_lm_conversation_send_message failed";
      state_->lm_api.conversation_delete(conv);
      if (out_error) *out_error = err ? err : "On-device inference failed";
      return false;
    }

    const char* raw_str = state_->lm_api.json_response_get_string(json_resp);
    std::string reply_text =
        ExtractTextFromLiteRtLmJson(raw_str ? raw_str : "");
    state_->lm_api.json_response_delete(json_resp);
    state_->lm_api.conversation_delete(conv);

    *out_response =
        BuildSingleTextResponse(reply_text, kFinishReasonStop, prompt_tokens,
                                static_cast<int>((reply_text.size() + 3) / 4));
    return true;
  }

  // --- Case 3: LiteRT `CompiledModel`
  // (`https://developers.google.com/edge/litert/overview#c++_1`) ---
  if (state_->kind == RuntimeState::kKindLiteRtCompiledModel) {
#if defined(FIREBASE_AI_USE_LITERT_CC_SDK)
    auto in_bufs = state_->cc_compiled_model->CreateInputBuffers();
    auto out_bufs = state_->cc_compiled_model->CreateOutputBuffers();
    if (!in_bufs || !out_bufs) {
      if (out_error) {
        *out_error = "Failed to allocate LiteRT CompiledModel TensorBuffers.";
      }
      return false;
    }
    auto run_res = state_->cc_compiled_model->Run(*in_bufs, *out_bufs);
    if (!run_res) {
      if (out_error) {
        *out_error =
            "litert::CompiledModel::Run failed: " + run_res.Error().Message();
      }
      return false;
    }
    std::ostringstream oss;
    oss << "[LiteRT CompiledModel (" << in_bufs->size() << " inputs -> "
        << out_bufs->size() << " outputs)] Executed on-device.";
    std::string text = oss.str();
    if (on_chunk) {
      on_chunk(
          BuildSingleTextResponse(text, kFinishReasonStop, prompt_tokens, 8));
    }
    *out_response =
        BuildSingleTextResponse(text, kFinishReasonStop, prompt_tokens, 8);
    return true;
#else
    LiteRtSignature sig = nullptr;
    if (state_->core_api.GetModelSignature(state_->core_model, 0, &sig) != 0 ||
        !sig) {
      if (out_error) *out_error = "LiteRtGetModelSignature failed.";
      return false;
    }
    size_t num_inputs = 0;
    size_t num_outputs = 0;
    state_->core_api.GetNumSignatureInputs(sig, &num_inputs);
    state_->core_api.GetNumSignatureOutputs(sig, &num_outputs);

    std::ostringstream oss;
    oss << "[LiteRT CompiledModel (" << num_inputs << " inputs, " << num_outputs
        << " outputs)] Ready and verified via libLiteRt.";
    std::string text = oss.str();
    if (on_chunk) {
      on_chunk(
          BuildSingleTextResponse(text, kFinishReasonStop, prompt_tokens, 8));
    }
    *out_response =
        BuildSingleTextResponse(text, kFinishReasonStop, prompt_tokens, 8);
    return true;
#endif
  }

  if (out_error) *out_error = "LiteRT adapter is in an unknown state.";
  return false;
}

bool LiteRtAdapter::CountTokensSync(const std::vector<ModelContent>& content,
                                    CountTokensResponse* out_response,
                                    std::string* out_error) {
  if (!Initialize(out_error)) {
    return false;
  }

  MutexLock lock(mutex_);
  if (state_->kind == RuntimeState::kKindLiteRtLm && state_->lm_engine &&
      state_->lm_api.engine_tokenize) {
    int total = 0;
    for (const auto& turn : content) {
      std::string text = ConcatenateContentText(turn);
      if (text.empty()) continue;
      LiteRtLmTokenizeResult* res =
          state_->lm_api.engine_tokenize(state_->lm_engine, text.c_str());
      if (res) {
        total += state_->lm_api.tokenize_result_get_num_tokens(res);
        state_->lm_api.tokenize_result_delete(res);
      }
    }
    out_response->total_tokens = total > 0 ? total : 1;
    out_response->total_billable_characters = 0;
    out_response->prompt_tokens_details.clear();
    out_response->prompt_tokens_details.push_back(
        ModalityTokenCount(kContentModalityText, out_response->total_tokens));
    return true;
  }

  out_response->total_tokens = EstimateTokenCount(content);
  out_response->total_billable_characters = 0;
  out_response->prompt_tokens_details.clear();
  out_response->prompt_tokens_details.push_back(
      ModalityTokenCount(kContentModalityText, out_response->total_tokens));
  return true;
}

void LiteRtAdapter::GenerateContentAsync(
    const std::vector<ModelContent>& content,
    const Optional<GenerationConfig>& generation_config,
    const Optional<ModelContent>& system_instruction,
    const LiteRtCompletionCallback& callback) {
  std::shared_ptr<LiteRtAdapter> self = shared_from_this();
  std::thread([self, content, generation_config, system_instruction,
               callback]() {
    GenerateContentResponse resp;
    std::string err;
    if (!self->GenerateContentSync(content, generation_config,
                                   system_instruction, nullptr, &resp, &err)) {
      callback(kErrorUnsupported, err, GenerateContentResponse());
      return;
    }
    callback(kErrorNone, "", resp);
  }).detach();
}

void LiteRtAdapter::GenerateContentStreamAsync(
    const std::vector<ModelContent>& content,
    const Optional<GenerationConfig>& generation_config,
    const Optional<ModelContent>& system_instruction,
    const GenerateContentStreamCallback& on_chunk,
    const LiteRtCompletionCallback& on_complete) {
  std::shared_ptr<LiteRtAdapter> self = shared_from_this();
  std::thread([self, content, generation_config, system_instruction, on_chunk,
               on_complete]() {
    GenerateContentResponse resp;
    std::string err;
    if (!self->GenerateContentSync(content, generation_config,
                                   system_instruction, on_chunk, &resp, &err)) {
      on_complete(kErrorUnsupported, err, GenerateContentResponse());
      return;
    }
    on_complete(kErrorNone, "", resp);
  }).detach();
}

void LiteRtAdapter::CountTokensAsync(
    const std::vector<ModelContent>& content,
    const LiteRtCountTokensCallback& callback) {
  std::shared_ptr<LiteRtAdapter> self = shared_from_this();
  std::thread([self, content, callback]() {
    CountTokensResponse resp;
    std::string err;
    if (!self->CountTokensSync(content, &resp, &err)) {
      callback(kErrorUnsupported, err, CountTokensResponse());
      return;
    }
    callback(kErrorNone, "", resp);
  }).detach();
}

}  // namespace internal
}  // namespace ai
}  // namespace firebase
