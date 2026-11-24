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

#include <map>

#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {

// Local per-instance callback list used internally (e.g., by
// ReferenceCountedFutureImpl) to invalidate non-owning handles when their
// backing object is destroyed. Contains no global state.
class CleanupNotifier {
 public:
  typedef void (*CleanupCallback)(void* object);
  typedef void (*GenericFnPtr)();
  typedef void (*TrampolineCallback)(void* object, GenericFnPtr callback);

  CleanupNotifier();
  ~CleanupNotifier();

  void RegisterObject(void* object, CleanupCallback callback);
  void RegisterObject(void* object, TrampolineCallback trampoline,
                      GenericFnPtr callback);
  void UnregisterObject(void* object);
  void CleanupAll();

 private:
  struct CallbackEntry {
    TrampolineCallback trampoline;
    GenericFnPtr callback;
  };

  Mutex mutex_;
  std::map<void*, CallbackEntry> callbacks_;
  bool cleaned_up_;
};

// Typed wrapper for CleanupNotifier.
template <typename T>
class TypedCleanupNotifier {
 public:
  typedef void (*CleanupCallback)(T* object);

  void RegisterObject(T* object, CleanupCallback callback) {
    notifier_.RegisterObject(
        object, &InvokeTypedCallback,
        reinterpret_cast<CleanupNotifier::GenericFnPtr>(callback));
  }

  void UnregisterObject(T* object) { notifier_.UnregisterObject(object); }

  void CleanupAll() { notifier_.CleanupAll(); }

  CleanupNotifier& cleanup_notifier() { return notifier_; }

 private:
  static void InvokeTypedCallback(void* object,
                                  CleanupNotifier::GenericFnPtr callback) {
    reinterpret_cast<CleanupCallback>(callback)(static_cast<T*>(object));
  }

  CleanupNotifier notifier_;
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_CLEANUP_NOTIFIER_H_
