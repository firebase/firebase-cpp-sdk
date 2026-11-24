/*
 * Copyright 2018 Google LLC
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

#include "app/src/cleanup_notifier.h"

#include <utility>

namespace firebase {

CleanupNotifier::CleanupNotifier() : cleaned_up_(false) {}

CleanupNotifier::~CleanupNotifier() { CleanupAll(); }

void CleanupNotifier::RegisterObject(void* object, CleanupCallback callback) {
  MutexLock lock(mutex_);
  auto i = callbacks_.find(object);
  if (i != callbacks_.end()) {
    i->second = callback;
  } else {
    callbacks_.insert(std::make_pair(object, callback));
  }
}

void CleanupNotifier::UnregisterObject(void* object) {
  MutexLock lock(mutex_);
  callbacks_.erase(object);
}

void CleanupNotifier::CleanupAll() {
  for (;;) {
    std::pair<void*, CleanupCallback> object_and_callback;
    {
      MutexLock lock(mutex_);
      if (cleaned_up_) return;
      if (callbacks_.empty()) {
        cleaned_up_ = true;
        return;
      }
      auto it = callbacks_.begin();
      object_and_callback = *it;
      callbacks_.erase(it);
    }
    // Invoke cleanup callbacks outside mutex_ so callbacks that acquire their
    // own mutex (e.g., FutureBase::Release) and call UnregisterObject cannot
    // invert lock order with mutex_.
    object_and_callback.second(object_and_callback.first);
  }
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
