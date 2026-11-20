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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TYPES_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TYPES_H_

#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "firebase/internal/common.h"

namespace firebase {
namespace ai {

/// @brief Error codes returned by Firebase AI Logic futures.
enum Error {
  /// The operation was a success, no error occurred.
  kErrorNone = 0,
  /// Invalid argument passed to the API (e.g., empty model name or prompt).
  kErrorInvalidArgument = 1,
  /// Failed to serialize request or deserialize response JSON.
  kErrorSerializationFailed = 2,
  /// Network or transport failure while communicating with the backend.
  kErrorNetworkFailed = 3,
  /// The request timed out before completing.
  kErrorTimeout = 4,
  /// The backend returned an HTTP error status (e.g., 4xx or 5xx).
  kErrorHttpError = 5,
  /// The prompt or response was blocked by safety settings or policy.
  kErrorResponseBlocked = 6,
  /// The FirebaseAI or GenerativeModel instance is no longer valid.
  kErrorInvalidState = 7,
  /// An unknown or internal error occurred.
  kErrorUnknown = 8,
  /// The requested operation or on-device model is not available/supported.
  kErrorUnsupported = 9,
};

/// @brief Lightweight optional value container for C++14 compatibility.
template <typename T>
class Optional {
 public:
  /// @brief Construct an empty Optional.
  Optional() : has_value_(false), value_() {}

  /// @brief Construct an Optional containing a value.
  ///
  /// @param value The value to store.
  Optional(const T& value)  // NOLINT(runtime/explicit)
      : has_value_(true), value_(value) {}

  /// @brief Construct an Optional by moving a value.
  ///
  /// @param value The value to move into this Optional.
  Optional(T&& value)  // NOLINT(runtime/explicit)
      : has_value_(true), value_(std::move(value)) {}

  /// @brief Construct an Optional from an implicitly convertible value (e.g.,
  /// `const char*` for `Optional<std::string>`).
  template <
      typename U,
      typename std::enable_if<
          !std::is_same<typename std::decay<U>::type, Optional<T>>::value &&
              !std::is_same<typename std::decay<U>::type, T>::value &&
              std::is_convertible<U, T>::value,
          int>::type = 0>
  Optional(U&& value)  // NOLINT(runtime/explicit)
      : has_value_(true), value_(std::forward<U>(value)) {}

  /// @brief Assign a value to this Optional.
  ///
  /// @param value The value to store.
  /// @return Reference to this Optional.
  Optional& operator=(const T& value) {
    has_value_ = true;
    value_ = value;
    return *this;
  }

  /// @brief Move-assign a value to this Optional.
  ///
  /// @param value The value to move into this Optional.
  /// @return Reference to this Optional.
  Optional& operator=(T&& value) {
    has_value_ = true;
    value_ = std::move(value);
    return *this;
  }

  /// @brief Assign an implicitly convertible value to this Optional.
  template <
      typename U,
      typename std::enable_if<
          !std::is_same<typename std::decay<U>::type, Optional<T>>::value &&
              !std::is_same<typename std::decay<U>::type, T>::value &&
              std::is_convertible<U, T>::value,
          int>::type = 0>
  Optional& operator=(U&& value) {
    has_value_ = true;
    value_ = T(std::forward<U>(value));
    return *this;
  }

  /// @brief Returns true if this Optional holds a value.
  bool has_value() const { return has_value_; }

  /// @brief Conversion to bool indicating whether a value is present.
  explicit operator bool() const { return has_value_; }

  /// @brief Returns a const reference to the contained value.
  const T& value() const {
    assert(has_value_);
    return value_;
  }

  /// @brief Returns a mutable reference to the contained value.
  T& value() {
    assert(has_value_);
    return value_;
  }

  /// @brief Returns the contained value if present, or `default_value`.
  ///
  /// @param default_value Fallback value when empty.
  /// @return The contained value or `default_value`.
  T value_or(const T& default_value) const {
    return has_value_ ? value_ : default_value;
  }

  /// @brief Dereference operator.
  const T& operator*() const { return value(); }

  /// @brief Dereference operator.
  T& operator*() { return value(); }

  /// @brief Member access operator.
  const T* operator->() const { return &value(); }

