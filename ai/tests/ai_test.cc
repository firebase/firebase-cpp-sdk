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

#include "firebase/ai.h"

#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "ai/src/common/http_client.h"
#include "ai/src/common/litert_c_bridge.h"
#include "ai/src/common/serialization.h"
#include "app/src/variant_util.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"

namespace firebase {
namespace ai {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;

template <typename T>
void WaitForFuture(const Future<T>& fut) {
  while (fut.status() != kFutureStatusComplete) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

TEST(FirebaseAITest, UrlConstructionGoogleAIAndEnterprise) {
  AppOptions options;
  options.set_app_id("1:123456789:android:abcdef");
  options.set_api_key("test-api-key");
  options.set_project_id("my-test-project");

  App* app = firebase::testing::CreateApp(options);
  ASSERT_NE(app, nullptr);

  std::string google_ai_url = internal::AiHttpClient::ConstructModelUrl(
      app, Backend::GoogleAI(), "gemini-2.5-flash", "generateContent");
  EXPECT_THAT(google_ai_url,
              Eq("https://firebasevertexai.googleapis.com/v1beta/projects/"
                 "my-test-project/models/gemini-2.5-flash:generateContent"));

  std::string enterprise_url = internal::AiHttpClient::ConstructModelUrl(
      app, Backend::Enterprise("us-central1"), "models/gemini-2.5-pro",
      "streamGenerateContent?alt=sse");
  EXPECT_THAT(enterprise_url,
              Eq("https://firebasevertexai.googleapis.com/v1beta/projects/"
                 "my-test-project/locations/us-central1/publishers/google/"
                 "models/gemini-2.5-pro:streamGenerateContent?alt=sse"));

  std::string template_url = internal::AiHttpClient::ConstructTemplateUrl(
      app, Backend::GoogleAI(), "welcome-template", "templateGenerateContent");
  EXPECT_THAT(
      template_url,
      Eq("https://firebasevertexai.googleapis.com/v1beta/projects/"
         "my-test-project/templates/welcome-template:templateGenerateContent"));

  FirebaseAI* ai_google = FirebaseAI::GetInstance(app, Backend::GoogleAI());
  ASSERT_NE(ai_google, nullptr);
  EXPECT_EQ(ai_google, FirebaseAI::GetInstance(Backend::GoogleAI()));

  FirebaseAI* ai_vertex =
      FirebaseAI::GetInstance(app, Backend::VertexAI("us-central1"));
  ASSERT_NE(ai_vertex, nullptr);
  EXPECT_NE(ai_google, ai_vertex);

  GenerativeModel model = ai_google->GetGenerativeModel("gemini-2.5-flash");
  std::vector<ModelContent> initial_history{
      ModelContent::Text("Hello!"),
      ModelContent::Model("Hi there! How can I help?")};
  Chat chat = model.StartChat(initial_history);
  EXPECT_EQ(chat.history().size(), 2u);

  delete ai_google;
  delete ai_vertex;
  delete app;
}

TEST(FirebaseAITest, SchemaAndJsonSchemaSerialization) {
  std::map<std::string, Schema> props;
  props["city"] = Schema::String("City name");
  props["units"] =
      Schema::Enum(std::vector<std::string>{"celsius", "fahrenheit"},
                   std::string("Temperature units"), true);
  std::vector<std::string> optional_props{"units"};
  Schema obj_schema =
      Schema::Object(props, optional_props, std::string("Weather query"));

  Variant open_api_var = internal::SchemaToVariant(obj_schema);
  std::string open_api_json = util::VariantToJson(open_api_var);
  EXPECT_THAT(open_api_json, HasSubstr("\"type\":\"OBJECT\""));
  EXPECT_THAT(open_api_json, HasSubstr("\"required\":[\"city\"]"));
  EXPECT_THAT(open_api_json,
              HasSubstr("\"enum\":[\"celsius\",\"fahrenheit\"]"));

  Variant json_schema_var = internal::JsonSchemaToVariant(obj_schema);
  std::string std_json_schema = util::VariantToJson(json_schema_var);
  EXPECT_THAT(std_json_schema, HasSubstr("\"type\":\"object\""));
  EXPECT_THAT(std_json_schema, HasSubstr("\"type\":[\"string\",\"null\"]"));
}

TEST(FirebaseAITest, GenerateContentRequestAndCountTokensSerialization) {
  GenerationConfig gen_config;
  gen_config.temperature = 0.5f;
  gen_config.max_output_tokens = 256;
  gen_config.thinking_config = ThinkingConfig(128, true);

  std::vector<SafetySetting> safety_settings{
      SafetySetting(kHarmCategoryHateSpeech, kHarmBlockThresholdOnlyHigh,
                    kHarmBlockMethodSeverity)};

  std::map<std::string, Schema> fn_params;
  fn_params["location"] = Schema::String("Location name");
  FunctionDeclaration fn_decl("get_weather", "Gets the current weather",
                              fn_params);
  std::vector<Tool> tools{Tool(fn_decl), Tool(GoogleSearch())};

  ToolConfig tool_config(FunctionCallingConfig::Auto());
  ModelContent sys_instruction =
      ModelContent::System("You are a helpful meteorologist.");

  std::vector<ModelContent> contents{
      ModelContent::Text("What is the weather in Boston?")};

  // GoogleAI omits SafetySetting.method; Enterprise includes it.
  std::string google_req_json = internal::BuildGenerateContentRequestJson(
      contents, gen_config, safety_settings, tools, tool_config,
      sys_instruction, kBackendProviderGoogleAI);
  EXPECT_THAT(google_req_json, HasSubstr("\"What is the weather in Boston?\""));
  EXPECT_THAT(google_req_json, HasSubstr("\"thinkingBudget\":128"));
  EXPECT_THAT(google_req_json, HasSubstr("\"includeThoughts\":true"));
  EXPECT_THAT(google_req_json, HasSubstr("\"googleSearch\":{}"));
  EXPECT_THAT(google_req_json, ::testing::Not(HasSubstr("\"SEVERITY\"")));

  std::string enterprise_req_json = internal::BuildGenerateContentRequestJson(
      contents, gen_config, safety_settings, tools, tool_config,
      sys_instruction, kBackendProviderEnterprise);
  EXPECT_THAT(enterprise_req_json, HasSubstr("\"method\":\"SEVERITY\""));

  // GoogleAI countTokens wraps in generateContentRequest.
  std::string google_count_json = internal::BuildCountTokensRequestJson(
      "gemini-2.5-flash", contents, gen_config, safety_settings, tools,
      tool_config, sys_instruction, kBackendProviderGoogleAI);
  EXPECT_THAT(google_count_json, HasSubstr("\"generateContentRequest\""));
  EXPECT_THAT(google_count_json,
              HasSubstr("\"model\":\"models/gemini-2.5-flash\""));
}

TEST(FirebaseAITest, ParseGenerateContentResponseAndSseStream) {
  const char kSampleJson[] = R"({
    "candidates": [{
      "content": {
        "role": "model",
        "parts": [
          {"text": "Let me check the weather...", "thought": true, "thoughtSignature": "sig-123"},
          {"text": "It is 72F and sunny in Boston."},
          {"functionCall": {"name": "get_forecast", "id": "call-1", "args": {"days": 3}}}
        ]
      },
      "finishReason": "STOP",
      "safetyRatings": [{
        "category": "HARM_CATEGORY_HARASSMENT",
        "probability": "NEGLIGIBLE",
        "blocked": false
      }],
      "citationMetadata": {
        "citationSources": [{
          "startIndex": 0,
          "endIndex": 10,
          "uri": "https://example.com"
        }]
      }
    }],
    "usageMetadata": {
      "promptTokenCount": 12,
      "candidatesTokenCount": 20,
      "thoughtsTokenCount": 8,
      "totalTokenCount": 40,
      "promptTokensDetails": [{"modality": "TEXT", "tokenCount": 12}]
    }
  })";

