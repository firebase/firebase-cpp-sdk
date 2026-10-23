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

#ifndef FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_FUNCTION_CALLING_H_
#define FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_FUNCTION_CALLING_H_

#include <map>
#include <string>
#include <vector>

#include "firebase/ai/schema.h"
#include "firebase/ai/types.h"

namespace firebase {
namespace ai {

/// @brief Structured representation of a function declaration that the model
/// may invoke.
///
/// Mirrors `Firebase.AI.FunctionDeclaration` in Unity and
/// `FunctionDeclaration` in Flutter.
class FunctionDeclaration {
 public:
  /// @brief Default constructor.
  FunctionDeclaration() : uses_json_schema_(false) {}

  /// @brief Construct a `FunctionDeclaration` using OpenAPI `Schema` parameter
  /// definitions.
  ///
  /// @param name The name of the function. Must be a-z, A-Z, 0-9, or contain
  /// underscores and dashes, with a maximum length of 63.
  /// @param description A brief description of what the function does.
  /// @param parameters Map of parameter names to their `Schema` definitions.
  /// @param optional_parameters List of parameter names that are optional. Any
  /// parameter in `parameters` not listed here is marked as required.
  FunctionDeclaration(const std::string& name, const std::string& description,
                      const std::map<std::string, Schema>& parameters,
                      const std::vector<std::string>& optional_parameters =
                          std::vector<std::string>());

  /// @brief Construct a `FunctionDeclaration` using a full `JsonSchema` object
  /// for its parameters (`parametersJsonSchema`).
  ///
  /// @param name The name of the function.
  /// @param description A brief description of what the function does.
  /// @param parameters_json_schema The root object `JsonSchema` describing the
  /// function parameters.
  FunctionDeclaration(const std::string& name, const std::string& description,
                      const JsonSchema& parameters_json_schema);

  /// @brief Returns the function name.
  const std::string& name() const { return name_; }
  /// @brief Sets the function name.
  void set_name(const std::string& name) { name_ = name; }

  /// @brief Returns the function description.
  const std::string& description() const { return description_; }
  /// @brief Sets the function description.
  void set_description(const std::string& description) {
    description_ = description;
  }

  /// @brief Returns the parameter schema.
  const Schema& parameters() const { return parameters_; }
  /// @brief Sets the parameter schema.
  void set_parameters(const Schema& parameters) { parameters_ = parameters; }

  /// @brief Returns true if this declaration uses `parametersJsonSchema` rather
  /// than OpenAPI `parameters`.
  bool uses_json_schema() const { return uses_json_schema_; }
  /// @brief Sets whether this declaration uses `parametersJsonSchema`.
  void set_uses_json_schema(bool uses_json_schema) {
    uses_json_schema_ = uses_json_schema;
  }

 private:
  std::string name_;
  std::string description_;
  Schema parameters_;
  bool uses_json_schema_;
};

/// @brief Tool that enables the model to ground its responses using Google
/// Search.
struct GoogleSearch {};

/// @brief Tool that enables the model to generate and execute Python code on
/// the backend.
struct CodeExecution {};

/// @brief Tool that enables the model to ground its responses using Google
/// Maps.
struct GoogleMaps {};

/// @brief Tool that enables the model to retrieve and ground responses on
/// public web URLs provided in the prompt.
struct UrlContext {};

/// @brief A helper tool that the model may use when generating responses, such
/// as function declarations, Google Search grounding, Code Execution, Google
/// Maps, or URL Context.
///
/// Mirrors `Firebase.AI.Tool` in Unity and `Tool` in Flutter.
class Tool {
 public:
  /// @brief Default constructor creates an empty Tool.
  Tool() {}

  /// @brief Construct a Tool containing function declarations.
  ///
  /// @param function_declarations The functions to expose to the model.
  explicit Tool(const std::vector<FunctionDeclaration>& function_declarations)
      : function_declarations_(function_declarations) {}

  /// @brief Construct a Tool containing a single function declaration.
  ///
  /// @param function_declaration The function to expose to the model.
  explicit Tool(const FunctionDeclaration& function_declaration)
      : function_declarations_(1, function_declaration) {}

  /// @brief Construct a Tool enabling Google Search grounding.
  explicit Tool(const GoogleSearch& google_search)
      : google_search_(google_search) {}

  /// @brief Construct a Tool enabling backend Code Execution.
  explicit Tool(const CodeExecution& code_execution)
      : code_execution_(code_execution) {}

  /// @brief Construct a Tool enabling Google Maps grounding.
  explicit Tool(const GoogleMaps& google_maps) : google_maps_(google_maps) {}

  /// @brief Construct a Tool enabling URL Context grounding.
  explicit Tool(const UrlContext& url_context) : url_context_(url_context) {}

