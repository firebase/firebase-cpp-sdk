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

#include <errno.h>

#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "app/src/path.h"
#include "app/src/reference_count.h"
#include "flatbuffers/util.h"
#include "testlab/src/common/common.h"
#include "testlab/src/include/firebase/testlab.h"

#if defined(_WIN32)
// windows.h must be first to define basic Windows types.
// clang-format off
#include <windows.h>  // NOLINT
// clang-format on
#include <direct.h>
#include <shellapi.h>
#else
#include <stdlib.h>
#include <sys/stat.h>
#endif  // defined(_WIN32)

#if FIREBASE_PLATFORM_OSX
#include "testlab/src/desktop/testlab_macos.h"
#endif

#if FIREBASE_PLATFORM_WINDOWS
#define mkdir(x, y) _mkdir(x)
#endif  // FIREBASE_PLATFORM_WINDOWS

using firebase::internal::ReferenceCount;
using firebase::internal::ReferenceCountLock;

namespace firebase {
namespace test_lab {
namespace game_loop {

static ReferenceCount g_initializer;  // NOLINT

static const char* kScenarioFlagPrefix = "--game_loop_scenario=";
static const char* kResultsDirFlagPrefix = "--game_loop_results_dir=";
static const char* kLogFileName = "firebase-game-loop.log";

namespace internal {

// Determine whether the test lab module is initialized.
bool IsInitialized() { return g_initializer.references() > 0; }
static void ParseCommandLineArgs();
static FILE* GetCustomResultsFile();

}  // namespace internal

void Initialize(const ::firebase::App& app) {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (ref_count.references() != 0) {
    LogWarning("Test Lab API already initialized");
    return;
  }
  ref_count.AddReference();
  internal::CreateOrOpenLogFile();
  internal::ParseCommandLineArgs();
}

void Terminate() {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (ref_count.references() == 0) {
    LogWarning("Test Lab API was terminated or never initialized");
    return;
  }
  if (ref_count.references() == 1) {
    internal::SetScenario(0);
    internal::CloseLogFile();
    internal::TerminateCommon();
  }
  ref_count.RemoveReference();
}

int GetScenario() {
  if (!internal::IsInitialized()) {
    return 0;
  }
  return internal::GetScenario();
}

void LogText(const char* format, ...) {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (GetScenario() == 0) return;
  va_list args;
  va_start(args, format);
  internal::LogText(format, args);
  va_end(args);
}

void FinishScenario(::firebase::test_lab::game_loop::ScenarioOutcome outcome) {
  if (GetScenario() == 0) return;
  FILE* result_file = internal::OpenCustomResultsFile(GetScenario());
  if (result_file != nullptr) {
    internal::OutputResult(outcome, result_file);
  }
  Terminate();
  // TODO(brandonmorris): Find a way to signal a test is complete (e.g. write to
  // a file in the results dir or set an env var).
}

namespace internal {

static bool NotEmpty(const char* str) {
  return str != nullptr && str[0] != '\0';
}

static const char* GetTempDir() {
  char* temp;
#if defined(_WIN32)
  temp = std::getenv("TMP");
  if (NotEmpty(temp)) return temp;
  temp = std::getenv("TEMP");
  if (NotEmpty(temp)) return temp;
  temp = std::getenv("USERPROFILE");
  if (NotEmpty(temp)) return temp;
  // If all else fails, return the current directory
  return "";
#else
  temp = std::getenv("TMPDIR");
  if (NotEmpty(temp)) return temp;
  temp = std::getenv("TMP");
  if (NotEmpty(temp)) return temp;
  temp = std::getenv("TEMP");
  if (NotEmpty(temp)) return temp;
  temp = std::getenv("TEMPDIR");
  if (NotEmpty(temp)) return temp;
  return "/tmp";
#endif  // defined(_WIN32)
}

void CreateOrOpenLogFile() {
  std::string temp = GetTempDir();
  std::string log_filename = temp + "/" + kLogFileName;
  g_log_file = fopen(log_filename.c_str(), "w+");
  if (g_log_file == nullptr) {
    LogError(
        "Could not open the temporary log file at %s. Any logs from this game "
        "loop scenario will not be included in the custom results: %s",
        log_filename.c_str(), strerror(errno));
  }
}

static std::string GetResultFilename() {
  if (GetScenario() <= 0) {
    return "";
  }
  return "results_scenario_" + std::to_string(GetScenario()) + ".json";
}

static std::string GetArgumentForPrefix(std::string prefix,
                                        std::vector<std::string> arguments) {
  for (const std::string& argument : arguments) {
    if (argument.compare(0, prefix.size(), prefix) == 0) {
      return argument.substr(prefix.size());
    }
  }
  return std::string();
}

static void SetScenarioIfNotEmpty(std::string scenario_str) {
  if (!scenario_str.empty()) {
    int scenario;
    flatbuffers::StringToNumber(scenario_str.c_str(), &scenario);
    SetScenario(scenario);
  }
}

static std::vector<std::string> GetCommandLineArgs() {
#if FIREBASE_PLATFORM_WINDOWS
  wchar_t** arg_list;
  int n_args;
  arg_list = CommandLineToArgvW(GetCommandLineW(), &n_args);
  std::vector<std::string> arguments(n_args);
  for (int i = 0; i < n_args; i++) {
    const wchar_t* arg = arg_list[i];
    int arg_characters = static_cast<int>(wcslen(arg_list[i]));
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, arg, arg_characters, NULL,
                                          0, NULL, NULL);
    std::string argument(size_needed, 0);
    std::vector<char> buffer(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, arg, arg_characters, &buffer[0],
                        size_needed, NULL, NULL);
    arguments[i].append(&buffer[0]);
  }
  LocalFree(arg_list);
  return arguments;
#elif FIREBASE_PLATFORM_OSX
  return GetArguments();
#elif FIREBASE_PLATFORM_LINUX
  FILE* proc = fopen("/proc/self/cmdline", "r");
  std::vector<char> buffer;
  // /proc "files" aren't real files, so we can't know the total length ahead of
  // time and need to read one character at a time.
  for (;;) {
    int c = getc(proc);
    if (c == EOF) break;
    buffer.push_back(c);
  }
  return TokenizeByCharacter(buffer, '\0');
#else
#warning Game loop command line flags will not be parsed
  return std::vector<std::string>();
#endif
}

static void ParseCommandLineArgs() {
  std::vector<std::string> arguments = GetCommandLineArgs();
  std::string scenario_str =
      GetArgumentForPrefix(kScenarioFlagPrefix, arguments);
  SetScenarioIfNotEmpty(scenario_str);
  std::string directory =
      GetArgumentForPrefix(kResultsDirFlagPrefix, arguments);
  if (!directory.empty()) {
    SetResultsDirectory(directory.c_str());
  }
}

}  // namespace internal

}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase
