/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/src/include/firebase/future.h"

namespace firebase {

const int FutureBase::kWaitTimeoutInfinite = -1;

bool FutureBase::Wait(int timeout_milliseconds) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (!api) {
    return false;
  }
  return api->WaitForFuture(handle, timeout_milliseconds);
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
