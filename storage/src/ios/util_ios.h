// Copyright 2016 Google LLC

#ifndef FIREBASE_STORAGE_SRC_IOS_UTIL_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_UTIL_IOS_H_

#import <Foundation/Foundation.h>

#include <string>

#include "storage/src/include/firebase/storage/common.h"

namespace firebase::storage::internal {

Error NSErrorToErrorCode(NSError* _Nullable error);
std::string GetNSErrorMessage(NSError* _Nullable error, Error code);
std::string GetNSExceptionReason(NSException* _Nullable exception);

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_IOS_UTIL_IOS_H_
