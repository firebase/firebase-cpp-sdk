// Copyright 2017 Google LLC
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

#ifndef FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_COMMON_H_
#define FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_COMMON_H_

#include "firebase/variant.h"

namespace firebase {
namespace functions {

/// Error code returned by Cloud Functions C++ functions (standard gRPC codes).
enum Error {
  kErrorNone = 0,
  kErrorCancelled = 1,
  kErrorUnknown = 2,
  kErrorInvalidArgument = 3,
  kErrorDeadlineExceeded = 4,
  kErrorNotFound = 5,
  kErrorAlreadyExists = 6,
  kErrorPermissionDenied = 7,
  kErrorUnauthenticated = 16,
  kErrorResourceExhausted = 8,
  kErrorFailedPrecondition = 9,
  kErrorAborted = 10,
  kErrorOutOfRange = 11,
  kErrorUnimplemented = 12,
  kErrorInternal = 13,
  kErrorUnavailable = 14,
  kErrorDataLoss = 15,
};

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
const char* GetErrorMessage(Error error);
}  // namespace internal
/// @endcond

}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_COMMON_H_