  /// @brief Member access operator.
  T* operator->() { return &value(); }

  /// @brief Clears any contained value.
  void reset() {
    has_value_ = false;
    value_ = T();
  }

  /// @brief Equality comparison.
  bool operator==(const Optional<T>& other) const {
    if (has_value_ != other.has_value_) return false;
    return !has_value_ || (value_ == other.value_);
  }

  /// @brief Inequality comparison.
  bool operator!=(const Optional<T>& other) const { return !(*this == other); }

 private:
  bool has_value_;
  T value_;
};

/// @brief Identifies which backend provider to target for Firebase AI calls.
enum BackendProvider {
  /// The Gemini Developer API backend (`GoogleAI`).
  kBackendProviderGoogleAI = 0,
  /// The Vertex AI Gemini API backend (`Enterprise` / `VertexAI`).
  kBackendProviderEnterprise = 1,
};

/// @brief Specifies the backend configuration for `FirebaseAI`.
///
/// Mirrors `FirebaseAI.Backend` in the Unity and Flutter SDKs.
class Backend {
 public:
  /// @brief Creates a `Backend` targeting the Gemini Developer API
  /// (`GoogleAI`).
  ///
  /// @return A `Backend` configured for GoogleAI.
  static Backend GoogleAI() { return Backend(kBackendProviderGoogleAI, ""); }

  /// @brief Creates a `Backend` targeting the Vertex AI Gemini API.
  ///
  /// @param location The Google Cloud region identifier, defaulting to "global"
  /// (as in the Unity SDK).
  /// @return A `Backend` configured for Vertex AI / Enterprise.
  static Backend Enterprise(const std::string& location = "global") {
    return Backend(kBackendProviderEnterprise, location);
  }

  /// @brief Alias for `Enterprise`, matching the Flutter SDK `vertexAI` naming.
  ///
  /// @param location The Google Cloud region identifier, defaulting to
  /// "us-central1".
  /// @return A `Backend` configured for Vertex AI.
  static Backend VertexAI(const std::string& location = "us-central1") {
    return Backend(kBackendProviderEnterprise, location);
  }

  /// @brief Default constructor initializes to `Backend::GoogleAI()`.
  Backend() : provider_(kBackendProviderGoogleAI), location_("") {}

  /// @brief Returns the backend provider type.
  BackendProvider provider() const { return provider_; }

  /// @brief Returns the configured location (for Enterprise / VertexAI).
  const std::string& location() const { return location_; }

  /// @brief Equality comparison.
  bool operator==(const Backend& other) const {
    return provider_ == other.provider_ && location_ == other.location_;
  }

  /// @brief Inequality comparison.
  bool operator!=(const Backend& other) const { return !(*this == other); }

  /// @brief Less-than operator for use in associative containers.
  bool operator<(const Backend& other) const {
    if (provider_ != other.provider_) {
      return static_cast<int>(provider_) < static_cast<int>(other.provider_);
    }
    return location_ < other.location_;
  }

 private:
  Backend(BackendProvider provider, const std::string& location)
      : provider_(provider), location_(location) {}

  BackendProvider provider_;
  std::string location_;
};

/// @brief Configuration options for requests made to the backend.
struct RequestOptions {
  /// @brief Default request timeout in milliseconds (180 seconds).
  static const int64_t kDefaultTimeoutMs = 180000;

  /// @brief Construct default RequestOptions.
  ///
  /// @param timeout_ms Request timeout in milliseconds.
  /// @param limited_use_app_check_token Whether to use a limited-use App Check
  /// token instead of a cached token.
  explicit RequestOptions(int64_t timeout_ms = kDefaultTimeoutMs,
                          bool limited_use_app_check_token = false)
      : timeout_ms(timeout_ms),
        limited_use_app_check_token(limited_use_app_check_token) {}

  /// @brief Request timeout in milliseconds.
  int64_t timeout_ms;

  /// @brief Whether to request a limited-use App Check token.
  bool limited_use_app_check_token;
};

/// @brief Categories of harm that the model checks for safety ratings and
/// filtering.
enum HarmCategory {
  /// Category is unspecified or unrecognized.
  kHarmCategoryUnknown = 0,
  /// Harassment content.
  kHarmCategoryHarassment,
  /// Hate speech and content.
  kHarmCategoryHateSpeech,
  /// Sexually explicit content.
  kHarmCategorySexuallyExplicit,
  /// Dangerous content.
  kHarmCategoryDangerousContent,
  /// Content that may harm civic integrity.
  kHarmCategoryCivicIntegrity,
};

/// @brief Threshold levels for blocking harmful content in `SafetySetting`.
enum HarmBlockThreshold {
  /// Threshold is unspecified.
  kHarmBlockThresholdUnknown = 0,
  /// Block when low, medium, or high probability of harm is detected.
  kHarmBlockThresholdLowAndAbove,
  /// Block when medium or high probability of harm is detected.
  kHarmBlockThresholdMediumAndAbove,
  /// Block only when high probability of harm is detected.
  kHarmBlockThresholdOnlyHigh,
  /// Always show content regardless of harm probability (still rated).
  kHarmBlockThresholdNone,
  /// Disable the safety filter completely.
  kHarmBlockThresholdOff,
};

/// @brief Specify how the block threshold should be evaluated in
/// `SafetySetting`.
enum HarmBlockMethod {
  /// Method is unspecified.
  kHarmBlockMethodUnknown = 0,
  /// Consider both probability and severity scores.
  kHarmBlockMethodSeverity,
  /// Consider only the probability score.
  kHarmBlockMethodProbability,
};

/// @brief Probability that a given piece of content is harmful.
enum HarmProbability {
  /// Probability is unspecified or unrecognized.
  kHarmProbabilityUnknown = 0,
  /// Content has a negligible chance of being unsafe.
  kHarmProbabilityNegligible,
  /// Content has a low chance of being unsafe.
  kHarmProbabilityLow,
  /// Content has a medium chance of being unsafe.
  kHarmProbabilityMedium,
  /// Content has a high chance of being unsafe.
  kHarmProbabilityHigh,
};

/// @brief Severity of harm for a piece of content.
enum HarmSeverity {
  /// Severity is unspecified or unrecognized.
  kHarmSeverityUnknown = 0,
  /// Negligible degree of harm.
  kHarmSeverityNegligible,
  /// Low degree of harm.
  kHarmSeverityLow,
  /// Medium degree of harm.
  kHarmSeverityMedium,
  /// High degree of harm.
  kHarmSeverityHigh,
};

/// @brief Reason why a model stopped generating tokens for a `Candidate`.
enum FinishReason {
  /// Finish reason is unspecified or unrecognized.
  kFinishReasonUnknown = 0,
  /// Natural stop point of the model or provided stop sequence.
  kFinishReasonStop,
  /// The maximum number of tokens as specified in the request was reached.
  kFinishReasonMaxTokens,
  /// The token generation was stopped because the response was flagged for
  /// safety reasons.
  kFinishReasonSafety,
  /// The token generation was stopped because the response was flagged for
  /// unauthorized citations.
  kFinishReasonRecitation,
  /// The token generation was stopped for another reason.
  kFinishReasonOther,
  /// Token generation was stopped because the response contained forbidden
  /// terms.
  kFinishReasonBlocklist,
  /// Token generation was stopped because the response contained potentially
  /// prohibited content.
  kFinishReasonProhibitedContent,
  /// Token generation was stopped because the content potentially contained
  /// Sensitive Personally Identifiable Information (SPII).
  kFinishReasonSpii,
  /// The function call generated by the model is invalid.
  kFinishReasonMalformedFunctionCall,
};

/// @brief Reason why a prompt was blocked in `PromptFeedback`.
enum BlockReason {
  /// Block reason is unspecified or unrecognized.
  kBlockReasonUnknown = 0,
  /// The prompt was blocked because it was flagged by safety settings.
  kBlockReasonSafety,
  /// The prompt was blocked for another reason.
  kBlockReasonOther,
  /// The prompt was blocked because it contained terms from the terminology
  /// blocklist.
  kBlockReasonBlocklist,
  /// The prompt was blocked because it contained prohibited content.
  kBlockReasonProhibitedContent,
};

/// @brief Content modality type used for token counting details.
enum ContentModality {
  /// Unrecognized or unspecified modality.
  kContentModalityUnspecified = 0,
  /// Plain text.
  kContentModalityText,
  /// Image.
  kContentModalityImage,
  /// Video.
  kContentModalityVideo,
  /// Audio.
  kContentModalityAudio,
  /// Document (e.g., PDF).
  kContentModalityDocument,
};

/// @brief Supported response modalities for `GenerationConfig`.
enum ResponseModality {
  /// Unspecified modality.
  kResponseModalityUnspecified = 0,
  /// Text output modality.
  kResponseModalityText,
  /// Image output modality.
  kResponseModalityImage,
  /// Audio output modality (reserved).
  kResponseModalityAudio,
};

/// @brief Thinking level for Gemini 2.5+ thinking models.
enum ThinkingLevel {
  /// Unspecified thinking level.
  kThinkingLevelUnspecified = 0,
  /// Minimal thinking.
  kThinkingLevelMinimal,
  /// Low thinking.
  kThinkingLevelLow,
  /// Medium thinking.
  kThinkingLevelMedium,
  /// High thinking.
  kThinkingLevelHigh,
};

/// @brief Outcome of a server-side code execution part.
enum CodeExecutionOutcome {
  /// Unspecified or unrecognized outcome.
  kCodeExecutionOutcomeUnspecified = 0,
  /// Code execution completed successfully.
  kCodeExecutionOutcomeOk,
  /// Code execution finished but with an error.
  kCodeExecutionOutcomeFailed,
  /// Code execution timed out.
  kCodeExecutionOutcomeDeadlineExceeded,
};

/// @brief Status of URL retrieval for URL context grounding.
enum UrlRetrievalStatus {
  /// Unspecified or unrecognized status.
  kUrlRetrievalStatusUnspecified = 0,
  /// The URL was retrieved successfully.
  kUrlRetrievalStatusSuccess,
  /// The URL retrieval failed with an error.
  kUrlRetrievalStatusError,
  /// The URL could not be retrieved because it is behind a paywall.
  kUrlRetrievalStatusPaywall,
  /// The URL content was flagged as unsafe.
  kUrlRetrievalStatusUnsafe,
};

/// @brief Geographical coordinates (latitude and longitude).
struct LatLng {
  /// @brief Default constructor initializes coordinates to (0, 0).
  LatLng() : latitude(0.0), longitude(0.0) {}