  /// @brief Static factory creating a Tool with function declarations.
  static Tool FunctionDeclarations(
      const std::vector<FunctionDeclaration>& declarations) {
    return Tool(declarations);
  }

  /// @brief Returns the function declarations in this tool.
  const std::vector<FunctionDeclaration>& function_declarations() const {
    return function_declarations_;
  }

  /// @brief Returns the optional Google Search configuration.
  const Optional<GoogleSearch>& google_search() const { return google_search_; }

  /// @brief Returns the optional Code Execution configuration.
  const Optional<CodeExecution>& code_execution() const {
    return code_execution_;
  }

  /// @brief Returns the optional Google Maps configuration.
  const Optional<GoogleMaps>& google_maps() const { return google_maps_; }

  /// @brief Returns the optional URL Context configuration.
  const Optional<UrlContext>& url_context() const { return url_context_; }

 private:
  std::vector<FunctionDeclaration> function_declarations_;
  Optional<GoogleSearch> google_search_;
  Optional<CodeExecution> code_execution_;
  Optional<GoogleMaps> google_maps_;
  Optional<UrlContext> url_context_;
};

/// @brief Controls how the model uses the provided `FunctionDeclaration` tools.
class FunctionCallingConfig {
 public:
  /// @brief Execution mode for function calling.
  enum Mode {
    /// Mode is unspecified.
    kModeUnspecified = 0,
    /// Model decides whether to predict a function call or a natural language
    /// response (default).
    kModeAuto,
    /// Model is constrained to always predict a function call.
    kModeAny,
    /// Model will not predict any function call.
    kModeNone,
  };

  /// @brief Default constructor initializes to `kModeAuto`.
  FunctionCallingConfig() : mode_(kModeAuto) {}

  /// @brief Creates a `FunctionCallingConfig` with mode `kModeAuto`.
  static FunctionCallingConfig Auto() {
    return FunctionCallingConfig(kModeAuto, std::vector<std::string>());
  }

  /// @brief Creates a `FunctionCallingConfig` with mode `kModeAny`.
  ///
  /// @param allowed_function_names Optional list of function names the model is
  /// allowed to call. If empty, the model may call any declared function.
  static FunctionCallingConfig Any(
      const std::vector<std::string>& allowed_function_names =
          std::vector<std::string>()) {
    return FunctionCallingConfig(kModeAny, allowed_function_names);
  }

  /// @brief Creates a `FunctionCallingConfig` with mode `kModeNone`.
  static FunctionCallingConfig None() {
    return FunctionCallingConfig(kModeNone, std::vector<std::string>());
  }

  /// @brief Returns the function calling mode.
  Mode mode() const { return mode_; }

  /// @brief Returns the list of allowed function names (for `kModeAny`).
  const std::vector<std::string>& allowed_function_names() const {
    return allowed_function_names_;
  }

 private:
  FunctionCallingConfig(Mode mode,
                        const std::vector<std::string>& allowed_function_names)
      : mode_(mode), allowed_function_names_(allowed_function_names) {}

  Mode mode_;
  std::vector<std::string> allowed_function_names_;
};

/// @brief Configuration for tools provided to the model.
class ToolConfig {
 public:
  /// @brief Default constructor.
  ToolConfig() {}

  /// @brief Construct a `ToolConfig` with function calling and/or retrieval
  /// configuration.
  ///
  /// @param function_calling_config Optional function calling configuration.
  /// @param retrieval_config Optional retrieval configuration (e.g. for Google
  /// Maps).
  explicit ToolConfig(
      const Optional<FunctionCallingConfig>& function_calling_config,
      const Optional<RetrievalConfig>& retrieval_config =
          Optional<RetrievalConfig>())
      : function_calling_config_(function_calling_config),
        retrieval_config_(retrieval_config) {}

  /// @brief Returns the optional `FunctionCallingConfig`.
  const Optional<FunctionCallingConfig>& function_calling_config() const {
    return function_calling_config_;
  }
  /// @brief Sets the `FunctionCallingConfig`.
  void set_function_calling_config(const FunctionCallingConfig& config) {
    function_calling_config_ = config;
  }

  /// @brief Returns the optional `RetrievalConfig`.
  const Optional<RetrievalConfig>& retrieval_config() const {
    return retrieval_config_;
  }
  /// @brief Sets the `RetrievalConfig`.
  void set_retrieval_config(const RetrievalConfig& config) {
    retrieval_config_ = config;
  }

 private:
  Optional<FunctionCallingConfig> function_calling_config_;
  Optional<RetrievalConfig> retrieval_config_;
};

}  // namespace ai
}  // namespace firebase

#endif  // FIREBASE_AI_SRC_INCLUDE_FIREBASE_AI_FUNCTION_CALLING_H_
