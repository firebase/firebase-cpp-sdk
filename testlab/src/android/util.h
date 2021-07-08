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

#ifndef FIREBASE_TESTLAB_SRC_ANDROID_UTIL_H_
#define FIREBASE_TESTLAB_SRC_ANDROID_UTIL_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

/// Returns true if the Test Lab API has been initialized and a game loop is
/// running
bool IsInitialized();

/// Prepares any platform-specific resources associated with the SDK.
void Initialize(const ::firebase::App* app);

/// Cleans up any platform-specific resources associated with the SDK.
void Terminate();

/// Obtains a file handle to the custom results file sent by the intent.
FILE* RetrieveCustomResultsFile();

/// Calls finish() on the activity, ending the game loop scenario.
void CallFinish();

}  // namespace internal
}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase

#endif  // FIREBASE_TESTLAB_SRC_ANDROID_UTIL_H_
