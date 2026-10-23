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

#include "ai/src/common/serialization.h"

#include <sstream>

#include "app/src/base64.h"
#include "app/src/log.h"
#include "app/src/variant_util.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

const Variant* FindField(const Variant& map_var, const char* key) {
  if (!map_var.is_map()) return nullptr;
  auto it = map_var.map().find(Variant::FromStaticString(key));
  if (it == map_var.map().end()) return nullptr;
  return &it->second;
}

std::string GetStringField(const Variant& map_var, const char* key,
                           const std::string& default_val = "") {
  const Variant* v = FindField(map_var, key);
  if (v && v->is_string()) {
    return v->string_value();
  }
  return default_val;
}

int GetIntField(const Variant& map_var, const char* key, int default_val = 0) {
  const Variant* v = FindField(map_var, key);
  if (!v) return default_val;
  if (v->is_int64()) return static_cast<int>(v->int64_value());
  if (v->is_double()) return static_cast<int>(v->double_value());
  return default_val;
}

float GetFloatField(const Variant& map_var, const char* key,
                    float default_val = 0.0f) {
  const Variant* v = FindField(map_var, key);
  if (!v) return default_val;
  if (v->is_double()) return static_cast<float>(v->double_value());
  if (v->is_int64()) return static_cast<float>(v->int64_value());
  return default_val;
}

bool GetBoolField(const Variant& map_var, const char* key,
                  bool default_val = false) {
  const Variant* v = FindField(map_var, key);
  if (v && v->is_bool()) {
    return v->bool_value();
  }
  return default_val;
}

const char* SchemaTypeToOpenApiString(SchemaType type) {
  switch (type) {
    case kSchemaTypeString:
      return "STRING";
    case kSchemaTypeNumber:
      return "NUMBER";
    case kSchemaTypeInteger:
      return "INTEGER";
    case kSchemaTypeBoolean:
      return "BOOLEAN";
    case kSchemaTypeArray:
      return "ARRAY";
    case kSchemaTypeObject:
      return "OBJECT";
    case kSchemaTypeUnspecified:
    default:
      return "";
  }
}

const char* SchemaTypeToJsonSchemaString(SchemaType type) {
  switch (type) {
    case kSchemaTypeString:
      return "string";
    case kSchemaTypeNumber:
      return "number";
    case kSchemaTypeInteger:
      return "integer";
    case kSchemaTypeBoolean:
      return "boolean";
    case kSchemaTypeArray:
      return "array";
    case kSchemaTypeObject:
      return "object";
    case kSchemaTypeUnspecified:
    default:
      return "";
  }
}

const char* HarmCategoryToString(HarmCategory category) {
  switch (category) {
    case kHarmCategoryHarassment:
      return "HARM_CATEGORY_HARASSMENT";
    case kHarmCategoryHateSpeech:
      return "HARM_CATEGORY_HATE_SPEECH";
    case kHarmCategorySexuallyExplicit:
      return "HARM_CATEGORY_SEXUALLY_EXPLICIT";
    case kHarmCategoryDangerousContent:
      return "HARM_CATEGORY_DANGEROUS_CONTENT";
    case kHarmCategoryCivicIntegrity:
      return "HARM_CATEGORY_CIVIC_INTEGRITY";
    case kHarmCategoryUnknown:
    default:
      return "HARM_CATEGORY_UNSPECIFIED";
  }
}

HarmCategory ParseHarmCategory(const std::string& str) {
  if (str == "HARM_CATEGORY_HARASSMENT") return kHarmCategoryHarassment;
  if (str == "HARM_CATEGORY_HATE_SPEECH") return kHarmCategoryHateSpeech;
  if (str == "HARM_CATEGORY_SEXUALLY_EXPLICIT")
    return kHarmCategorySexuallyExplicit;
  if (str == "HARM_CATEGORY_DANGEROUS_CONTENT")
    return kHarmCategoryDangerousContent;
  if (str == "HARM_CATEGORY_CIVIC_INTEGRITY")
    return kHarmCategoryCivicIntegrity;
  return kHarmCategoryUnknown;
}

const char* HarmBlockThresholdToString(HarmBlockThreshold threshold) {
  switch (threshold) {
    case kHarmBlockThresholdLowAndAbove:
      return "BLOCK_LOW_AND_ABOVE";
    case kHarmBlockThresholdMediumAndAbove:
      return "BLOCK_MEDIUM_AND_ABOVE";
    case kHarmBlockThresholdOnlyHigh:
      return "BLOCK_ONLY_HIGH";
    case kHarmBlockThresholdNone:
      return "BLOCK_NONE";
    case kHarmBlockThresholdOff:
      return "OFF";
    case kHarmBlockThresholdUnknown:
    default:
      return "HARM_BLOCK_THRESHOLD_UNSPECIFIED";
  }
}

const char* HarmBlockMethodToString(HarmBlockMethod method) {
  switch (method) {
    case kHarmBlockMethodSeverity:
      return "SEVERITY";
    case kHarmBlockMethodProbability:
      return "PROBABILITY";
    case kHarmBlockMethodUnknown:
    default:
      return "HARM_BLOCK_METHOD_UNSPECIFIED";
  }
}

HarmProbability ParseHarmProbability(const std::string& str) {
  if (str == "NEGLIGIBLE") return kHarmProbabilityNegligible;
  if (str == "LOW") return kHarmProbabilityLow;
  if (str == "MEDIUM") return kHarmProbabilityMedium;
  if (str == "HIGH") return kHarmProbabilityHigh;
  return kHarmProbabilityUnknown;
}

HarmSeverity ParseHarmSeverity(const std::string& str) {
  if (str == "HARM_SEVERITY_NEGLIGIBLE") return kHarmSeverityNegligible;
  if (str == "HARM_SEVERITY_LOW") return kHarmSeverityLow;
  if (str == "HARM_SEVERITY_MEDIUM") return kHarmSeverityMedium;
  if (str == "HARM_SEVERITY_HIGH") return kHarmSeverityHigh;
  return kHarmSeverityUnknown;
}

