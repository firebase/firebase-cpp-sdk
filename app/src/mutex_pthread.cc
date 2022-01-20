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
#include <errno.h>
#include <pthread.h>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {

Mutex::Mutex(Mode mode) {
  pthread_mutexattr_t attr;
  int ret = pthread_mutexattr_init(&attr);
  FIREBASE_ASSERT(ret == 0);
  if (mode & kModeRecursive) {
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    FIREBASE_ASSERT(ret == 0);
  }
  ret = pthread_mutex_init(&mutex_, &attr);
  FIREBASE_ASSERT(ret == 0);
  ret = pthread_mutexattr_destroy(&attr);
  FIREBASE_ASSERT(ret == 0);
}

Mutex::~Mutex() {
  int ret = pthread_mutex_destroy(&mutex_);
  FIREBASE_ASSERT(ret == 0);
  (void)ret;
}

void Mutex::Acquire() {
  int ret = pthread_mutex_lock(&mutex_);
  if (ret == EINVAL) {
    return;
  }
#if defined(__APPLE__)
  // Lock / unlock will fail in a static initializer on OSX and iOS.
  FIREBASE_ASSERT(ret == 0 || ret == EINVAL);
#else
  FIREBASE_ASSERT(ret == 0);
#endif  // defined(__APPLE__)
  (void)ret;
}

void Mutex::Release() {
  int ret = pthread_mutex_unlock(&mutex_);
#if defined(__APPLE__)
  // Lock / unlock will fail in a static initializer on OSX and iOS.
  FIREBASE_ASSERT(ret == 0 || ret == EINVAL);
#else
  FIREBASE_ASSERT(ret == 0);
#endif  // defined(__APPLE__)
  (void)ret;
}

}  // namespace firebase
