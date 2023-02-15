// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "testlab/src/common/common.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "app/src/log.h"
#include "app/src/util.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "testlab/scenario_result_generated.h"
#include "testlab/scenario_result_resource.h"
#include "testlab/src/include/firebase/testlab.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    test_lab,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::test_lab::game_loop::Initialize(*app);
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::test_lab::game_loop::Terminate();
      }
    },
    false);

namespace firebase {
namespace test_lab {
namespace game_loop {

void SetScenario(int scenario_number) {
  if (!internal::IsInitialized() || GetScenario() == scenario_number) return;
  LogDebug("Resetting scenario number to %d", scenario_number);
  internal::ResetLog();
  internal::SetScenario(scenario_number);
}

void SetResultsDirectory(const char* path) {
  internal::SetResultsDirectory(path);
}

std::string GetResultsDirectory() { return internal::GetResultsDirectory(); }

namespace internal {

FILE* g_log_file;
std::string* g_results_dir;

static const char* kRootType = "ScenarioResult";

static int g_scenario = 0;

void SetScenario(int scenario) { g_scenario = scenario; }
int GetScenario() { return g_scenario; }
std::string GetResultsDirectory() {
  if (g_results_dir == nullptr) {
    return "";
  }
  return *g_results_dir;
}

std::vector<std::string> ReadLines(FILE* file);
std::string ScenarioToString(int scenario);

const char* OutcomeToString(
    ::firebase::test_lab::game_loop::ScenarioOutcome outcome) {
  switch (outcome) {
    case kScenarioOutcomeSuccess:
      return "success";
    case kScenarioOutcomeFailure:
      return "failure";
  }
}

void OutputResult(::firebase::test_lab::game_loop::ScenarioOutcome outcome,
                  FILE* result_file) {
  flatbuffers::FlatBufferBuilder builder(1024);
  std::string outcome_string = OutcomeToString(outcome);
  auto outcome_string_offset = builder.CreateString(outcome_string);
  std::rewind(g_log_file);
  std::vector<std::string> logs = ReadLines(g_log_file);
  auto text_log_offset = builder.CreateVectorOfStrings(logs);
  ScenarioResultBuilder result_builder(builder);
  result_builder.add_scenario_number(GetScenario());
  result_builder.add_outcome(outcome_string_offset);
  result_builder.add_text_log(text_log_offset);
  auto result = result_builder.Finish();
  builder.Finish(result);
  flatbuffers::Parser parser;
  const char* schema =
      reinterpret_cast<const char*>(scenario_result_resource_data);
  parser.Parse(schema);
  parser.SetRootType(kRootType);
  std::string jsongen;
  flatbuffers::GenerateText(parser, builder.GetBufferPointer(), &jsongen);
  fprintf(result_file, "%s", jsongen.c_str());
  fflush(result_file);
}

void LogText(const char* format, va_list args) {
  vfprintf(g_log_file, format, args);
  fprintf(g_log_file, "\n");
  fflush(g_log_file);
  LogMessageV(firebase::kLogLevelDebug, format, args);
}

void CloseLogFile() {
  if (g_log_file) {
    fclose(g_log_file);
    g_log_file = nullptr;
  }
}

std::vector<std::string> ReadLines(FILE* file) {
  fseek(file, 0, SEEK_END);
  auto file_size = ftell(file);
  rewind(file);
  std::vector<char> buffer(file_size);
  if (file_size != fread(&buffer[0], 1, file_size, file)) {
    LogError(
        "Could not read the custom results log file. Any results logged during "
        "the game loop scenario will not be included in the custom results.");
    return std::vector<std::string>();
  }
  return TokenizeByCharacter(buffer, '\n');
}

std::vector<std::string> TokenizeByCharacter(std::vector<char> buffer,
                                             char token) {
  std::vector<std::string> tokens;
  auto line_begin = buffer.begin();
  auto buffer_end = buffer.end();
  while (line_begin < buffer_end) {
    auto line_end = std::find(line_begin, buffer_end, token);
    tokens.push_back(std::string(line_begin, line_end));
    line_begin = line_end + 1;
  }
  return tokens;
}

void TerminateCommon() { SetResultsDirectory(nullptr); }

void ResetLog() {
  if (g_log_file) {
    fclose(g_log_file);
    g_log_file = nullptr;
  }
  CreateOrOpenLogFile();
}

void SetResultsDirectory(const char* path) {
  if (g_results_dir) {
    delete g_results_dir;
    g_results_dir = nullptr;
  }
  if (path && strlen(path)) {
    g_results_dir = new std::string(path);
  }
}

FILE* OpenCustomResultsFile(int scenario) {
  std::string file_name =
      "results_scenario_" + ScenarioToString(scenario) + ".json";
  std::string file_path;
  if (g_results_dir != nullptr) {
    file_path = *g_results_dir + "/" + file_name;
  } else {
    file_path = file_name;
  }
  FILE* file = fopen(file_path.c_str(), "w");
  if (file == nullptr) {
    LogError(
        "Could not open custom results file at %s. Results for this scenario "
        "will not be included: %s",
        file_path.c_str(), strerror(errno));
  }
  return file;
}

// std::to_string() isn't supported on Android NDK
std::string ScenarioToString(int scenario) {
  std::ostringstream stream;
  stream << scenario;
  return stream.str();
}

}  // namespace internal
}  // namespace game_loop
}  //  namespace test_lab
}  // namespace firebase
