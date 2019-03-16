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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_UTIL_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_UTIL_IOS_H_

#import <Foundation/Foundation.h>

#include "storage/src/include/firebase/storage/common.h"

namespace firebase {
namespace storage {
namespace internal {

Error NSErrorToErrorCode(NSError* error);

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_UTIL_IOS_H_
