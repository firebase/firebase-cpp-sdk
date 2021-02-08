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

#include "testlab/src/include/firebase/testlab.h"

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/log.h"
#include "app/src/log.h"
#include "app/src/reference_count.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "testlab/src/android/util.h"
#include "testlab/src/common/common.h"

using firebase::internal::ReferenceCount;
using firebase::internal::ReferenceCountLock;

namespace firebase {
namespace test_lab {
namespace game_loop {

static ReferenceCount g_initializer;  // NOLINT

namespace internal {

// Determine whether the test lab module is initialized.
bool IsInitialized() { return g_initializer.references() > 0; }

}  // namespace internal

// Initialize the API
void Initialize(const firebase::App& app) {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (ref_count.references() != 0) {
    LogWarning("Test Lab API already initialized");
    return;
  }
  ref_count.AddReference();
  LogDebug("Firebase Test Lab API initializing");
  internal::Initialize(&app);
}

// Clean up the API
void Terminate() {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (ref_count.references() == 0) {
    LogWarning("Test Lab API was never initialized");
    return;
  }
  if (ref_count.references() == 1) {
    internal::Terminate();
  }
  ref_count.RemoveReference();
}

// Return the game loop scenario's integer ID, or 0 if no game loop is running
int GetScenario() {
  if (!internal::IsInitialized()) return 0;
  return internal::GetScenario();
}

// Log progress text to the game loop's custom results and device logs
void LogText(const char* format, ...) {
  if (GetScenario() == 0) return;
  va_list args;
  va_start(args, format);
  internal::LogText(format, args);
  va_end(args);
}

// Complete the game loop scenario with the specified outcome
void FinishScenario(ScenarioOutcome outcome) {
  if (GetScenario() == 0) return;
  FILE* result_file = internal::RetrieveCustomResultsFile();
  if (result_file == nullptr) {
    LogError("Could not obtain the custom results file");
  } else {
    internal::OutputResult(outcome, result_file);
  }
  internal::CallFinish();
  Terminate();
  // TODO(brandonmorris): This works, but isn't the proper way to exit the app.
  // Look into either using ANativeActivity_finish or calling finish() on the
  // main thread.
  exit(0);
}

}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase
