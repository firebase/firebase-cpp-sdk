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

#include "app/src/semaphore.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

namespace {

// Callback function installed by FutureBase::Wait().
// This will be called when the future is complete.
void WaitCallback(const FutureBase &, void *user_data) {
  Semaphore *semaphore = static_cast<Semaphore *>(user_data);
  // Wake up the thread that called Wait().
  semaphore->Post();
}

}  // anonymous namespace

const int FutureBase::kWaitTimeoutInfinite = -1;

bool FutureBase::Wait(int timeout_milliseconds) const {
  Semaphore semaphore(0);
  auto callback_handle = AddOnCompletion(WaitCallback, &semaphore);
  if (timeout_milliseconds == kWaitTimeoutInfinite) {
    semaphore.Wait();
    return true;
  } else {
    bool completed = semaphore.TimedWait(timeout_milliseconds);
    if (!completed) {
      RemoveOnCompletion(callback_handle);
    }
    return completed;
  }
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