  GenerateContentResponse resp;
  std::string err;
  ASSERT_TRUE(internal::ParseGenerateContentResponseJson(
      kSampleJson, kBackendProviderGoogleAI, &resp, &err))
      << err;
  EXPECT_THAT(resp.text(), Eq("It is 72F and sunny in Boston."));
  EXPECT_THAT(resp.thought_summary(), Eq("Let me check the weather..."));
  ASSERT_EQ(resp.function_calls().size(), 1u);
  EXPECT_THAT(resp.function_calls()[0].name, Eq("get_forecast"));
  ASSERT_TRUE(resp.function_calls()[0].id.has_value());
  EXPECT_THAT(resp.function_calls()[0].id.value(), Eq("call-1"));
  EXPECT_EQ(resp.function_calls()[0].args.at("days").int64_value(), 3);
  ASSERT_TRUE(resp.usage_metadata().has_value());
  EXPECT_EQ(resp.usage_metadata()->total_token_count, 40);
  EXPECT_EQ(resp.usage_metadata()->thoughts_token_count, 8);

  // Verify SseStreamParser handles fragmented chunks across lines.
  std::vector<std::string> streamed_texts;
  internal::SseStreamParser parser(
      kBackendProviderGoogleAI,
      [&streamed_texts](const GenerateContentResponse& chunk) {
        streamed_texts.push_back(chunk.text());
      });

