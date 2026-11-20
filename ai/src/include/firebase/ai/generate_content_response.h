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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATE_CONTENT_RESPONSE_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATE_CONTENT_RESPONSE_H_

#include <string>
#include <vector>

#include "firebase/ai/model_content.h"
#include "firebase/ai/safety.h"
#include "firebase/ai/types.h"

namespace firebase {
namespace ai {

/// @brief A single citation referencing a source used by the model in a
/// response.
struct Citation {
  /// @brief Default constructor.
  Citation() : start_index(0), end_index(0) {}

  /// @brief Start byte/character index in the response text.
  int start_index;

  /// @brief End byte/character index in the response text.
  int end_index;

  /// @brief URI of the cited source, if available.
  std::string uri;

  /// @brief Title of the cited work, if available.
  std::string title;

  /// @brief License of the cited work, if available.
  std::string license;

  /// @brief Publication date (ISO-8601 string or year-month-day), if available.
  std::string publication_date;
};

/// @brief Collection of citations for a `Candidate` response.
struct CitationMetadata {
  /// @brief List of citations attributed to the candidate.
  std::vector<Citation> citations;
};

/// @brief Grounding chunk from a web source.
struct WebGroundingChunk {
  /// @brief URI reference of the web chunk.
  std::string uri;

  /// @brief Title of the web page.
  std::string title;

  /// @brief Domain of the web source (if provided).
  std::string domain;
};

/// @brief Grounding chunk from Google Maps.
struct GoogleMapsGroundingChunk {
  /// @brief URI link to the place on Google Maps.
  std::string uri;

  /// @brief Title / name of the place.
  std::string title;

  /// @brief Google Maps Place ID (`places/...`).
  std::string place_id;
};

/// @brief A chunk of grounding data supporting a response candidate.
struct GroundingChunk {
  /// @brief Web grounding chunk details, if this chunk came from the web.
  Optional<WebGroundingChunk> web;

  /// @brief Google Maps grounding chunk details, if this chunk came from Maps.
  Optional<GoogleMapsGroundingChunk> maps;
};

/// @brief Identifies a specific span of text within a `ModelContent` part.
struct GroundingSegment {
  /// @brief Default constructor.
  GroundingSegment() : part_index(0), start_index(0), end_index(0) {}

  /// @brief Zero-based index of the `Part` in `ModelContent::parts()`.
  int part_index;

  /// @brief Start byte index in the part's text.
  int start_index;

  /// @brief End byte index in the part's text.
  int end_index;

  /// @brief The text corresponding to the segment.
  std::string text;
};

/// @brief Links a `GroundingSegment` in the model response to one or more
/// `GroundingChunk` indices.
struct GroundingSupport {
  /// @brief The segment of the model response supported by the chunks.
  GroundingSegment segment;

  /// @brief Indices into `GroundingMetadata::grounding_chunks`.
  std::vector<int> grounding_chunk_indices;
};

/// @brief Google Search entry point widget data returned with grounded
/// responses.
struct SearchEntryPoint {
  /// @brief Web content snippet HTML/CSS that can be embedded in a webview.
  std::string rendered_content;

  /// @brief Base64-encoded JSON blob representing query/url pairs.
  std::string sdk_blob;
};

/// @brief Metadata returned when grounding (such as Google Search or Google
/// Maps) is enabled.
struct GroundingMetadata {
  /// @brief Search queries executed by the model for web grounding.
  std::vector<std::string> web_search_queries;

  /// @brief Search entry point widget information, if available.
  Optional<SearchEntryPoint> search_entry_point;

  /// @brief Supporting chunks retrieved during grounding.
  std::vector<GroundingChunk> grounding_chunks;

  /// @brief Links between response text segments and `grounding_chunks`.
  std::vector<GroundingSupport> grounding_supports;

