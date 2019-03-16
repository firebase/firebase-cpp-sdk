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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_COMPLETION_BLOCK_IOS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_COMPLETION_BLOCK_IOS_H_

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
namespace database {
class DatabaseInternal;
namespace internal {

// This file contains functions and structures for creating CompletetionBlocks.
// Nearly all transactions run the same block (the objective C version of a
// lambda) but they need to capture slightly different data. The block generated
// by CreateCompletionBlock is passed to the withCompletionBlock: argument of
// each transaction to be run when the transaction is completed.
//
// This is not a part of the util_ios.h header because placing any Objective C
// code in the header causes the linter to think the whole file is Objective C
// and then apply the wrong set of linting rules.

template <typename T>
struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<T> handle_, ReferenceCountedFutureImpl *_Nonnull impl_)
      : handle(handle_), impl(impl_), database(nullptr) {}
  SafeFutureHandle<T> handle;
  ReferenceCountedFutureImpl *_Nonnull impl;
  DatabaseInternal *_Nullable database;
};

typedef void (^CompletionBlock)(NSError *_Nullable, FIRDatabaseReference *_Nonnull);

CompletionBlock _Nonnull CreateCompletionBlock(SafeFutureHandle<void> handle,
                                               ReferenceCountedFutureImpl *_Nonnull future);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_COMPLETION_BLOCK_IOS_H_