  /// @brief Construct a LatLng with given latitude and longitude.
  ///
  /// @param latitude Latitude in degrees [-90, 90].
  /// @param longitude Longitude in degrees [-180, 180].
  LatLng(double latitude, double longitude)
      : latitude(latitude), longitude(longitude) {}

  /// @brief The latitude in degrees.
  double latitude;

  /// @brief The longitude in degrees.
  double longitude;
};

/// @brief Configuration for retrieval-based tools (such as Google Maps).
struct RetrievalConfig {
  /// @brief Optional geographical location of the user.
  Optional<LatLng> lat_lng;

  /// @brief Optional language code (BCP-47) for localized results.
  Optional<std::string> language_code;
};

/// @brief Determines how `GenerativeModel` routes requests between on-device
/// LiteRT inference and cloud Firebase AI inference.
///
/// Mirrors `InferenceMode` in the Firebase AI Web SDK (`hybrid-helpers.ts`).
enum InferenceMode {
  /// Prefer on-device LiteRT inference if the local model is available;
  /// fall back to cloud Firebase AI if unavailable or if local inference fails.
  kInferenceModePreferOnDevice = 0,
  /// Only use on-device LiteRT inference. Fails with `kErrorUnsupported` if the
  /// local model is unavailable.
  kInferenceModeOnlyOnDevice = 1,
  /// Only use cloud Firebase AI inference (default when no `HybridParams` are
  /// configured).
  kInferenceModeOnlyInCloud = 2,
  /// Prefer cloud Firebase AI inference; fall back to on-device LiteRT
  /// inference if the cloud request fails (for example, when offline).
  kInferenceModePreferInCloud = 3,
};

/// @brief Indicates whether a `GenerateContentResponse` was produced by the
/// cloud backend or by the on-device LiteRT model.
enum InferenceSource {
  /// Response was generated by the cloud Firebase AI backend.
  kInferenceSourceInCloud = 0,
  /// Response was generated locally on-device via LiteRT / LiteRT-LM.
  kInferenceSourceOnDevice = 1,
};

/// @brief Hardware accelerator selection for Google AI Edge LiteRT
/// (`litert::HwAccelerators`).
enum LiteRtAccelerator {
  /// CPU execution (`litert::HwAccelerators::kCpu`).
  kLiteRtAcceleratorCpu = 1,
  /// GPU execution (`litert::HwAccelerators::kGpu`).
  kLiteRtAcceleratorGpu = 2,
  /// NPU execution (`litert::HwAccelerators::kNpu`).
  kLiteRtAcceleratorNpu = 4,
};

/// @brief Configuration for local on-device inference powered by Google AI Edge
/// LiteRT (`https://developers.google.com/edge/litert/overview#c++_1`) and
/// LiteRT-LM for Gemma `.litertlm` / `.tflite` models.
struct OnDeviceParams {
  /// @brief Default constructor.
  OnDeviceParams()
      : accelerator(kLiteRtAcceleratorCpu),
        max_num_tokens(0),
        num_threads(4),
        temperature(0.8f),
        top_k(40),
        top_p(0.95f) {}

