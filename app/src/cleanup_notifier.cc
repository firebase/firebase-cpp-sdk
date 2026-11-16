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

#include <assert.h>

#include <algorithm>

#include "app/src/no_destructor.h"

namespace firebase {
namespace {

using CleanupNotifierMap = std::map<void*, CleanupNotifier*>;

// Guards CleanupNotifiersByOwner() and CleanupNotifier::owners_. Wrapped in
// NoDestructor so static initialization and exit-time destructor unregistration
// never observe an uninitialized or destroyed mutex/map.
Mutex& CleanupNotifiersMutex() {
  static NoDestructor<Mutex> mutex;
  return *mutex;
}

CleanupNotifierMap& CleanupNotifiersByOwner() {
  static NoDestructor<CleanupNotifierMap> notifiers;
  return *notifiers;
}

}  // namespace

CleanupNotifier::CleanupNotifier() : cleaned_up_(false) {}

CleanupNotifier::~CleanupNotifier() {
  CleanupAll();
  UnregisterAllOwners();
}

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
  MutexLock lock(mutex_);
  if (!cleaned_up_) {
    while (callbacks_.begin() != callbacks_.end()) {
      std::pair<void*, CleanupCallback> object_and_callback =
          *callbacks_.begin();
      object_and_callback.second(object_and_callback.first);
      UnregisterObject(object_and_callback.first);
    }
    cleaned_up_ = true;
  }
}

void CleanupNotifier::UnregisterAllOwners() {
  MutexLock lock(CleanupNotifiersMutex());
  while (owners_.begin() != owners_.end()) {
    UnregisterOwner(this, owners_[0]);
  }
}

void CleanupNotifier::RegisterOwner(CleanupNotifier* notifier, void* owner) {
  MutexLock lock(CleanupNotifiersMutex());
  auto& notifiers = CleanupNotifiersByOwner();
  auto it = notifiers.find(owner);
  if (it != notifiers.end()) UnregisterOwner(it);
  notifiers[owner] = notifier;
  notifier->owners_.push_back(owner);
}

void CleanupNotifier::UnregisterOwner(CleanupNotifier* /*notifier*/,
                                      void* owner) {
  MutexLock lock(CleanupNotifiersMutex());
  auto& notifiers = CleanupNotifiersByOwner();
  auto it = notifiers.find(owner);
  if (it != notifiers.end()) UnregisterOwner(it);
}

void CleanupNotifier::UnregisterOwner(
    std::map<void*, CleanupNotifier*>::iterator it) {
  MutexLock lock(CleanupNotifiersMutex());
  void* owner = it->first;
  CleanupNotifier* notifier = it->second;
  CleanupNotifiersByOwner().erase(it);
  auto* owners = &notifier->owners_;
  auto owner_it = std::find(owners->begin(), owners->end(), owner);
  assert(owner_it != owners->end());
  owners->erase(owner_it);
}

CleanupNotifier* CleanupNotifier::FindByOwner(void* owner) {
  MutexLock lock(CleanupNotifiersMutex());
  auto& notifiers = CleanupNotifiersByOwner();
  auto it = notifiers.find(owner);
  return it != notifiers.end() ? it->second : nullptr;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
