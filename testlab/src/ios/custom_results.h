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

#ifndef FIREBASE_TESTLAB_CLIENT_CPP_SRC_IOS_CUSTOM_RESULTS_H_
#define FIREBASE_TESTLAB_CLIENT_CPP_SRC_IOS_CUSTOM_RESULTS_H_

#include <stdio.h>

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

static const char* kResultsDir = "GameLoopResults";

// Creates and returns the custom results file.
FILE* CreateCustomResultsFile(int scenario);

// Creates and returns the log file for storing intermediate results.
FILE* CreateLogFile();

}  // namespace internal
}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase

#endif  // FIREBASE_TESTLAB_CLIENT_CPP_SRC_IOS_CUSTOM_RESULTS_H_
