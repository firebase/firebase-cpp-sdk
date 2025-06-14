// Copyright 2025 Google LLC
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

#ifndef FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_
#define FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_

#include <windows.h>

#include <vector>

namespace firebase {
namespace analytics {
namespace internal {

HMODULE VerifyAndLoadAnalyticsLibrary(
    const wchar_t* library_filename,
    const std::vector<std::vector<unsigned char>>& allowed_hashes);

}  // namespace internal
}  // namespace analytics
}  // namespace firebase

#endif  // FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_
