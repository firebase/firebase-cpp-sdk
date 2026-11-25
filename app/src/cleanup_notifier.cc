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

namespace {

void InvokeUntypedCallback(void* object,
                           CleanupNotifier::GenericFnPtr callback) {
  reinterpret_cast<CleanupNotifier::CleanupCallback>(callback)(object);
}

}  // namespace

CleanupNotifier::CleanupNotifier() = default;

CleanupNotifier::~CleanupNotifier() { CleanupAll(); }

void CleanupNotifier::RegisterObject(void* object, CleanupCallback callback) {
  RegisterObject(object, &InvokeUntypedCallback,
                 reinterpret_cast<GenericFnPtr>(callback));
}

void CleanupNotifier::RegisterObject(void* object,
                                     TrampolineCallback trampoline,
                                     GenericFnPtr callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (cleaned_up_) return;
  callbacks_[object] = CallbackEntry{trampoline, callback};
}

void CleanupNotifier::UnregisterObject(void* object) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = callbacks_.find(object);
  if (it != callbacks_.end()) {
    callbacks_.erase(it);
    return;
  }
  // If CleanupAll() is currently executing this object's cleanup callback on
  // another thread, wait until that callback completes so the caller cannot
  // destroy `object` while CleanupAll() is still dereferencing it.
  // If called re-entrantly from the same thread inside the cleanup callback,
  // return immediately to avoid self-deadlock.
  const std::thread::id self = std::this_thread::get_id();
  cv_.wait(lock, [this, object, self]() {
    return in_flight_object_ != object || in_flight_thread_ == self;
  });
}

void CleanupNotifier::CleanupAll() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (cleaned_up_) {
    const std::thread::id self = std::this_thread::get_id();
    cv_.wait(lock, [this, self]() {
      return in_flight_object_ == nullptr || in_flight_thread_ == self;
    });
    return;
  }
  cleaned_up_ = true;
  while (!callbacks_.empty()) {
    auto it = callbacks_.begin();
    void* object = it->first;
    CallbackEntry entry = it->second;
    callbacks_.erase(it);
    in_flight_object_ = object;
    in_flight_thread_ = std::this_thread::get_id();
    lock.unlock();
    entry.trampoline(object, entry.callback);
    lock.lock();
    in_flight_object_ = nullptr;
    in_flight_thread_ = std::thread::id{};
    cv_.notify_all();
  }
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