  /// @brief Optional Google Maps widget context token.
  Optional<std::string> google_maps_widget_context_token;
};

/// @brief Metadata for a single URL retrieved by the `UrlContext` tool.
struct UrlMetadata {
  /// @brief Default constructor.
  UrlMetadata() : retrieval_status(kUrlRetrievalStatusUnspecified) {}

  /// @brief The retrieved URL.
  std::string retrieved_url;

  /// @brief Status of retrieving the URL.
  UrlRetrievalStatus retrieval_status;
};

/// @brief Metadata returned when the `UrlContext` tool is used.
struct UrlContextMetadata {
  /// @brief List of URLs retrieved and their retrieval status.
  std::vector<UrlMetadata> url_metadata;
};

/// @brief A response candidate generated by the model.
///
/// Mirrors `Firebase.AI.Candidate` in Unity and `Candidate` in Flutter.
struct Candidate {
  /// @brief Default constructor.
  Candidate() : finish_reason(kFinishReasonUnknown) {}

  /// @brief Generated content returned from the model.
  ModelContent content;

  /// @brief Safety ratings for the response candidate.
  std::vector<SafetyRating> safety_ratings;

  /// @brief Citation metadata for the response candidate, if any.
  Optional<CitationMetadata> citation_metadata;

  /// @brief Grounding metadata for the response candidate, if any.
  Optional<GroundingMetadata> grounding_metadata;

  /// @brief URL context metadata for the response candidate, if any.
  Optional<UrlContextMetadata> url_context_metadata;

  /// @brief The reason why the model stopped generating tokens.
  FinishReason finish_reason;

  /// @brief Additional message explaining `finish_reason`, if any.
  std::string finish_message;
};

/// @brief Content filtering metadata for the input prompt.
///
/// Mirrors `Firebase.AI.PromptFeedback` in Unity and `PromptFeedback` in
/// Flutter.
struct PromptFeedback {
  /// @brief Default constructor.
  PromptFeedback() : block_reason(kBlockReasonUnknown) {}

  /// @brief The reason why the prompt was blocked, if it was blocked.
  BlockReason block_reason;

  /// @brief Safety ratings for the prompt across harm categories.
  std::vector<SafetyRating> safety_ratings;

  /// @brief Human-readable message describing `block_reason`, if any.
  std::string block_reason_message;
};

/// @brief Token count breakdown for a specific `ContentModality`.
struct ModalityTokenCount {
  /// @brief Default constructor.
  ModalityTokenCount()
      : modality(kContentModalityUnspecified), token_count(0) {}

  /// @brief Construct a `ModalityTokenCount`.
  ModalityTokenCount(ContentModality modality, int token_count)
      : modality(modality), token_count(token_count) {}

  /// @brief The modality associated with this token count.
  ContentModality modality;

  /// @brief The number of tokens for this modality.
  int token_count;
};

/// @brief Token usage metadata for a `GenerateContentResponse`.
///
/// Mirrors `Firebase.AI.UsageMetadata` in Unity and `UsageMetadata` in Flutter.
struct UsageMetadata {
  /// @brief Default constructor initializes all counts to 0.
  UsageMetadata()
      : prompt_token_count(0),
        candidates_token_count(0),
        total_token_count(0),
        thoughts_token_count(0),
        tool_use_prompt_token_count(0),
        cached_content_token_count(0) {}

  /// @brief Number of tokens in the input prompt.
  int prompt_token_count;

  /// @brief Number of tokens in the generated candidates.
  int candidates_token_count;

  /// @brief Total number of tokens across prompt, thinking, and candidates.
  int total_token_count;

  /// @brief Number of tokens used by the model's thinking process.
  int thoughts_token_count;

  /// @brief Number of tokens in tool-use prompt results.
  int tool_use_prompt_token_count;

  /// @brief Number of tokens served from cached content.
  int cached_content_token_count;

  /// @brief Per-modality breakdown of prompt tokens.
  std::vector<ModalityTokenCount> prompt_tokens_details;

  /// @brief Per-modality breakdown of candidate output tokens.
  std::vector<ModalityTokenCount> candidates_tokens_details;

