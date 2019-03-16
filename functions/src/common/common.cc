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

#include "functions/src/include/firebase/functions/common.h"

#include <ctime>

namespace firebase {
namespace functions {
namespace internal {

const char* GetErrorMessage(Error error) {
  switch (error) {
    case kErrorNone:
      return "OK";
    case kErrorCancelled:
      return "Cancelled";
    case kErrorUnknown:
      return "Unknown";
    case kErrorInvalidArgument:
      return "Invalid Argument";
    case kErrorDeadlineExceeded:
      return "Deadline Exceeded";
    case kErrorNotFound:
      return "Not Found";
    case kErrorAlreadyExists:
      return "Already Exists";
    case kErrorPermissionDenied:
      return "Permission Denied";
    case kErrorUnauthenticated:
      return "Unauthenticated";
    case kErrorResourceExhausted:
      return "Resource Exhausted";
    case kErrorFailedPrecondition:
      return "Failed Precondition";
    case kErrorAborted:
      return "Aborted";
    case kErrorOutOfRange:
      return "Out of Range";
    case kErrorUnimplemented:
      return "Unimplemented";
    case kErrorInternal:
      return "Internal";
    case kErrorUnavailable:
      return "Unavailable";
    case kErrorDataLoss:
      return "Data Loss";
    // This shouldn't happen, but the iOS and Android SDKs default to INTERNAL.
    default:
      return "Internal";
  }
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
