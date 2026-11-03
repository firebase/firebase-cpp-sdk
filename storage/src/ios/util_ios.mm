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

#include <string>

#import "FirebaseStorage-Swift.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/util_ios.h"

namespace firebase {
namespace storage {
namespace internal {

Error NSErrorToErrorCode(NSError* _Nullable error) {
  if (!error) return kErrorNone;
  if (![error.domain isEqualToString:FIRStorageErrorDomain] &&
      ![error.domain isEqualToString:FIRStorageErrorCodeDomain]) {
    return kErrorUnknown;
  }
  switch (error.code) {
    case FIRStorageErrorCodeUnknown:
      return kErrorUnknown;
    case FIRStorageErrorCodeObjectNotFound:
      return kErrorObjectNotFound;
    case FIRStorageErrorCodeBucketNotFound:
      return kErrorBucketNotFound;
    case FIRStorageErrorCodeProjectNotFound:
      return kErrorProjectNotFound;
    case FIRStorageErrorCodeQuotaExceeded:
      return kErrorQuotaExceeded;
    case FIRStorageErrorCodeUnauthenticated:
      return kErrorUnauthenticated;
    case FIRStorageErrorCodeUnauthorized:
      return kErrorUnauthorized;
    case FIRStorageErrorCodeRetryLimitExceeded:
      return kErrorRetryLimitExceeded;
    case FIRStorageErrorCodeNonMatchingChecksum:
      return kErrorNonMatchingChecksum;
    case FIRStorageErrorCodeDownloadSizeExceeded:
      return kErrorDownloadSizeExceeded;
    case FIRStorageErrorCodeCancelled:
      return kErrorCancelled;
    case FIRStorageErrorCodeInvalidArgument:
      return kErrorInvalidArgument;
    default:
      return kErrorUnknown;
  }
}

std::string GetNSErrorMessage(NSError* _Nullable error, Error code) {
  if (!error) {
    return GetErrorMessage(code);
  }
  // Hold the description in a strong local: -UTF8String returns a pointer into
  // the string's storage, which is only valid while the NSString is alive, so
  // the std::string copy must be made before desc_str goes out of scope.
  NSString* desc_str = error.localizedDescription;
  const char* desc = desc_str ? desc_str.UTF8String : nullptr;
  if (desc && desc[0] != '\0') {
    return std::string(desc);
  }
  return GetErrorMessage(code);
}

std::string GetNSExceptionReason(NSException* _Nullable exception) {
  NSString* reason = exception ? exception.reason : nil;
  const char* reason_cstr = reason ? reason.UTF8String : nullptr;
  return reason_cstr ? std::string(reason_cstr)
                     : std::string("Unknown exception");
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