  /// @brief Construct `OnDeviceParams` with a local model path.
  ///
  /// @param model_path Path to a local `.litertlm` (Gemma) or `.tflite`
  /// (LiteRT CompiledModel) file, or `"simulated://gemma-3-270m-it"` for
  /// local testing without a downloaded model file.
  /// @param accelerator Hardware accelerator (`kLiteRtAcceleratorCpu`,
  /// `kLiteRtAcceleratorGpu`, or `kLiteRtAcceleratorNpu`).
  explicit OnDeviceParams(const std::string& model_path,
                          LiteRtAccelerator accelerator = kLiteRtAcceleratorCpu)
      : model_path(model_path),
        accelerator(accelerator),
        max_num_tokens(0),
        num_threads(4),
        temperature(0.8f),
        top_k(40),
        top_p(0.95f) {}

  /// @brief File path to the local `.litertlm` or `.tflite` model on disk.
  std::string model_path;

  /// @brief Optional path to the LiteRT / LiteRT-LM shared library
  /// (`libLiteRt.dylib` / `libCLiteRTLM_mac.dylib` / `.so` / `.dll`). If empty,
  /// the runtime searches standard library paths and `FIREBASE_LITERT_LIB_PATH`
  /// / `FIREBASE_LITERT_LM_LIB_PATH` environment variables.
  std::string runtime_library_path;

  /// @brief Optional directory for caching compiled LiteRT artifacts.
  std::string cache_dir;

  /// @brief Hardware accelerator to target (`Cpu`, `Gpu`, `Npu`).
  LiteRtAccelerator accelerator;

  /// @brief Maximum context + output token capacity for LiteRT-LM. If 0
  /// (default), automatically queries
  /// `litert_lm_loaded_file_max_context_tokens` from the `.litertlm` model file
  /// (e.g., 4096 for Gemma 3 1B).
  int max_num_tokens;

  /// @brief Number of CPU threads for LiteRT execution.
  int num_threads;

  /// @brief Default sampling temperature (overridden by `GenerationConfig` if
  /// set).
  float temperature;

  /// @brief Default top-K sampling parameter (overridden by `GenerationConfig`
  /// if set).
  int top_k;

  /// @brief Default top-P nucleus sampling parameter (overridden by
  /// `GenerationConfig` if set).
  float top_p;
};

/// @brief Hybrid inference configuration combining an `InferenceMode` policy
/// with `OnDeviceParams` for LiteRT on-device execution.
struct HybridParams {
  /// @brief Default constructor initializes to `kInferenceModeOnlyInCloud`.
  HybridParams() : mode(kInferenceModeOnlyInCloud) {}

  /// @brief Construct `HybridParams` with a mode and on-device LiteRT params.
  HybridParams(InferenceMode mode, const OnDeviceParams& on_device_params)
      : mode(mode), on_device_params(on_device_params) {}

  /// @brief Routing mode between cloud Firebase AI and on-device LiteRT.
  InferenceMode mode;

  /// @brief Parameters for the on-device LiteRT / LiteRT-LM model.
  OnDeviceParams on_device_params;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_TYPES_H_
