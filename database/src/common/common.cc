// Copyright 2016 Google LLC
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

#include "database/src/include/firebase/database/common.h"

#include "app/src/include/firebase/internal/common.h"

namespace firebase {
namespace database {

static const char* g_error_messages[] = {
    // kErrorNone
    "The operation was a success, no error occurred.",
    // kErrorDisconnected
    "The operation had to be aborted due to a network disconnect.",
    // kErrorExpiredToken
    "The supplied auth token has expired.",
    // kErrorInvalidToken
    "The specified authentication token is invalid.",
    // kErrorMaxRetries
    "The transaction had too many retries.",
    // kErrorNetworkError
    "The operation could not be performed due to a network error.",
    // kErrorOperationFailed
    "The server indicated that this operation failed.",
    // kErrorOverriddenBySet
    "The transaction was overridden by a subsequent set.",
    // kErrorPermissionDenied
    "This client does not have permission to perform this operation.",
    // kErrorUnavailable
    "The service is unavailable.",
    // kErrorUnknownError
    "An unknown error occurred.",
    // kErrorWriteCanceled
    "The write was canceled locally.",
    // kErrorInvalidVariantType
    "You specified an invalid Variant type for a field.",
    // kErrorConflictingOperationInProgress
    "An operation that conflicts with this one is already in progress.",
    // kErrorTransactionAbortedByUser
    "The transaction was aborted by the user's code.",
};

const char* GetErrorMessage(Error error) {
  return error < FIREBASE_ARRAYSIZE(g_error_messages) ? g_error_messages[error]
                                                      : "";
}

static const Variant* g_server_value_timestamp = nullptr;
const Variant& ServerTimestamp() {
  if (g_server_value_timestamp == nullptr) {
    // The Firebase server defines a ServerValue for Timestamp as a map with the
    // key ".sv" and the value "timestamp".
    std::map<Variant, Variant> server_value;
    server_value.insert(std::make_pair(".sv", "timestamp"));
    g_server_value_timestamp = new Variant(server_value);
  }
  return *g_server_value_timestamp;
}

}  // namespace database
}  // namespace firebase
