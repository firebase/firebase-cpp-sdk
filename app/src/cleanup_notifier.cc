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
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace firebase {

namespace {

struct NotifierState {
  void* in_flight_object = nullptr;
  std::thread::id in_flight_thread;
  std::condition_variable cv;
};

std::mutex& GetStateMutex() {
  static std::mutex* m = new std::mutex();
  return *m;
}

std::map<CleanupNotifier*, std::unique_ptr<NotifierState>>& GetStatesMap() {
  static auto* m = new std::map<CleanupNotifier*, std::unique_ptr<NotifierState>>();
  return *m;
}

std::mutex& GetOwnersMutex() {
  static std::mutex* m = new std::mutex();
  return *m;
}

std::map<void *, CleanupNotifier *>& GetOwnersMap() {
  static auto* m = new std::map<void *, CleanupNotifier *>();
  return *m;
}

}  // namespace

CleanupNotifier::CleanupNotifier() : cleaned_up_(false) {}

CleanupNotifier::~CleanupNotifier() {
  CleanupAll();
  UnregisterAllOwners();
}

void CleanupNotifier::RegisterObject(void *object, CleanupCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_[object] = std::move(callback);
}

void CleanupNotifier::UnregisterObject(void *object) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(object);
  }
  std::unique_lock<std::mutex> state_lock(GetStateMutex());
  GetStatesMap()[this]->cv.wait(state_lock, [&]() {
    auto it = GetStatesMap().find(this);
    if (it == GetStatesMap().end()) return true;
    auto& state = *(it->second);
    if (state.in_flight_object == object && state.in_flight_thread != std::this_thread::get_id()) {
      return false;
    }
    return true;
  });
}

void CleanupNotifier::CleanupAll() {
  bool needs_cleanup = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cleaned_up_) {
      cleaned_up_ = true;
      needs_cleanup = true;
    }
  }
  if (!needs_cleanup) return;

  {
    std::lock_guard<std::mutex> state_lock(GetStateMutex());
    GetStatesMap()[this] = std::make_unique<NotifierState>();
  }

  while (true) {
    void* obj = nullptr;
    CleanupCallback cb = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (callbacks_.empty()) break;
      auto it = callbacks_.begin();
      obj = it->first;
      cb = std::move(it->second);
      callbacks_.erase(it);
    }
    
    {
      std::lock_guard<std::mutex> state_lock(GetStateMutex());
      auto& state = *(GetStatesMap()[this]);
      state.in_flight_object = obj;
      state.in_flight_thread = std::this_thread::get_id();
    }
    
    if (cb) {
      cb(obj);
    }
    
    {
      std::lock_guard<std::mutex> state_lock(GetStateMutex());
      auto& state = *(GetStatesMap()[this]);
      state.in_flight_object = nullptr;
      state.in_flight_thread = std::thread::id();
      state.cv.notify_all();
    }
  }
  
  {
    std::lock_guard<std::mutex> state_lock(GetStateMutex());
    GetStatesMap().erase(this);
  }
}

void CleanupNotifier::UnregisterAllOwners() {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  while (owners_.begin() != owners_.end()) {
    UnregisterOwner(GetOwnersMap().find(owners_[0]));
  }
}

void CleanupNotifier::RegisterOwner(CleanupNotifier *notifier, void *owner) {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  if (it != map.end()) UnregisterOwner(it);
  map[owner] = notifier;
  notifier->owners_.push_back(owner);
}

void CleanupNotifier::UnregisterOwner(CleanupNotifier *notifier, void *owner) {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  if (it != map.end()) UnregisterOwner(it);
}

void CleanupNotifier::UnregisterOwner(
    std::map<void *, CleanupNotifier *>::iterator it) {
  auto& map = GetOwnersMap();
  if (it == map.end()) return;
  void *owner = it->first;
  CleanupNotifier *notifier = it->second;
  map.erase(it);
  auto *owners = &notifier->owners_;
  auto owner_it = std::find(owners->begin(), owners->end(), owner);
  if (owner_it != owners->end()) {
    owners->erase(owner_it);
  }
}

CleanupNotifier *CleanupNotifier::FindByOwner(void *owner) {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  return it != map.end() ? it->second : nullptr;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
