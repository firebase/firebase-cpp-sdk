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

#include <cassert>
#include "database/src/ios/util_ios.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"

namespace firebase {
namespace database {
namespace internal {

Error NSErrorToErrorCode(NSError* error) {
  if (!error) return kErrorNone;
  if ([error.domain isEqualToString:NSURLErrorDomain]) {
    return kErrorNetworkError;
  }
  switch (error.code) {
    // LINT.IfChange
    // Error codes declared in FirebaseDatabase/Library/Utilities/FUtilities.m:
    case -1:  // kFErrorCodeDataStale (internal, mapped to Disconnected/MaxRetries)
    case -4: {
      return kErrorDisconnected;
    }
    case -2: {
      return kErrorOperationFailed;
    }
    case 1:
    case -3: {
      return kErrorPermissionDenied;
    }
    case -6: {
      return kErrorExpiredToken;
    }
    case -7: {
      return kErrorInvalidToken;
    }
    case -8: {
      return kErrorMaxRetries;
    }
    case -9: {
      return kErrorOverriddenBySet;
    }
    case 2:
    case -10: {
      return kErrorUnavailable;
    }
    case -11: {
      return kErrorUnknownError;
    }
    case -24: {
      return kErrorNetworkError;
    }
    case 3:
    case -25: {
      return kErrorWriteCanceled;
    }
    case 9999: {
      NSString* status = error.userInfo[NSLocalizedDescriptionKey];
      if ([status hasPrefix:@"permission_denied"]) return kErrorPermissionDenied;
      if ([status hasPrefix:@"unavailable"]) return kErrorUnavailable;
      if ([status hasPrefix:@"expired_token"]) return kErrorExpiredToken;
      if ([status hasPrefix:@"invalid_token"]) return kErrorInvalidToken;
      if ([status hasPrefix:@"maxretries"]) return kErrorMaxRetries;
      if ([status hasPrefix:@"overriddenbyset"]) return kErrorOverriddenBySet;
      if ([status hasPrefix:@"network_error"]) return kErrorNetworkError;
      if ([status hasPrefix:@"write_canceled"]) return kErrorWriteCanceled;
      return kErrorOperationFailed;
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
