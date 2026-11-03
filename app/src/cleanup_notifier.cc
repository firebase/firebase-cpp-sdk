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

CleanupNotifier::CleanupNotifier() : cleaned_up_(false) {}

CleanupNotifier::~CleanupNotifier() {
  CleanupAll();
  UnregisterAllOwners();
}

void CleanupNotifier::RegisterObject(void *object, CleanupCallback callback) {
  std::scoped_lock lock(mutex_);
  cleaned_up_ = false;
  callbacks_[object] = std::move(callback);
}

void CleanupNotifier::UnregisterObject(void *object) {
  std::unique_lock<std::mutex> lock(mutex_);
  callbacks_.erase(object);
  cv_.wait(lock, [&]() {
    return in_flight_object_ != object ||
           in_flight_thread_ == std::this_thread::get_id();
  });
}

void CleanupNotifier::CleanupAll() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (true) {
    cv_.wait(lock, [&]() {
      return in_flight_object_ == nullptr ||
             in_flight_thread_ == std::this_thread::get_id();
    });
    if (callbacks_.empty()) {
      cleaned_up_ = true;
      break;
    }
    auto it = callbacks_.begin();
    void* obj = it->first;
    CleanupCallback cb = std::move(it->second);
    callbacks_.erase(it);
    void* prev_in_flight_object = in_flight_object_;
    std::thread::id prev_in_flight_thread = in_flight_thread_;
    in_flight_object_ = obj;
    in_flight_thread_ = std::this_thread::get_id();
    lock.unlock();

    if (cb) {
      cb(obj);
    }

    lock.lock();
    in_flight_object_ = prev_in_flight_object;
    in_flight_thread_ = prev_in_flight_thread;
    cv_.notify_all();
  }
}

void CleanupNotifier::UnregisterAllOwners() {
  std::scoped_lock lock(GetOwnersMutex());
  while (!owners_.empty()) {
    UnregisterOwner(GetOwnersMap().find(owners_[0]));
  }
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
  std::scoped_lock lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  return it != map.end() ? it->second : nullptr;
}

}  // namespace firebase
