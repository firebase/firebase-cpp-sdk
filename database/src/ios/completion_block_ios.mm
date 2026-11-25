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

#include "database/src/ios/completion_block_ios.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/ios/util_ios.h"

namespace firebase {
namespace database {
namespace internal {

CompletionBlock CreateCompletionBlock(SafeFutureHandle<void> handle,
                                      std::shared_ptr<detail::FutureImplCore> future_core,
                                      std::weak_ptr<DatabaseInternal> database) {
  auto data = std::make_shared<FutureCallbackData<void>>(handle, std::move(future_core),
                                                         std::move(database));
  return ^void(NSError *_Nullable error, FIRDatabaseReference *_Nonnull /*ref*/) {
    if (auto locked_db = data->database.lock()) {
      Error error_code = NSErrorToErrorCode(error);
      const char* error_string = GetErrorMessage(error_code);
      ReferenceCountedFutureImpl::CompleteCore(data->future_core, data->handle, error_code,
                                               error_string);
    }
  };
}

}  //namespace internal
}  //namespace database
}  //namespace firebase
