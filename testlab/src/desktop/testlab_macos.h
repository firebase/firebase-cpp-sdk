// Copyright 2020 Google LLC
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

#ifndef FIREBASE_TESTLAB_SRC_DESKTOP_TESTLAB_MACOS_H_
#define FIREBASE_TESTLAB_SRC_DESKTOP_TESTLAB_MACOS_H_

#include <string>

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

// Get the command line arguments used to launch the running process.
std::vector<std::string> GetArguments();

}  // namespace internal
}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase

#endif  //  FIREBASE_TESTLAB_SRC_DESKTOP_TESTLAB_MACOS_H_
