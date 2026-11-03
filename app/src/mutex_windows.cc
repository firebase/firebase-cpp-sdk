/*
 * Copyright 2021 Google LLC
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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cassert>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"

namespace firebase {

Mutex::Mutex(Mode mode) : mode_(mode) {
  if (mode & kModeRecursive) {
    synchronization_object_ = CreateMutex(nullptr, FALSE, nullptr);
  } else {
    synchronization_object_ = CreateSemaphore(nullptr, 1, 1, nullptr);
  }
  FIREBASE_ASSERT(synchronization_object_ != nullptr);
}

Mutex::~Mutex() {
  if (synchronization_object_ != nullptr) {
    CloseHandle(synchronization_object_);
  }
}

void Mutex::Acquire() {
  DWORD ret = WaitForSingleObject(synchronization_object_, INFINITE);
  FIREBASE_ASSERT(ret == WAIT_OBJECT_0);
  (void)ret;
}

void Mutex::Release() {
  BOOL ret;
  if (mode_ & kModeRecursive) {
    ret = ReleaseMutex(synchronization_object_);
  } else {
    ret = ReleaseSemaphore(synchronization_object_, 1, 0);
  }
  FIREBASE_ASSERT(ret != 0);
  (void)ret;
}

}  // namespace firebase
