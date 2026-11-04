// Copyright 2016 Google LLC

#include "storage/src/ios/util_ios.h"

#import <Foundation/Foundation.h>

#include <string>

#import "FirebaseStorage-Swift.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/ios/storage_ios.h"

namespace firebase::storage::internal {

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
  if (!error) return GetErrorMessage(code);
  NSString* desc_str = error.localizedDescription;
  const char* desc = desc_str ? desc_str.UTF8String : nullptr;
  return (desc && desc[0] != '\0') ? std::string(desc) : GetErrorMessage(code);
}

std::string GetNSExceptionReason(NSException* _Nullable exception) {
  NSString* reason = exception ? exception.reason : nil;
  const char* reason_cstr = reason ? reason.UTF8String : nullptr;
  return reason_cstr ? std::string(reason_cstr) : "Unknown exception";
}

}  // namespace firebase::storage::internal