FinishReason ParseFinishReason(const std::string& str) {
  if (str == "STOP") return kFinishReasonStop;
  if (str == "MAX_TOKENS") return kFinishReasonMaxTokens;
  if (str == "SAFETY") return kFinishReasonSafety;
  if (str == "RECITATION") return kFinishReasonRecitation;
  if (str == "OTHER") return kFinishReasonOther;
  if (str == "BLOCKLIST") return kFinishReasonBlocklist;
  if (str == "PROHIBITED_CONTENT") return kFinishReasonProhibitedContent;
  if (str == "SPII") return kFinishReasonSpii;
  if (str == "MALFORMED_FUNCTION_CALL")
    return kFinishReasonMalformedFunctionCall;
  return kFinishReasonUnknown;
}

BlockReason ParseBlockReason(const std::string& str) {
  if (str == "SAFETY") return kBlockReasonSafety;
  if (str == "OTHER") return kBlockReasonOther;
  if (str == "BLOCKLIST") return kBlockReasonBlocklist;
  if (str == "PROHIBITED_CONTENT") return kBlockReasonProhibitedContent;
  return kBlockReasonUnknown;
}

ContentModality ParseContentModality(const std::string& str) {
  if (str == "TEXT") return kContentModalityText;
  if (str == "IMAGE") return kContentModalityImage;
  if (str == "VIDEO") return kContentModalityVideo;
  if (str == "AUDIO") return kContentModalityAudio;
  if (str == "DOCUMENT") return kContentModalityDocument;
  return kContentModalityUnspecified;
}

const char* ResponseModalityToString(ResponseModality modality) {
  switch (modality) {
    case kResponseModalityText:
      return "TEXT";
    case kResponseModalityImage:
      return "IMAGE";
    case kResponseModalityAudio:
      return "AUDIO";
    case kResponseModalityUnspecified:
    default:
      return "MODALITY_UNSPECIFIED";
  }
}

const char* ThinkingLevelToString(ThinkingLevel level) {
  switch (level) {
    case kThinkingLevelMinimal:
      return "MINIMAL";
    case kThinkingLevelLow:
      return "LOW";
    case kThinkingLevelMedium:
      return "MEDIUM";
    case kThinkingLevelHigh:
      return "HIGH";
    case kThinkingLevelUnspecified:
    default:
      return "THINKING_LEVEL_UNSPECIFIED";
  }
}

CodeExecutionOutcome ParseCodeExecutionOutcome(const std::string& str) {
  if (str == "OUTCOME_OK") return kCodeExecutionOutcomeOk;
  if (str == "OUTCOME_FAILED") return kCodeExecutionOutcomeFailed;
  if (str == "OUTCOME_DEADLINE_EXCEEDED")
    return kCodeExecutionOutcomeDeadlineExceeded;
  return kCodeExecutionOutcomeUnspecified;
}

const char* CodeExecutionOutcomeToString(CodeExecutionOutcome outcome) {
  switch (outcome) {
    case kCodeExecutionOutcomeOk:
      return "OUTCOME_OK";
    case kCodeExecutionOutcomeFailed:
      return "OUTCOME_FAILED";
    case kCodeExecutionOutcomeDeadlineExceeded:
      return "OUTCOME_DEADLINE_EXCEEDED";
    case kCodeExecutionOutcomeUnspecified:
    default:
      return "OUTCOME_UNSPECIFIED";
  }
}

UrlRetrievalStatus ParseUrlRetrievalStatus(const std::string& str) {
  if (str == "URL_RETRIEVAL_STATUS_SUCCESS") return kUrlRetrievalStatusSuccess;
  if (str == "URL_RETRIEVAL_STATUS_ERROR") return kUrlRetrievalStatusError;
  if (str == "URL_RETRIEVAL_STATUS_PAYWALL") return kUrlRetrievalStatusPaywall;
  if (str == "URL_RETRIEVAL_STATUS_UNSAFE") return kUrlRetrievalStatusUnsafe;
  return kUrlRetrievalStatusUnspecified;
}

std::vector<ModalityTokenCount> ParseModalityTokenCounts(
    const Variant* list_var) {
  std::vector<ModalityTokenCount> result;
  if (!list_var || !list_var->is_vector()) return result;
  for (const auto& elem : list_var->vector()) {
    if (!elem.is_map()) continue;
    ModalityTokenCount mtc;
    mtc.modality = ParseContentModality(GetStringField(elem, "modality"));
    mtc.token_count = GetIntField(elem, "tokenCount");
    result.push_back(mtc);
  }
  return result;
}

CitationMetadata ParseCitationMetadata(const Variant& map_var,
                                       BackendProvider provider) {
  CitationMetadata metadata;
  const char* key =
      (provider == kBackendProviderGoogleAI) ? "citationSources" : "citations";
  const Variant* citations_var = FindField(map_var, key);
  if (!citations_var) {
    // Fallback to the other key in case backend format varies.
    citations_var = FindField(map_var, (provider == kBackendProviderGoogleAI)
                                           ? "citations"
                                           : "citationSources");
  }
  if (citations_var && citations_var->is_vector()) {
    for (const auto& item : citations_var->vector()) {
      if (!item.is_map()) continue;
      Citation c;
      c.start_index = GetIntField(item, "startIndex");
      c.end_index = GetIntField(item, "endIndex");
      c.uri = GetStringField(item, "uri");
      c.title = GetStringField(item, "title");
      c.license = GetStringField(item, "license");
      const Variant* pub_date = FindField(item, "publicationDate");
      if (pub_date && pub_date->is_string()) {
        c.publication_date = pub_date->string_value();
      } else if (pub_date && pub_date->is_map()) {
        int year = GetIntField(*pub_date, "year");
        int month = GetIntField(*pub_date, "month");
        int day = GetIntField(*pub_date, "day");
        std::ostringstream oss;
        if (year > 0) oss << year;
        if (month > 0) oss << "-" << month;
        if (day > 0) oss << "-" << day;
        c.publication_date = oss.str();
      }
      metadata.citations.push_back(c);
    }
  }
  return metadata;
}

