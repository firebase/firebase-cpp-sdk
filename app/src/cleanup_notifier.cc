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
#include <map>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace firebase {

namespace {

template <typename T>
class NoDestructor {
 public:
  NoDestructor() { new (storage_) T(); }
  T& get() { return *reinterpret_cast<T*>(storage_); }

 private:
  alignas(T) char storage_[sizeof(T)];
};

std::mutex& GetOwnersMutex() {
  static NoDestructor<std::mutex> m;
  return m.get();
}

std::map<void *, CleanupNotifier *>& GetOwnersMap() {
  static NoDestructor<std::map<void *, CleanupNotifier *>> m;
  return m.get();
}
}  // namespace

CleanupNotifier::CleanupNotifier() = default;

CleanupNotifier::~CleanupNotifier() {
  CleanupAll();
  UnregisterAllOwners();
}

void CleanupNotifier::RegisterObject(void *object, CleanupCallback callback) {
  std::scoped_lock lock(mutex_);
  callbacks_[object] = std::move(callback);
}

void CleanupNotifier::UnregisterObject(void *object) {
  std::unique_lock<std::mutex> lock(mutex_);
  callbacks_.erase(object);
  cv_.wait(lock, [&]() {
    if (in_flight_thread_ == std::this_thread::get_id()) return true;
    return std::find(in_flight_objects_.begin(), in_flight_objects_.end(),
                     object) == in_flight_objects_.end();
  });
}

void CleanupNotifier::CleanupAll() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (true) {
    cv_.wait(lock, [&]() {
      return in_flight_objects_.empty() ||
             in_flight_thread_ == std::this_thread::get_id();
    });
    if (callbacks_.empty()) {
      break;
    }
    auto it = callbacks_.begin();
    void* obj = it->first;
    CleanupCallback cb = std::move(it->second);
    callbacks_.erase(it);
    in_flight_objects_.push_back(obj);
    in_flight_thread_ = std::this_thread::get_id();
    lock.unlock();

    struct InFlightGuard {
      CleanupNotifier* self;
      std::unique_lock<std::mutex>& lock;
      ~InFlightGuard() {
        lock.lock();
        self->in_flight_objects_.pop_back();
        if (self->in_flight_objects_.empty()) {
          self->in_flight_thread_ = std::thread::id();
        }
        self->cv_.notify_all();
      }
    } guard{this, lock};

    if (cb) {
      cb(obj);
    }
  }
}

void CleanupNotifier::UnregisterAllOwners() {
  std::scoped_lock lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  for (void* owner : owners_) {
    auto it = map.find(owner);
    if (it != map.end() && it->second == this) {
      map.erase(it);
    }
  }
  owners_.clear();
}

void CleanupNotifier::RegisterOwner(CleanupNotifier *notifier, void *owner) {
  std::scoped_lock lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  if (it != map.end()) UnregisterOwner(it);
  map[owner] = notifier;
  notifier->owners_.push_back(owner);
}

void CleanupNotifier::UnregisterOwner(CleanupNotifier *notifier, void *owner) {
  std::scoped_lock lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  if (it != map.end() && it->second == notifier) {
    UnregisterOwner(it);
  }
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
  std::scoped_lock lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  return it != map.end() ? it->second : nullptr;
}

}  // namespace firebase
