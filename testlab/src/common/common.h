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

#ifndef FIREBASE_TESTLAB_SRC_COMMON_COMMON_H_
#define FIREBASE_TESTLAB_SRC_COMMON_COMMON_H_

#include <stdarg.h>

#include <cstdio>
#include <vector>

#include "testlab/src/include/firebase/testlab.h"

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

extern FILE* g_log_file;
extern std::string* g_results_dir;

void SetScenario(int scenario);
int GetScenario();

// Formats and produces the custom result file
void OutputResult(::firebase::test_lab::game_loop::ScenarioOutcome outcome,
                  FILE* result_file);

// Format and write the scenario's custom results. Does not close the file after
// writing.
void LogText(const char* format, va_list args);

void TerminateCommon();

void CreateOrOpenLogFile();  // Implemented in platform specific module
void CloseLogFile();
bool IsInitialized();

std::vector<std::string> TokenizeByCharacter(std::vector<char> buffer,
                                             char token);

void ResetLog();

void SetResultsDirectory(const char* path);

std::string GetResultsDirectory();

FILE* OpenCustomResultsFile(int scenario);

}  // namespace internal
}  // namespace game_loop
}  //  namespace test_lab
}  // namespace firebase

#endif  // FIREBASE_TESTLAB_SRC_COMMON_COMMON_H_