GroundingMetadata ParseGroundingMetadata(const Variant& map_var) {
  GroundingMetadata gm;
  const Variant* queries = FindField(map_var, "webSearchQueries");
  if (queries && queries->is_vector()) {
    for (const auto& q : queries->vector()) {
      if (q.is_string()) gm.web_search_queries.push_back(q.string_value());
    }
  }

  const Variant* sep = FindField(map_var, "searchEntryPoint");
  if (sep && sep->is_map()) {
    SearchEntryPoint entry;
    entry.rendered_content = GetStringField(*sep, "renderedContent");
    entry.sdk_blob = GetStringField(*sep, "sdkBlob");
    gm.search_entry_point = entry;
  }

  const Variant* chunks = FindField(map_var, "groundingChunks");
  if (chunks && chunks->is_vector()) {
    for (const auto& ch : chunks->vector()) {
      if (!ch.is_map()) continue;
      GroundingChunk gc;
      const Variant* web = FindField(ch, "web");
      if (web && web->is_map()) {
        WebGroundingChunk wgc;
        wgc.uri = GetStringField(*web, "uri");
        wgc.title = GetStringField(*web, "title");
        wgc.domain = GetStringField(*web, "domain");
        gc.web = wgc;
      }
      const Variant* maps = FindField(ch, "maps");
      if (maps && maps->is_map()) {
        GoogleMapsGroundingChunk mgc;
        mgc.uri = GetStringField(*maps, "uri");
        mgc.title = GetStringField(*maps, "title");
        mgc.place_id = GetStringField(*maps, "placeId");
        gc.maps = mgc;
      }
      gm.grounding_chunks.push_back(gc);
    }
  }

  const Variant* supports = FindField(map_var, "groundingSupports");
  if (supports && supports->is_vector()) {
    for (const auto& sup : supports->vector()) {
      if (!sup.is_map()) continue;
      GroundingSupport gs;
      const Variant* seg = FindField(sup, "segment");
      if (seg && seg->is_map()) {
        gs.segment.part_index = GetIntField(*seg, "partIndex");
        gs.segment.start_index = GetIntField(*seg, "startIndex");
        gs.segment.end_index = GetIntField(*seg, "endIndex");
        gs.segment.text = GetStringField(*seg, "text");
      }
      const Variant* indices = FindField(sup, "groundingChunkIndices");
      if (indices && indices->is_vector()) {
        for (const auto& idx : indices->vector()) {
          if (idx.is_int64()) {
            gs.grounding_chunk_indices.push_back(
                static_cast<int>(idx.int64_value()));
          }
        }
      }
      gm.grounding_supports.push_back(gs);
    }
  }

  const Variant* widget_token =
      FindField(map_var, "googleMapsWidgetContextToken");
  if (widget_token && widget_token->is_string()) {
    gm.google_maps_widget_context_token =
        std::string(widget_token->string_value());
  }
  return gm;
}

UrlContextMetadata ParseUrlContextMetadata(const Variant& map_var) {
  UrlContextMetadata ucm;
  const Variant* list_var = FindField(map_var, "urlMetadata");
  if (list_var && list_var->is_vector()) {
    for (const auto& elem : list_var->vector()) {
      if (!elem.is_map()) continue;
      UrlMetadata um;
      um.retrieved_url = GetStringField(elem, "retrievedUrl");
      um.retrieval_status =
          ParseUrlRetrievalStatus(GetStringField(elem, "urlRetrievalStatus"));
      ucm.url_metadata.push_back(um);
    }
  }
  return ucm;
}

}  // namespace

Variant SchemaToVariant(const Schema& schema) {
  Variant map = Variant::EmptyMap();
  const char* type_str = SchemaTypeToOpenApiString(schema.type());
  if (type_str[0] != '\0') {
    map.map()["type"] = type_str;
  }
  if (schema.description().has_value()) {
    map.map()["description"] = schema.description().value();
  }
  if (schema.format().has_value()) {
    map.map()["format"] = schema.format().value();
  }
  if (schema.nullable().has_value()) {
    map.map()["nullable"] = schema.nullable().value();
  }
  if (!schema.enum_values().empty()) {
    Variant enums = Variant::EmptyVector();
    for (const auto& val : schema.enum_values()) {
      enums.vector().push_back(val);
    }
    map.map()["enum"] = enums;
  }
  if (!schema.properties().empty()) {
    Variant props = Variant::EmptyMap();
    for (const auto& kv : schema.properties()) {
      props.map()[kv.first] = SchemaToVariant(kv.second);
    }
    map.map()["properties"] = props;
  }
  if (!schema.required_properties().empty()) {
    Variant req = Variant::EmptyVector();
    for (const auto& r : schema.required_properties()) {
      req.vector().push_back(r);
    }
    map.map()["required"] = req;
  }
  if (!schema.property_ordering().empty()) {
    Variant ord = Variant::EmptyVector();
    for (const auto& p : schema.property_ordering()) {
      ord.vector().push_back(p);
    }
    map.map()["propertyOrdering"] = ord;
  }
  if (schema.items() != nullptr) {
    map.map()["items"] = SchemaToVariant(*schema.items());
  }
  if (schema.title().has_value()) {
    map.map()["title"] = schema.title().value();
  }
  if (schema.min_items().has_value()) {
    map.map()["minItems"] = schema.min_items().value();
  }
  if (schema.max_items().has_value()) {
    map.map()["maxItems"] = schema.max_items().value();
  }
  if (schema.minimum().has_value()) {
    map.map()["minimum"] = schema.minimum().value();
  }
  if (schema.maximum().has_value()) {
    map.map()["maximum"] = schema.maximum().value();
  }
  if (!schema.any_of().empty()) {
    Variant any_of_vec = Variant::EmptyVector();
    for (const auto& sub : schema.any_of()) {
      any_of_vec.vector().push_back(SchemaToVariant(sub));
    }
    map.map()["anyOf"] = any_of_vec;
  }
  return map;
}

Variant JsonSchemaToVariant(const JsonSchema& schema) {
  Variant map = Variant::EmptyMap();
  const char* type_str = SchemaTypeToJsonSchemaString(schema.type());
  bool is_nullable = schema.nullable().value_or(false);
  if (type_str[0] != '\0') {
    if (is_nullable) {
      Variant types = Variant::EmptyVector();
      types.vector().push_back(type_str);
      types.vector().push_back("null");
      map.map()["type"] = types;
    } else {
      map.map()["type"] = type_str;
    }
  }
  if (schema.description().has_value()) {
    map.map()["description"] = schema.description().value();
  }
  if (schema.format().has_value()) {
    map.map()["format"] = schema.format().value();
  }
  if (!schema.enum_values().empty()) {
    Variant enums = Variant::EmptyVector();
    for (const auto& val : schema.enum_values()) {
      enums.vector().push_back(val);
    }
    map.map()["enum"] = enums;
  }
  if (!schema.properties().empty()) {
    Variant props = Variant::EmptyMap();
    for (const auto& kv : schema.properties()) {
      props.map()[kv.first] = JsonSchemaToVariant(kv.second);
    }
    map.map()["properties"] = props;
  }
  if (!schema.required_properties().empty()) {
    Variant req = Variant::EmptyVector();
    for (const auto& r : schema.required_properties()) {
      req.vector().push_back(r);
    }
    map.map()["required"] = req;
  }
  if (schema.items() != nullptr) {
    map.map()["items"] = JsonSchemaToVariant(*schema.items());
  }
  if (schema.title().has_value()) {
    map.map()["title"] = schema.title().value();
  }
  if (schema.min_items().has_value()) {
    map.map()["minItems"] = schema.min_items().value();
  }
  if (schema.max_items().has_value()) {
    map.map()["maxItems"] = schema.max_items().value();
  }
  if (schema.minimum().has_value()) {
    map.map()["minimum"] = schema.minimum().value();
  }
  if (schema.maximum().has_value()) {
    map.map()["maximum"] = schema.maximum().value();
  }
  if (!schema.any_of().empty()) {
    Variant any_of_vec = Variant::EmptyVector();
    for (const auto& sub : schema.any_of()) {
      any_of_vec.vector().push_back(JsonSchemaToVariant(sub));
    }
    if (is_nullable && type_str[0] == '\0') {
      Variant null_type = Variant::EmptyMap();
      null_type.map()["type"] = "null";
      any_of_vec.vector().push_back(null_type);
    }
    map.map()["anyOf"] = any_of_vec;
  }
  return map;
}

Variant PartToVariant(const Part& part) {
  Variant map = Variant::EmptyMap();
  switch (part.kind()) {
    case Part::kKindText: {
      map.map()["text"] = part.text_part().text;
      break;
    }
    case Part::kKindInlineData: {
      Variant inline_data = Variant::EmptyMap();
      inline_data.map()["mimeType"] = part.inline_data_part().mime_type;
      std::string raw(
          reinterpret_cast<const char*>(part.inline_data_part().data.data()),
          part.inline_data_part().data.size());
      std::string encoded;
      ::firebase::internal::Base64EncodeWithPadding(raw, &encoded);
      inline_data.map()["data"] = encoded;
      map.map()["inlineData"] = inline_data;
      break;
    }
    case Part::kKindFileData: {
      Variant file_data = Variant::EmptyMap();
      file_data.map()["mimeType"] = part.file_data_part().mime_type;
      file_data.map()["fileUri"] = part.file_data_part().uri;
      map.map()["fileData"] = file_data;
      break;
    }
    case Part::kKindFunctionCall: {
      Variant fc = Variant::EmptyMap();
      fc.map()["name"] = part.function_call_part().name;
      Variant args = Variant::EmptyMap();
      for (const auto& kv : part.function_call_part().args) {
        args.map()[kv.first] = kv.second;
      }
      fc.map()["args"] = args;
      if (part.function_call_part().id.has_value()) {
        fc.map()["id"] = part.function_call_part().id.value();
      }
      map.map()["functionCall"] = fc;
      break;
    }
    case Part::kKindFunctionResponse: {
      Variant fr = Variant::EmptyMap();
      fr.map()["name"] = part.function_response_part().name;
      Variant resp = Variant::EmptyMap();
      for (const auto& kv : part.function_response_part().response) {
        resp.map()[kv.first] = kv.second;
      }
      fr.map()["response"] = resp;
      if (part.function_response_part().id.has_value()) {
        fr.map()["id"] = part.function_response_part().id.value();
      }
      map.map()["functionResponse"] = fr;
      break;
    }
    case Part::kKindExecutableCode: {
      Variant ec = Variant::EmptyMap();
      ec.map()["language"] = (part.executable_code_part().language ==
                              ExecutableCodePart::kLanguagePython)
                                 ? "PYTHON"
                                 : "LANGUAGE_UNSPECIFIED";
      ec.map()["code"] = part.executable_code_part().code;
      map.map()["executableCode"] = ec;
      break;
    }
    case Part::kKindCodeExecutionResult: {
      Variant cer = Variant::EmptyMap();
      cer.map()["outcome"] = CodeExecutionOutcomeToString(
          part.code_execution_result_part().outcome);
      cer.map()["output"] = part.code_execution_result_part().output;
      map.map()["codeExecutionResult"] = cer;
      break;
    }
    case Part::kKindNone:
    default:
      break;
  }

  if (part.is_thought()) {
    map.map()["thought"] = true;
  }
  if (part.thought_signature().has_value()) {
    map.map()["thoughtSignature"] = part.thought_signature().value();
  }
  return map;
}

