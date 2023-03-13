// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_COMMON_COMMON_H_
#define FIREBASE_APP_CHECK_SRC_COMMON_COMMON_H_

namespace firebase {
namespace app_check {
namespace internal {

// Used by App Check functions that return a future
enum AppCheckFn {
  kAppCheckFnGetAppCheckToken = 0,
  kAppCheckFnGetAppCheckStringInternal,
  kAppCheckFnCount,
};

// Helper function to get an existing AppCheck instance or nullptr,
// if an instance does not already exist.
::firebase::app_check::AppCheck* GetExistingAppCheckInstance(
    ::firebase::App* app);

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_COMMON_COMMON_H_