  /// @brief Per-modality breakdown of tool-use prompt tokens.
  std::vector<ModalityTokenCount> tool_use_prompt_tokens_details;

  /// @brief Per-modality breakdown of cached content tokens.
  std::vector<ModalityTokenCount> cache_tokens_details;
};

/// @brief The model's response to a generate content request.
///
/// Mirrors `Firebase.AI.GenerateContentResponse` in Unity and
/// `GenerateContentResponse` in Flutter.
class GenerateContentResponse {
 public:
  /// @brief Default constructor.
  GenerateContentResponse() : inference_source_(kInferenceSourceInCloud) {}

  /// @brief Construct a `GenerateContentResponse`.
  GenerateContentResponse(
      const std::vector<Candidate>& candidates,
      const Optional<PromptFeedback>& prompt_feedback,
      const Optional<UsageMetadata>& usage_metadata,
      InferenceSource inference_source = kInferenceSourceInCloud)
      : candidates_(candidates),
        prompt_feedback_(prompt_feedback),
        usage_metadata_(usage_metadata),
        inference_source_(inference_source) {}

  /// @brief Returns the list of response candidates generated by the model.
  const std::vector<Candidate>& candidates() const { return candidates_; }
  /// @brief Sets the list of response candidates.
  void set_candidates(const std::vector<Candidate>& candidates) {
    candidates_ = candidates;
  }

  /// @brief Returns the prompt feedback, if any.
  const Optional<PromptFeedback>& prompt_feedback() const {
    return prompt_feedback_;
  }
  /// @brief Sets the prompt feedback.
  void set_prompt_feedback(const PromptFeedback& feedback) {
    prompt_feedback_ = feedback;
  }

  /// @brief Returns the token usage metadata, if any.
  const Optional<UsageMetadata>& usage_metadata() const {
    return usage_metadata_;
  }
  /// @brief Sets the token usage metadata.
  void set_usage_metadata(const UsageMetadata& metadata) {
    usage_metadata_ = metadata;
  }

  /// @brief Returns whether this response was generated in the cloud
  /// (`kInferenceSourceInCloud`) or locally on-device via LiteRT
  /// (`kInferenceSourceOnDevice`).
  InferenceSource inference_source() const { return inference_source_; }
  /// @brief Sets the inference source for this response.
  void set_inference_source(InferenceSource source) {
    inference_source_ = source;
  }

  /// @brief Convenience property returning the concatenated non-thought text
  /// parts of the first candidate, or an empty string if none exist.
  std::string text() const;

  /// @brief Convenience property returning the concatenated thought summary
  /// text parts of the first candidate, or an empty string if none exist.
  std::string thought_summary() const;

  /// @brief Convenience property returning all `FunctionCallPart` items from
  /// the first candidate.
  std::vector<FunctionCallPart> function_calls() const;

  /// @brief Convenience property returning all non-thought `InlineDataPart`
  /// items (such as generated images) from the first candidate.
  std::vector<InlineDataPart> inline_data_parts() const;

 private:
  std::vector<Candidate> candidates_;
  Optional<PromptFeedback> prompt_feedback_;
  Optional<UsageMetadata> usage_metadata_;
  InferenceSource inference_source_;
};

/// @brief The model's response to a `CountTokens` request.
///
/// Mirrors `Firebase.AI.CountTokensResponse` in Unity and
/// `CountTokensResponse` in Flutter.
struct CountTokensResponse {
  /// @brief Default constructor.
  CountTokensResponse() : total_tokens(0), total_billable_characters(0) {}

  /// @brief Total number of tokens that the input content represents.
  int total_tokens;

  /// @brief Total number of billable characters (Deprecated; may be 0).
  int total_billable_characters;

  /// @brief Breakdown of prompt tokens by modality.
  std::vector<ModalityTokenCount> prompt_tokens_details;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_GENERATE_CONTENT_RESPONSE_H_