bool PartFromVariant(const Variant& variant, Part* out_part) {
  if (!variant.is_map() || !out_part) return false;

  bool is_thought = GetBoolField(variant, "thought", false);
  Optional<std::string> thought_sig;
  const Variant* sig_var = FindField(variant, "thoughtSignature");
  if (sig_var && sig_var->is_string()) {
    thought_sig = std::string(sig_var->string_value());
  }

  const Variant* text_var = FindField(variant, "text");
  if (text_var && text_var->is_string()) {
    *out_part =
        Part(TextPart(text_var->string_value()), is_thought, thought_sig);
    return true;
  }

  const Variant* inline_var = FindField(variant, "inlineData");
  if (inline_var && inline_var->is_map()) {
    std::string mime_type = GetStringField(*inline_var, "mimeType");
    std::string base64_data = GetStringField(*inline_var, "data");
    std::string decoded;
    ::firebase::internal::Base64Decode(base64_data, &decoded);
    std::vector<uint8_t> bytes(decoded.begin(), decoded.end());
    *out_part = Part(InlineDataPart(mime_type, bytes), is_thought, thought_sig);
    return true;
  }

  const Variant* file_var = FindField(variant, "fileData");
  if (file_var && file_var->is_map()) {
    std::string mime_type = GetStringField(*file_var, "mimeType");
    std::string file_uri = GetStringField(*file_var, "fileUri");
    *out_part =
        Part(FileDataPart(mime_type, file_uri), is_thought, thought_sig);
    return true;
  }

  const Variant* fc_var = FindField(variant, "functionCall");
  if (fc_var && fc_var->is_map()) {
    std::string name = GetStringField(*fc_var, "name");
    std::map<std::string, Variant> args;
    const Variant* args_var = FindField(*fc_var, "args");
    if (args_var && args_var->is_map()) {
      for (const auto& kv : args_var->map()) {
        if (kv.first.is_string()) {
          args[kv.first.string_value()] = kv.second;
        }
      }
    }
    Optional<std::string> id;
    const Variant* id_var = FindField(*fc_var, "id");
    if (id_var && id_var->is_string()) {
      id = std::string(id_var->string_value());
    }
    *out_part = Part(FunctionCallPart(name, args, id), is_thought, thought_sig);
    return true;
  }

  const Variant* fr_var = FindField(variant, "functionResponse");
  if (fr_var && fr_var->is_map()) {
    std::string name = GetStringField(*fr_var, "name");
    std::map<std::string, Variant> resp;
    const Variant* resp_var = FindField(*fr_var, "response");
    if (resp_var && resp_var->is_map()) {
      for (const auto& kv : resp_var->map()) {
        if (kv.first.is_string()) {
          resp[kv.first.string_value()] = kv.second;
        }
      }
    }
    Optional<std::string> id;
    const Variant* id_var = FindField(*fr_var, "id");
    if (id_var && id_var->is_string()) {
      id = std::string(id_var->string_value());
    }
    *out_part =
        Part(FunctionResponsePart(name, resp, id), is_thought, thought_sig);
    return true;
  }

  const Variant* ec_var = FindField(variant, "executableCode");
  if (ec_var && ec_var->is_map()) {
    std::string lang_str = GetStringField(*ec_var, "language");
    ExecutableCodePart::CodeLanguage lang =
        (lang_str == "PYTHON") ? ExecutableCodePart::kLanguagePython
                               : ExecutableCodePart::kLanguageUnspecified;
    std::string code = GetStringField(*ec_var, "code");
    *out_part = Part(ExecutableCodePart(lang, code), is_thought, thought_sig);
    return true;
  }

  const Variant* cer_var = FindField(variant, "codeExecutionResult");
  if (cer_var && cer_var->is_map()) {
    CodeExecutionOutcome outcome =
        ParseCodeExecutionOutcome(GetStringField(*cer_var, "outcome"));
    std::string output = GetStringField(*cer_var, "output");
    *out_part =
        Part(CodeExecutionResultPart(outcome, output), is_thought, thought_sig);
    return true;
  }

  return false;
}

Variant ModelContentToVariant(const ModelContent& content) {
  Variant map = Variant::EmptyMap();
  map.map()["role"] = content.role().empty() ? "user" : content.role();
  Variant parts = Variant::EmptyVector();
  for (const auto& part : content.parts()) {
    parts.vector().push_back(PartToVariant(part));
  }
  map.map()["parts"] = parts;
  return map;
}

bool ModelContentFromVariant(const Variant& variant,
                             ModelContent* out_content) {
  if (!variant.is_map() || !out_content) return false;
  std::string role = GetStringField(variant, "role", "model");
  std::vector<Part> parts;
  const Variant* parts_var = FindField(variant, "parts");
  if (parts_var && parts_var->is_vector()) {
    for (const auto& part_var : parts_var->vector()) {
      Part p;
      if (PartFromVariant(part_var, &p)) {
        parts.push_back(p);
      }
    }
  }
  *out_content = ModelContent(role, parts);
  return true;
}

Variant SafetySettingToVariant(const SafetySetting& setting,
                               BackendProvider provider) {
  Variant map = Variant::EmptyMap();
  map.map()["category"] = HarmCategoryToString(setting.category());
  map.map()["threshold"] = HarmBlockThresholdToString(setting.threshold());
  if (provider == kBackendProviderEnterprise && setting.method().has_value()) {
    map.map()["method"] = HarmBlockMethodToString(setting.method().value());
  }
  return map;
}

