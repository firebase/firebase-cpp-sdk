/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_SRC_CLEANUP_NOTIFIER_H_
#define FIREBASE_APP_SRC_CLEANUP_NOTIFIER_H_

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {

// If an object gives out other objects that refer back to it, the original
// object can use this CleanupNotifier class to invalidate any other objects it
// gave out when the original object is deleted.
//
// Note: Each object can only have a single callback. If you register a second
// callback to an existing object's pointer, it will override the previous
// callback.
//
// Standard workflow:
// - Owner object has a CleanupNotifier in it.
// - Owned objects register themselves with their owner's CleanupNotifier when
//   they are created, and unregister themselves when they are deleted.
// - If the owner object is deleted before any owned objects, CleanupNotifier
//   will call each object's callback so they can remove any links back to their
//   owner (which is about to be deleted).
class CleanupNotifier {
 public:
  using CleanupCallback = std::function<void(void*)>;

  // Default constructor.
  CleanupNotifier();

  // When the destructor is called, it will call each object's cleanup callback.
  ~CleanupNotifier();

  // Register a callback to be called on a given object when it's time for
  // cleanup. If this object already has a callback registered, the previous
  // callback will be overwritten.
  void RegisterObject(void *object, CleanupCallback callback);

  // Unregister an object. This will remove it from the cleanup list without
  // calling the cleanup callback.
  //
  // If CleanupAll() is running the object's callback on another thread, this
  // blocks until that callback returns, so the caller may then safely destroy
  // the object. Callers must therefore not hold any lock that the callback
  // acquires, except `unlock_while_waiting`, which (if non-null and held by
  // the caller) is released while blocked and re-acquired before returning.
  void UnregisterObject(void *object, Mutex *unlock_while_waiting = nullptr);

  // Call all cleanup callbacks, clearing the list. You can call this manually
  // rather than using the destructor if you want more control over when it
  // executes.
  void CleanupAll();

  // Register an owner with the notifier.
  void RegisterOwner(void *owner) { RegisterOwner(this, owner); }

  // Unregister an owner from the notifier.
  void UnregisterOwner(void *owner) { UnregisterOwner(this, owner); }

  // Find a cleanup notifier by owner object.
  // If a cleanup notifier was registered with an owner object using
  // RegisterOwner().
  static CleanupNotifier *FindByOwner(void *owner);

 private:
  // Register the notifier's association with the owner object.
  // Notifiers can be register to multiple owner objects.
  static void RegisterOwner(CleanupNotifier *notifier, void *owner);

  // Unregister a notifier from an owner object.
  static void UnregisterOwner(CleanupNotifier *notifier, void *owner);

  // Unregister this notifier with all owner objects.
  void UnregisterAllOwners();

  // Unregister a notifier from an owner object.
  static void UnregisterOwner(std::map<void *, CleanupNotifier *>::iterator it);

 private:
  // Guards callbacks_, in_flight_objects_, in_flight_thread_, and waiters_.
  std::mutex mutex_;
  std::map<void *, CleanupCallback> callbacks_;
  std::condition_variable cv_;
  std::vector<void *> in_flight_objects_;
  std::thread::id in_flight_thread_;
  // Number of UnregisterObject() calls blocked on cv_; the destructor waits
  // for them to leave before destroying mutex_ and cv_.
  int waiters_ = 0;
  // List of owner objects registered with this notifier.
  std::vector<void *> owners_;
};

// Typed wrapper for CleanupNotifier. Helpful if you only need to clean
// up one type of object.
template <typename T>
class TypedCleanupNotifier {
 public:
  using CleanupCallback = std::function<void(T *)>;

  void RegisterObject(T *object, void (*callback)(T *)) {
    notifier_.RegisterObject(object, [callback](void *obj) {
      if (callback) callback(static_cast<T *>(obj));
    });
  }

  void RegisterObject(T *object, CleanupCallback callback) {
    notifier_.RegisterObject(object, [cb = std::move(callback)](void *obj) {
      if (cb) cb(static_cast<T *>(obj));
    });
  }

  void UnregisterObject(T *object, Mutex *unlock_while_waiting = nullptr) {
    notifier_.UnregisterObject(object, unlock_while_waiting);
  }

  void CleanupAll() { notifier_.CleanupAll(); }

  // Get the underlying notifier.
  CleanupNotifier &cleanup_notifier() { return notifier_; }

 private:
  CleanupNotifier notifier_;
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_CLEANUP_NOTIFIER_H_
