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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_CLEANUP_NOTIFIER_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_CLEANUP_NOTIFIER_H_

#include <map>
#include <vector>

#include "app/src/mutex.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

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
  typedef void (*CleanupCallback)(void *object);

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
  void UnregisterObject(void *object);

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
  // Guards callbacks_ and cleaned_up_.
  Mutex mutex_;
  std::map<void *, CleanupCallback> callbacks_;
  bool cleaned_up_;
  // List of owners of this notifier.
  // This is the inverse of cleanup_notifiers_by_owner_ for a notifier.
  std::vector<void *> owners_;

  // Guards owners_ and cleanup_notifiers_by_owner_.
  static Mutex cleanup_notifiers_by_owner_mutex_;
  // Global map of cleanup notifiers bucketed by owner object.
  static std::map<void *, CleanupNotifier *> *cleanup_notifiers_by_owner_;
};

// Typed wrapper for CleanupNotifier. Helpful if you only need to clean
// up one type of object.
template <typename T>
class TypedCleanupNotifier {
 public:
  typedef void (*CleanupCallback)(T *object);

  void RegisterObject(T *object, CleanupCallback callback) {
    notifier_.RegisterObject(
        object, reinterpret_cast<CleanupNotifier::CleanupCallback>(callback));
  }

  void UnregisterObject(T *object) { notifier_.UnregisterObject(object); }

  void CleanupAll() { notifier_.CleanupAll(); }

  // Get the underlying notifier.
  CleanupNotifier &cleanup_notifier() { return notifier_; }

 private:
  CleanupNotifier notifier_;
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_CLEANUP_NOTIFIER_H_