SafetyRating SafetyRatingFromVariant(const Variant& variant) {
  SafetyRating rating;
  if (!variant.is_map()) return rating;
  rating.category = ParseHarmCategory(GetStringField(variant, "category"));
  rating.probability =
      ParseHarmProbability(GetStringField(variant, "probability"));
  rating.blocked = GetBoolField(variant, "blocked", false);
  rating.probability_score = GetFloatField(variant, "probabilityScore", 0.0f);
  rating.severity = ParseHarmSeverity(GetStringField(variant, "severity"));
  rating.severity_score = GetFloatField(variant, "severityScore", 0.0f);
  return rating;
}

Variant GenerationConfigToVariant(const GenerationConfig& config) {
  Variant map = Variant::EmptyMap();
  if (config.temperature.has_value()) {
    map.map()["temperature"] = static_cast<double>(config.temperature.value());
  }
  if (config.top_p.has_value()) {
    map.map()["topP"] = static_cast<double>(config.top_p.value());
  }
  if (config.top_k.has_value()) {
    map.map()["topK"] = config.top_k.value();
  }
  if (config.candidate_count.has_value()) {
    map.map()["candidateCount"] = config.candidate_count.value();
  }
  if (config.max_output_tokens.has_value()) {
    map.map()["maxOutputTokens"] = config.max_output_tokens.value();
  }
  if (config.presence_penalty.has_value()) {
    map.map()["presencePenalty"] =
        static_cast<double>(config.presence_penalty.value());
  }
  if (config.frequency_penalty.has_value()) {
    map.map()["frequencyPenalty"] =
        static_cast<double>(config.frequency_penalty.value());
  }
  if (!config.stop_sequences.empty()) {
    Variant stops = Variant::EmptyVector();
    for (const auto& s : config.stop_sequences) {
      stops.vector().push_back(s);
    }
    map.map()["stopSequences"] = stops;
  }
  if (config.response_mime_type.has_value()) {
    map.map()["responseMimeType"] = config.response_mime_type.value();
  }
  if (config.response_schema.has_value()) {
    map.map()["responseSchema"] =
        SchemaToVariant(config.response_schema.value());
  }
  if (config.response_json_schema.has_value()) {
    map.map()["responseJsonSchema"] =
        JsonSchemaToVariant(config.response_json_schema.value());
  }
  if (!config.response_modalities.empty()) {
    Variant mods = Variant::EmptyVector();
    for (const auto& m : config.response_modalities) {
      mods.vector().push_back(ResponseModalityToString(m));
    }
    map.map()["responseModalities"] = mods;
  }
  if (config.thinking_config.has_value()) {
    const ThinkingConfig& tc = config.thinking_config.value();
    Variant tc_map = Variant::EmptyMap();
    if (tc.thinking_budget.has_value()) {
      tc_map.map()["thinkingBudget"] = tc.thinking_budget.value();
    }
    if (tc.thinking_level.has_value()) {
      tc_map.map()["thinkingLevel"] =
          ThinkingLevelToString(tc.thinking_level.value());
    }
    if (tc.include_thoughts.has_value()) {
      tc_map.map()["includeThoughts"] = tc.include_thoughts.value();
    }
    if (!tc_map.map().empty()) {
      map.map()["thinkingConfig"] = tc_map;
    }
  }
  if (config.image_config.has_value()) {
    const ImageConfig& ic = config.image_config.value();
    Variant ic_map = Variant::EmptyMap();
    if (ic.aspect_ratio.has_value()) {
      ic_map.map()["aspectRatio"] = ic.aspect_ratio.value();
    }
    if (ic.image_size.has_value()) {
      ic_map.map()["imageSize"] = ic.image_size.value();
    }
    if (!ic_map.map().empty()) {
      map.map()["imageConfig"] = ic_map;
    }
  }
  return map;
}

Variant ToolToVariant(const Tool& tool) {
  Variant map = Variant::EmptyMap();
  if (!tool.function_declarations().empty()) {
    Variant decls = Variant::EmptyVector();
    for (const auto& fn : tool.function_declarations()) {
      Variant fn_map = Variant::EmptyMap();
      fn_map.map()["name"] = fn.name();
      if (!fn.description().empty()) {
        fn_map.map()["description"] = fn.description();
      }
      if (fn.uses_json_schema()) {
        fn_map.map()["parametersJsonSchema"] =
            JsonSchemaToVariant(fn.parameters());
      } else {
        fn_map.map()["parameters"] = SchemaToVariant(fn.parameters());
      }
      decls.vector().push_back(fn_map);
    }
    map.map()["functionDeclarations"] = decls;
  }
  if (tool.google_search().has_value()) {
    map.map()["googleSearch"] = Variant::EmptyMap();
  }
  if (tool.code_execution().has_value()) {
    map.map()["codeExecution"] = Variant::EmptyMap();
  }
  if (tool.google_maps().has_value()) {
    map.map()["googleMaps"] = Variant::EmptyMap();
  }
  if (tool.url_context().has_value()) {
    map.map()["urlContext"] = Variant::EmptyMap();
  }
  return map;
}

