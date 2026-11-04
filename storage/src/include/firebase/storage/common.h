// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_COMMON_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_COMMON_H_

namespace firebase {
namespace storage {

enum Error {
  kErrorNone = 0,
  kErrorUnknown,
  kErrorObjectNotFound,
  kErrorBucketNotFound,
  kErrorProjectNotFound,
  kErrorQuotaExceeded,
  kErrorUnauthenticated,
  kErrorUnauthorized,
  kErrorRetryLimitExceeded,
  kErrorNonMatchingChecksum,
  kErrorDownloadSizeExceeded,
  kErrorCancelled,
  kErrorInvalidArgument,
};

const char* GetErrorMessage(Error error);

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_COMMON_H_
