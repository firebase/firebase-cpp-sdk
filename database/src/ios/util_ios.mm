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

#include "database/src/ios/util_ios.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"

namespace firebase {
namespace database {
namespace internal {

Error NSErrorToErrorCode(NSError* error) {
  if (!error) return kErrorNone;
  switch (error.code) {
    // LINT.IfChange
    // As far as I can tell, the only place error codes are declared is in the following file using
    // magic numbers:
    // http://cs/piper///depot_firebase_ios_Releases/FirebaseDatabase/Library/Utilities/FUtilities.m?l=229
    case 1: {
      return kErrorPermissionDenied;
    }
    case 2: {
      return kErrorUnavailable;
    }
    case 3: {
      return kErrorWriteCanceled;
    }
    default: {
      return kErrorUnknownError;
    }
      // LINT.ThenChange(//depot_firebase_ios_Releases/FirebaseDatabase/Library/Utilities/FUtilities.m)
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