Variant ToolConfigToVariant(const ToolConfig& config) {
  Variant map = Variant::EmptyMap();
  if (config.function_calling_config().has_value()) {
    const FunctionCallingConfig& fcc = config.function_calling_config().value();
    Variant fcc_map = Variant::EmptyMap();
    switch (fcc.mode()) {
      case FunctionCallingConfig::kModeAuto:
        fcc_map.map()["mode"] = "AUTO";
        break;
      case FunctionCallingConfig::kModeAny:
        fcc_map.map()["mode"] = "ANY";
        break;
      case FunctionCallingConfig::kModeNone:
        fcc_map.map()["mode"] = "NONE";
        break;
      case FunctionCallingConfig::kModeUnspecified:
      default:
        break;
    }
    if (!fcc.allowed_function_names().empty()) {
      Variant names = Variant::EmptyVector();
      for (const auto& name : fcc.allowed_function_names()) {
        names.vector().push_back(name);
      }
      fcc_map.map()["allowedFunctionNames"] = names;
    }
    map.map()["functionCallingConfig"] = fcc_map;
  }
  if (config.retrieval_config().has_value()) {
    const RetrievalConfig& rc = config.retrieval_config().value();
    Variant rc_map = Variant::EmptyMap();
    if (rc.lat_lng.has_value()) {
      Variant ll = Variant::EmptyMap();
      ll.map()["latitude"] = rc.lat_lng.value().latitude;
      ll.map()["longitude"] = rc.lat_lng.value().longitude;
      rc_map.map()["latLng"] = ll;
    }
    if (rc.language_code.has_value()) {
      rc_map.map()["languageCode"] = rc.language_code.value();
    }
    map.map()["retrievalConfig"] = rc_map;
  }
  return map;
}

namespace {

Variant BuildGenerateContentRequestVariant(
    const std::vector<ModelContent>& contents,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction,
    BackendProvider provider) {
  Variant root = Variant::EmptyMap();

  Variant contents_vec = Variant::EmptyVector();
  for (const auto& c : contents) {
    contents_vec.vector().push_back(ModelContentToVariant(c));
  }
  root.map()["contents"] = contents_vec;

  if (generation_config.has_value()) {
    Variant gc = GenerationConfigToVariant(generation_config.value());
    if (!gc.map().empty()) {
      root.map()["generationConfig"] = gc;
    }
  }

  if (!safety_settings.empty()) {
    Variant settings_vec = Variant::EmptyVector();
    for (const auto& s : safety_settings) {
      settings_vec.vector().push_back(SafetySettingToVariant(s, provider));
    }
    root.map()["safetySettings"] = settings_vec;
  }

  if (!tools.empty()) {
    Variant tools_vec = Variant::EmptyVector();
    for (const auto& t : tools) {
      tools_vec.vector().push_back(ToolToVariant(t));
    }
    root.map()["tools"] = tools_vec;
  }

  if (tool_config.has_value()) {
    Variant tc = ToolConfigToVariant(tool_config.value());
    if (!tc.map().empty()) {
      root.map()["toolConfig"] = tc;
    }
  }

  if (system_instruction.has_value()) {
    root.map()["systemInstruction"] =
        ModelContentToVariant(system_instruction.value());
  }

  return root;
}

}  // namespace

std::string BuildGenerateContentRequestJson(
    const std::vector<ModelContent>& contents,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction,
    BackendProvider provider) {
  Variant root = BuildGenerateContentRequestVariant(
      contents, generation_config, safety_settings, tools, tool_config,
      system_instruction, provider);
  return ::firebase::util::VariantToJson(root);
}

std::string BuildCountTokensRequestJson(
    const std::string& model_name, const std::vector<ModelContent>& contents,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction,
    BackendProvider provider) {
  if (provider == kBackendProviderGoogleAI) {
    Variant inner = BuildGenerateContentRequestVariant(
        contents, generation_config, safety_settings, tools, tool_config,
        system_instruction, provider);
    const char kPrefix[] = "models/";
    std::string normalized =
        (model_name.compare(0, sizeof(kPrefix) - 1, kPrefix) == 0)
            ? model_name
            : (std::string(kPrefix) + model_name);
    inner.map()["model"] = normalized;
    Variant wrapper = Variant::EmptyMap();
    wrapper.map()["generateContentRequest"] = inner;
    return ::firebase::util::VariantToJson(wrapper);
  }

  Variant root = Variant::EmptyMap();
  Variant contents_vec = Variant::EmptyVector();
  for (const auto& c : contents) {
    contents_vec.vector().push_back(ModelContentToVariant(c));
  }
  root.map()["contents"] = contents_vec;
  if (generation_config.has_value()) {
    Variant gc = GenerationConfigToVariant(generation_config.value());
    if (!gc.map().empty()) {
      root.map()["generationConfig"] = gc;
    }
  }
  if (!tools.empty()) {
    Variant tools_vec = Variant::EmptyVector();
    for (const auto& t : tools) {
      tools_vec.vector().push_back(ToolToVariant(t));
    }
    root.map()["tools"] = tools_vec;
  }
  if (system_instruction.has_value()) {
    root.map()["systemInstruction"] =
        ModelContentToVariant(system_instruction.value());
  }
  return ::firebase::util::VariantToJson(root);
}

std::string BuildTemplateGenerateContentRequestJson(
    const std::map<std::string, Variant>& inputs,
    const std::vector<ModelContent>& history) {
  Variant root = Variant::EmptyMap();
  if (!inputs.empty()) {
    Variant inputs_map = Variant::EmptyMap();
    for (const auto& kv : inputs) {
      inputs_map.map()[kv.first] = kv.second;
    }
    root.map()["inputs"] = inputs_map;
  }
  if (!history.empty()) {
    Variant history_vec = Variant::EmptyVector();
    for (const auto& c : history) {
      history_vec.vector().push_back(ModelContentToVariant(c));
    }
    root.map()["history"] = history_vec;
  }
  return ::firebase::util::VariantToJson(root);
}

std::string BuildTemplateGenerateContentRequestFromRawJson(
    const std::string& json_inputs, const std::vector<ModelContent>& history) {
  Variant parsed_inputs = ::firebase::util::JsonToVariant(json_inputs.c_str());
  Variant root = Variant::EmptyMap();
  if (parsed_inputs.is_map()) {
    root.map()["inputs"] = parsed_inputs;
  } else {
    root.map()["inputs"] = Variant::EmptyMap();
  }
  if (!history.empty()) {
    Variant history_vec = Variant::EmptyVector();
    for (const auto& c : history) {
      history_vec.vector().push_back(ModelContentToVariant(c));
    }
    root.map()["history"] = history_vec;
  }
  return ::firebase::util::VariantToJson(root);
}

