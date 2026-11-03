// Copyright 2018 Google LLC
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

namespace firebase {
namespace functions {
namespace internal {

const char* GetErrorMessage(Error error) {
  switch (error) {
    case kErrorNone:
      return "OK";
    case kErrorCancelled:
      return "The operation was cancelled";
    case kErrorUnknown:
      return "Unknown error or an error from a different error domain";
    case kErrorInvalidArgument:
      return "Client specified an invalid argument";
    case kErrorDeadlineExceeded:
      return "Deadline expired before operation could complete";
    case kErrorNotFound:
      return "Requested entity was not found";
    case kErrorAlreadyExists:
      return "Entity that we attempted to create already exists";
    case kErrorPermissionDenied:
      return "The caller does not have permission to execute the specified "
             "operation";
    case kErrorUnauthenticated:
      return "The request does not have valid authentication credentials for "
             "the operation";
    case kErrorResourceExhausted:
      return "Some resource has been exhausted, perhaps a per-user quota, or "
             "perhaps the entire file system is out of space";
    case kErrorFailedPrecondition:
      return "Operation was rejected because the system is not in a state "
             "required for the operation's execution";
    case kErrorAborted:
      return "The operation was aborted, typically due to a concurrency issue "
             "like transaction aborts, etc.";
    case kErrorOutOfRange:
      return "Operation was attempted past the valid range";
    case kErrorUnimplemented:
      return "Operation is not implemented or not supported/enabled";
    case kErrorInternal:
      return "Internal errors";
    case kErrorUnavailable:
      return "The service is currently unavailable";
    case kErrorDataLoss:
      return "Unrecoverable data loss or corruption";
  }
  return "Unknown error";
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