  std::string sse_part1 =
      "data: {\"candidates\":[{\"content\":{\"role\":\"model\",\"parts\":[{"
      "\"text\":\"Hello \"}]}}]}\r\ndata: {\"candidates\":[{\"content\":";
  std::string sse_part2 =
      "{\"role\":\"model\",\"parts\":[{\"text\":\"World!\"}]}}]}\n\n";

  parser.Feed(sse_part1.data(), sse_part1.size());
  parser.Feed(sse_part2.data(), sse_part2.size());
  parser.Flush();

  ASSERT_EQ(streamed_texts.size(), 2u);
  EXPECT_THAT(streamed_texts[0], Eq("Hello "));
  EXPECT_THAT(streamed_texts[1], Eq("World!"));
}

TEST(FirebaseAITest, HybridOnDeviceAndMultiTurnChatToggle) {
  AppOptions options;
  options.set_app_id("1:123456789:android:abcdef");
  options.set_api_key("invalid-api-key-for-fallback-test");
  options.set_project_id("my-test-project");

  App* app = firebase::testing::CreateApp(options);
  ASSERT_NE(app, nullptr);

  FirebaseAI* ai = FirebaseAI::GetInstance(app, Backend::GoogleAI());
  ASSERT_NE(ai, nullptr);

  OnDeviceParams on_device("simulated://gemma-3-270m-it",
                           kLiteRtAcceleratorCpu);
  HybridParams hybrid_params(kInferenceModeOnlyOnDevice, on_device);

  GenerativeModel model = ai->GetGenerativeModel(
      "gemini-2.5-flash", hybrid_params, Optional<GenerationConfig>(),
      ModelContent::System("Concise assistant"));
  EXPECT_TRUE(model.IsOnDeviceAvailable());
  EXPECT_EQ(model.inference_mode(), kInferenceModeOnlyOnDevice);

  Future<void> init_fut = model.InitializeOnDeviceModel();
  WaitForFuture(init_fut);
  EXPECT_EQ(init_fut.error(), kErrorNone);

  // Turn 1 in Chat: ONLY_ON_DEVICE
  Chat chat = model.StartChat();
  Future<GenerateContentResponse> turn1 =
      chat.SendMessage("Hello local Gemma!");
  WaitForFuture(turn1);
  ASSERT_EQ(turn1.error(), kErrorNone);
  ASSERT_NE(turn1.result(), nullptr);
  EXPECT_EQ(turn1.result()->inference_source(), kInferenceSourceOnDevice);
  EXPECT_THAT(turn1.result()->text(), HasSubstr("gemma-3-270m-it"));
  EXPECT_THAT(turn1.result()->text(), HasSubstr("Hello local Gemma!"));
  EXPECT_EQ(chat.history().size(), 2u);

  // Toggle Chat live to PREFER_ON_DEVICE and stream Turn 2
  chat.set_inference_mode(kInferenceModePreferOnDevice);
  EXPECT_EQ(chat.inference_mode(), kInferenceModePreferOnDevice);

  std::string streamed_reply;
  Future<void> turn2_stream = chat.SendMessageStream(
      "Second turn via streaming",
      [&streamed_reply](const GenerateContentResponse& chunk) {
        EXPECT_EQ(chunk.inference_source(), kInferenceSourceOnDevice);
        streamed_reply += chunk.text();
      });
  WaitForFuture(turn2_stream);
  ASSERT_EQ(turn2_stream.error(), kErrorNone);
  EXPECT_THAT(streamed_reply, HasSubstr("turn 2"));
  EXPECT_EQ(chat.history().size(), 4u);

  // CompactHistory on-device (replaces 4 turns with 2 compacted turns)
  Future<GenerateContentResponse> compact_fut = chat.CompactHistory();
  WaitForFuture(compact_fut);
  ASSERT_EQ(compact_fut.error(), kErrorNone);
  ASSERT_NE(compact_fut.result(), nullptr);
  EXPECT_EQ(chat.history().size(), 2u);
  EXPECT_THAT(chat.history()[0].parts()[0].text_part().text,
              HasSubstr("[Compacted Conversation Context]"));

  // ClearHistory
  chat.ClearHistory();
  EXPECT_TRUE(chat.history().empty());

  // CountTokens on-device
  Future<CountTokensResponse> count_fut =
      model.CountTokens("Count these tokens locally");
  WaitForFuture(count_fut);
  ASSERT_EQ(count_fut.error(), kErrorNone);
  ASSERT_NE(count_fut.result(), nullptr);
  EXPECT_GT(count_fut.result()->total_tokens, 0);

  delete ai;
  delete app;
}

TEST(FirebaseAITest, UnityCAbiBridgeLiteRt) {
  FirebaseAiLiteRtHandle handle = firebase_ai_litert_create(
      "simulated://gemma-3-270m-it", "", "",
      static_cast<int32_t>(kLiteRtAcceleratorCpu), 1024, 2, 0.7f, 40, 0.95f);
  ASSERT_NE(handle, nullptr);
  EXPECT_EQ(firebase_ai_litert_is_available(handle), 1);

  const char* req_json =
      "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"Ping from "
      "Unity C#\"}]}]}";
  char* resp_json = nullptr;
  char* err_str = nullptr;
  int32_t status = firebase_ai_litert_generate_content(handle, req_json,
                                                       &resp_json, &err_str);
  EXPECT_EQ(status, 0);
  EXPECT_EQ(err_str, nullptr);
  ASSERT_NE(resp_json, nullptr);
  EXPECT_THAT(std::string(resp_json), HasSubstr("\"ON_DEVICE\""));
  EXPECT_THAT(std::string(resp_json), HasSubstr("Ping from Unity C#"));
  firebase_ai_litert_free_string(resp_json);

  int32_t total_tokens = 0;
  status = firebase_ai_litert_count_tokens(handle, req_json, &total_tokens,
                                           &err_str);
  EXPECT_EQ(status, 0);
  EXPECT_GT(total_tokens, 0);

  firebase_ai_litert_destroy(handle);
}

}  // namespace
}  // namespace ai
}  // namespace firebase