bool ParseGenerateContentResponseJson(const std::string& json,
                                      BackendProvider provider,
                                      GenerateContentResponse* out_response,
                                      std::string* out_error) {
  if (!out_response) return false;
  Variant root = ::firebase::util::JsonToVariant(json.c_str());
  if (!root.is_map()) {
    if (out_error) {
      *out_error = "Unable to parse GenerateContentResponse JSON object.";
    }
    return false;
  }

  std::vector<Candidate> candidates;
  const Variant* candidates_var = FindField(root, "candidates");
  if (candidates_var && candidates_var->is_vector()) {
    for (const auto& cand_var : candidates_var->vector()) {
      if (!cand_var.is_map()) continue;
      Candidate cand;
      const Variant* content_var = FindField(cand_var, "content");
      if (content_var && content_var->is_map()) {
        ModelContentFromVariant(*content_var, &cand.content);
      } else {
        cand.content = ModelContent("model", std::vector<Part>());
      }

      const Variant* safety_var = FindField(cand_var, "safetyRatings");
      if (safety_var && safety_var->is_vector()) {
        for (const auto& sr : safety_var->vector()) {
          cand.safety_ratings.push_back(SafetyRatingFromVariant(sr));
        }
      }

      const Variant* citation_var = FindField(cand_var, "citationMetadata");
      if (citation_var && citation_var->is_map()) {
        cand.citation_metadata = ParseCitationMetadata(*citation_var, provider);
      }

      const Variant* grounding_var = FindField(cand_var, "groundingMetadata");
      if (grounding_var && grounding_var->is_map()) {
        cand.grounding_metadata = ParseGroundingMetadata(*grounding_var);
      }

      const Variant* url_ctx_var = FindField(cand_var, "urlContextMetadata");
      if (url_ctx_var && url_ctx_var->is_map()) {
        cand.url_context_metadata = ParseUrlContextMetadata(*url_ctx_var);
      }

      cand.finish_reason =
          ParseFinishReason(GetStringField(cand_var, "finishReason"));
      cand.finish_message = GetStringField(cand_var, "finishMessage");
      candidates.push_back(cand);
    }
  }

  Optional<PromptFeedback> prompt_feedback;
  const Variant* pf_var = FindField(root, "promptFeedback");
  if (pf_var && pf_var->is_map()) {
    PromptFeedback pf;
    pf.block_reason = ParseBlockReason(GetStringField(*pf_var, "blockReason"));
    pf.block_reason_message = GetStringField(*pf_var, "blockReasonMessage");
    const Variant* sr_var = FindField(*pf_var, "safetyRatings");
    if (sr_var && sr_var->is_vector()) {
      for (const auto& sr : sr_var->vector()) {
        pf.safety_ratings.push_back(SafetyRatingFromVariant(sr));
      }
    }
    prompt_feedback = pf;
  }

  Optional<UsageMetadata> usage_metadata;
  const Variant* um_var = FindField(root, "usageMetadata");
  if (um_var && um_var->is_map()) {
    UsageMetadata um;
    um.prompt_token_count = GetIntField(*um_var, "promptTokenCount");
    um.candidates_token_count = GetIntField(*um_var, "candidatesTokenCount");
    um.total_token_count = GetIntField(*um_var, "totalTokenCount");
    um.thoughts_token_count = GetIntField(*um_var, "thoughtsTokenCount");
    um.tool_use_prompt_token_count =
        GetIntField(*um_var, "toolUsePromptTokenCount");
    um.cached_content_token_count =
        GetIntField(*um_var, "cachedContentTokenCount");
    um.prompt_tokens_details =
        ParseModalityTokenCounts(FindField(*um_var, "promptTokensDetails"));
    um.candidates_tokens_details =
        ParseModalityTokenCounts(FindField(*um_var, "candidatesTokensDetails"));
    um.tool_use_prompt_tokens_details = ParseModalityTokenCounts(
        FindField(*um_var, "toolUsePromptTokensDetails"));
    um.cache_tokens_details =
        ParseModalityTokenCounts(FindField(*um_var, "cacheTokensDetails"));
    usage_metadata = um;
  }

  *out_response =
      GenerateContentResponse(candidates, prompt_feedback, usage_metadata);
  return true;
}

bool ParseCountTokensResponseJson(const std::string& json,
                                  CountTokensResponse* out_response,
                                  std::string* out_error) {
  if (!out_response) return false;
  Variant root = ::firebase::util::JsonToVariant(json.c_str());
  if (!root.is_map()) {
    if (out_error) {
      *out_error = "Unable to parse CountTokensResponse JSON object.";
    }
    return false;
  }

  CountTokensResponse resp;
  resp.total_tokens = GetIntField(root, "totalTokens");
  resp.total_billable_characters = GetIntField(root, "totalBillableCharacters");
  resp.prompt_tokens_details =
      ParseModalityTokenCounts(FindField(root, "promptTokensDetails"));
  *out_response = resp;
  return true;
}

std::string ParseHttpErrorJson(int status_code, const std::string& body) {
  std::ostringstream oss;
  oss << "HTTP " << status_code;
  if (!body.empty()) {
    Variant root = ::firebase::util::JsonToVariant(body.c_str());
    if (root.is_map()) {
      const Variant* err_var = FindField(root, "error");
      if (err_var && err_var->is_map()) {
        std::string status = GetStringField(*err_var, "status");
        std::string message = GetStringField(*err_var, "message");
        if (!status.empty()) oss << " (" << status << ")";
        if (!message.empty()) {
          oss << ": " << message;
          return oss.str();
        }
      }
    }
    oss << ": " << body;
  }
  return oss.str();
}

}  // namespace internal
}  // namespace ai
}  // namespace firebase
