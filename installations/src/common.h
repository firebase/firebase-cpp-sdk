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

#ifndef FIREBASE_FIS_CLIENT_CPP_SRC_COMMON_H_
#define FIREBASE_FIS_CLIENT_CPP_SRC_COMMON_H_

namespace firebase {
namespace installations {

enum InstallationsFn {
  kInstallationsFnGetId,
  kInstallationsFnGetToken,
  kInstallationsFnDelete,
  kInstallationsFnCount
};

/// @brief Describes the error codes returned by futures.
enum InstallationsError {
  // The future returned successfully.
  // This should always evaluate to zero, to ensure that the future returns
  // a zero result on success.
  kInstallationsErrorNone = 0,
  // The future returned unsuccessfully.
  kInstallationsErrorFailure,
};

}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_FIS_CLIENT_CPP_SRC_COMMON_H_
