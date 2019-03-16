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

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

Mutex CleanupNotifier::cleanup_notifiers_by_owner_mutex_;  // NOLINT
std::map<void *, CleanupNotifier *>
    *CleanupNotifier::cleanup_notifiers_by_owner_;

CleanupNotifier::CleanupNotifier() : cleaned_up_(false) {
  MutexLock lock(cleanup_notifiers_by_owner_mutex_);
  if (!cleanup_notifiers_by_owner_) {
    cleanup_notifiers_by_owner_ = new std::map<void *, CleanupNotifier *>();
  }
}

CleanupNotifier::~CleanupNotifier() {
  CleanupAll();
  UnregisterAllOwners();
  {
    MutexLock lock(cleanup_notifiers_by_owner_mutex_);
    if (cleanup_notifiers_by_owner_ && cleanup_notifiers_by_owner_->empty()) {
      delete cleanup_notifiers_by_owner_;
      cleanup_notifiers_by_owner_ = nullptr;
    }
  }
}

void CleanupNotifier::RegisterObject(void *object, CleanupCallback callback) {
  MutexLock lock(mutex_);
  auto i = callbacks_.find(object);
  if (i != callbacks_.end()) {
    i->second = callback;
  } else {
    callbacks_.insert(std::make_pair(object, callback));
  }
}

void CleanupNotifier::UnregisterObject(void *object) {
  MutexLock lock(mutex_);
  callbacks_.erase(object);
}

void CleanupNotifier::CleanupAll() {
  MutexLock lock(mutex_);
  if (!cleaned_up_) {
    while (callbacks_.begin() != callbacks_.end()) {
      std::pair<void *, CleanupCallback> object_and_callback =
        *callbacks_.begin();
      object_and_callback.second(object_and_callback.first);
      UnregisterObject(object_and_callback.first);
    }
    cleaned_up_ = true;
  }
}

void CleanupNotifier::UnregisterAllOwners() {
  MutexLock lock(cleanup_notifiers_by_owner_mutex_);
  while (owners_.begin() != owners_.end()) {
    UnregisterOwner(this, owners_[0]);
  }
}

void CleanupNotifier::RegisterOwner(CleanupNotifier *notifier, void *owner) {
  MutexLock lock(cleanup_notifiers_by_owner_mutex_);
  assert(cleanup_notifiers_by_owner_);
  auto it = cleanup_notifiers_by_owner_->find(owner);
  if (it != cleanup_notifiers_by_owner_->end()) UnregisterOwner(it);
  (*cleanup_notifiers_by_owner_)[owner] = notifier;
  notifier->owners_.push_back(owner);
}

void CleanupNotifier::UnregisterOwner(CleanupNotifier *notifier, void *owner) {
  MutexLock lock(cleanup_notifiers_by_owner_mutex_);
  assert(cleanup_notifiers_by_owner_);
  auto it = cleanup_notifiers_by_owner_->find(owner);
  if (it != cleanup_notifiers_by_owner_->end()) UnregisterOwner(it);
}

void CleanupNotifier::UnregisterOwner(
    std::map<void *, CleanupNotifier *>::iterator it) {
  MutexLock lock(cleanup_notifiers_by_owner_mutex_);
  assert(cleanup_notifiers_by_owner_);
  void *owner = it->first;
  CleanupNotifier *notifier = it->second;
  cleanup_notifiers_by_owner_->erase(it);
  auto *owners = &notifier->owners_;
  auto owner_it = std::find(owners->begin(), owners->end(), owner);
  assert(owner_it != owners->end());
  owners->erase(owner_it);
}

CleanupNotifier *CleanupNotifier::FindByOwner(void *owner) {
  MutexLock lock(cleanup_notifiers_by_owner_mutex_);
  if (!cleanup_notifiers_by_owner_) return nullptr;
  auto it = cleanup_notifiers_by_owner_->find(owner);
  return it != cleanup_notifiers_by_owner_->end() ? it->second : nullptr;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
