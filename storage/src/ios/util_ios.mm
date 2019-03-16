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

#import <Foundation/Foundation.h>

#import "FIRStorage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/ios/util_ios.h"

namespace firebase {
namespace storage {
namespace internal {

Error NSErrorToErrorCode(NSError* error) {
  if (!error) return kErrorNone;
  switch (error.code) {
    case FIRStorageErrorCodeUnknown: return kErrorUnknown;
    case FIRStorageErrorCodeObjectNotFound: return kErrorObjectNotFound;
    case FIRStorageErrorCodeBucketNotFound: return kErrorBucketNotFound;
    case FIRStorageErrorCodeProjectNotFound: return kErrorProjectNotFound;
    case FIRStorageErrorCodeQuotaExceeded: return kErrorQuotaExceeded;
    case FIRStorageErrorCodeUnauthenticated: return kErrorUnauthenticated;
    case FIRStorageErrorCodeUnauthorized: return kErrorUnauthorized;
    case FIRStorageErrorCodeRetryLimitExceeded: return kErrorRetryLimitExceeded;
    case FIRStorageErrorCodeNonMatchingChecksum: return kErrorNonMatchingChecksum;
    case FIRStorageErrorCodeDownloadSizeExceeded: return kErrorDownloadSizeExceeded;
    case FIRStorageErrorCodeCancelled: return kErrorCancelled;
    default: return kErrorUnknown;
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
