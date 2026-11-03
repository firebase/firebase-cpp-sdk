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

#ifndef FIREBASE_AI_SRC_COMMON_LITERT_C_BRIDGE_H_
#define FIREBASE_AI_SRC_COMMON_LITERT_C_BRIDGE_H_

#include <stdint.h>

#if defined(_WIN32)
#define FIREBASE_AI_C_EXPORT __declspec(dllexport)
#else
#define FIREBASE_AI_C_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Opaque handle to a C++ `firebase::ai::internal::LiteRtAdapter`
/// instance used by the Unity C# SDK for hybrid on-device inference.
typedef void* FirebaseAiLiteRtHandle;

/// @brief Callback invoked for each streamed `GenerateContentResponse` JSON
/// chunk from on-device LiteRT inference.
typedef void (*FirebaseAiLiteRtStreamChunkCallback)(const char* chunk_json,
                                                    void* user_data);

/// @brief Creates a C++ `LiteRtAdapter` handle for Unity P/Invoke.
FIREBASE_AI_C_EXPORT FirebaseAiLiteRtHandle firebase_ai_litert_create(
    const char* model_path, const char* runtime_library_path,
    const char* cache_dir, int32_t accelerator, int32_t max_num_tokens,
    int32_t num_threads, float temperature, int32_t top_k, float top_p);

/// @brief Destroys a `FirebaseAiLiteRtHandle` created by
/// `firebase_ai_litert_create`.
FIREBASE_AI_C_EXPORT void firebase_ai_litert_destroy(
    FirebaseAiLiteRtHandle handle);

/// @brief Returns 1 if the local LiteRT / LiteRT-LM model is available, 0
/// otherwise.
FIREBASE_AI_C_EXPORT int32_t
firebase_ai_litert_is_available(FirebaseAiLiteRtHandle handle);

/// @brief Eagerly initializes the on-device LiteRT model. Returns 0 on success,
/// non-zero `firebase::ai::Error` on failure (allocating `*out_error` if
/// non-null).
FIREBASE_AI_C_EXPORT int32_t
firebase_ai_litert_initialize(FirebaseAiLiteRtHandle handle, char** out_error);

/// @brief Runs synchronous on-device LiteRT inference from a Gemini-format
/// `GenerateContentRequest` JSON string and writes a Gemini-format
/// `GenerateContentResponse` JSON string to `*out_response_json`.
///
/// Caller must free `*out_response_json` and `*out_error` via
/// `firebase_ai_litert_free_string`.
FIREBASE_AI_C_EXPORT int32_t firebase_ai_litert_generate_content(
    FirebaseAiLiteRtHandle handle, const char* request_json,
    char** out_response_json, char** out_error);

/// @brief Runs streaming on-device LiteRT inference from a Gemini-format
/// `GenerateContentRequest` JSON string, invoking `chunk_callback` with each
/// incremental `GenerateContentResponse` JSON chunk.
FIREBASE_AI_C_EXPORT int32_t firebase_ai_litert_generate_content_stream(
    FirebaseAiLiteRtHandle handle, const char* request_json,
    FirebaseAiLiteRtStreamChunkCallback chunk_callback, void* user_data,
    char** out_error);

/// @brief Counts tokens locally using the C++ `LiteRtAdapter`.
FIREBASE_AI_C_EXPORT int32_t firebase_ai_litert_count_tokens(
    FirebaseAiLiteRtHandle handle, const char* request_json,
    int32_t* out_total_tokens, char** out_error);

/// @brief Frees a heap-allocated C string returned by `firebase_ai_litert_*`.
FIREBASE_AI_C_EXPORT void firebase_ai_litert_free_string(char* str);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // FIREBASE_AI_SRC_COMMON_LITERT_C_BRIDGE_H_
