// Copyright 2021 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_IOS_FIR_STORAGE_LIST_RESULT_POINTER_H_
#define FIREBASE_STORAGE_SRC_IOS_FIR_STORAGE_LIST_RESULT_POINTER_H_

#include "app/src/ios/pointer_ios.h"

// Forward declare Obj-C types
#ifdef __OBJC__
@class FIRStorageListResult;
#else
typedef struct objc_object FIRStorageListResult;
#endif

namespace firebase {
namespace storage {
namespace internal {

// Define FIRStorageListResultPointer. This is an iOS specific implementation
// detail that is not exposed in the public API.
FIREBASE_DEFINE_POINTER_WRAPPER(FIRStorageListResultPointer,
                                FIRStorageListResult);

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_FIR_STORAGE_LIST_RESULT_POINTER_H_
