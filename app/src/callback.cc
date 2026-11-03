/*
 * Copyright 2016 Google LLC
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

#include "app/src/callback.h"

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"

namespace firebase {
namespace callback {

class CallbackEntry;

// Process-wide monotonic ID generator so callback reference handles are never
// reused across CallbackDispatcher lifecycle resets (preventing ABA collisions).
static std::atomic<uintptr_t> g_next_callback_id{0};

class CallbackQueue : public std::list<std::shared_ptr<CallbackEntry>> {
 public:
  CallbackQueue() {}
  ~CallbackQueue() {}

  // Get the mutex that controls access to this queue.
  Mutex* mutex() { return &mutex_; }

 private:
  Mutex mutex_;
};

// Entry within the callback queue.
class CallbackEntry {
 public:
  // Construct an entry with a reference to the specified callback object.
  // callback_mutex is used to synchronize callback_ and executing_ state
  // transitions.
  CallbackEntry(Callback* callback, Mutex* callback_mutex, uintptr_t id)
      : callback_(callback),
        mutex_(callback_mutex),
        executing_(false),
        id_(id) {}

  // Destroy the callback if it has not already been disabled or executed.
  ~CallbackEntry() { DisableCallback(); }

  uintptr_t id() const { return id_; }

  // Execute the callback associated with this entry.
  // Returns true if a callback was associated with this entry and was executed,
  // false otherwise.
  bool Execute() {
    {
      MutexLock lock(*mutex_);
      if (!callback_) return false;
      executing_ = true;
    }

    callback_->Run();

    {
      MutexLock lock(*mutex_);
      executing_ = false;
    }

    // Note: The implementation of BlockingCallback below relies on the
    // callback being disabled after being run. If that changes, please
    // make sure to also update BlockingCallback.
    DisableCallback();

    return true;
  }

  // Remove and delete the callback object from this entry if it is not
  // currently executing.
  bool DisableCallback() {
    Callback* callback_to_delete = nullptr;
    {
      MutexLock lock(*mutex_);
      if (executing_ || !callback_) return false;
      callback_to_delete = callback_;
      callback_ = nullptr;
    }
    delete callback_to_delete;
    return true;
  }

 private:
  // Callback to call from PollCallbacks().
  Callback* callback_;
  // Mutex that is held when modifying callback_ and executing_.
  Mutex* mutex_;
  // A flag set to true while callback_ is executing.
  bool executing_;
  // Unique opaque identifier for this entry.
  uintptr_t id_;
};

// Dispatches a queue of callbacks.
class CallbackDispatcher {
 public:
  CallbackDispatcher() {}

  ~CallbackDispatcher() {
    std::vector<std::shared_ptr<CallbackEntry>> entries_to_destroy;
    {
      MutexLock lock(*queue_.mutex());
      // Move all remaining callbacks out of this dispatcher's queue before
      // destroying them outside the queue mutex.
      size_t remaining_callbacks = queue_.size();
      if (remaining_callbacks) {
        LogWarning("Callback dispatcher shut down with %zu pending callbacks",
                   remaining_callbacks);
      }
      entries_to_destroy.reserve(remaining_callbacks);
      while (!queue_.empty()) {
        entries_to_destroy.push_back(std::move(queue_.back()));
        queue_.pop_back();
      }
    }
    entries_to_destroy.clear();
  }

  // Add a callback to the dispatch queue returning a reference
  // to the entry which can optionally be removed prior to dispatch.
  void* AddCallback(Callback* callback) {
    MutexLock lock(*queue_.mutex());
    uintptr_t id = ++g_next_callback_id;
    while (id == 0) {
      id = ++g_next_callback_id;
    }
    auto entry =
        std::make_shared<CallbackEntry>(callback, &execution_mutex_, id);
    queue_.push_back(std::move(entry));
    return reinterpret_cast<void*>(id);
  }

  // Remove the callback entry with the specified reference from the queue.
  // Returns true if the callback was still in the queue and removed, false
  // otherwise.
  bool DisableCallback(void* callback_reference) {
    if (!callback_reference) {
      return false;
    }
    uintptr_t target_id = reinterpret_cast<uintptr_t>(callback_reference);
    std::shared_ptr<CallbackEntry> entry_to_disable;
    {
      MutexLock lock(*queue_.mutex());
      for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if (*it && (*it)->id() == target_id) {
          entry_to_disable = std::move(*it);
          queue_.erase(it);
          break;
        }
      }
    }
    if (entry_to_disable) {
      entry_to_disable->DisableCallback();
      return true;
    }
    return false;
  }

  // Dispatch queued callbacks returning the number of callbacks that were
  // dispatched and removed from the queue.
  int DispatchCallbacks() {
    int dispatched = 0;
    Mutex* queue_mutex = queue_.mutex();
    queue_mutex->Acquire();
    while (!queue_.empty()) {
      // Move the std::shared_ptr out of the queue before unlocking.
      std::shared_ptr<CallbackEntry> callback_entry = std::move(queue_.front());
      queue_.pop_front();
      queue_mutex->Release();
      if (callback_entry) {
        callback_entry->Execute();
        callback_entry.reset();
      }
      dispatched++;
      queue_mutex->Acquire();
    }
    queue_mutex->Release();
    return dispatched;
  }

  // Flush pending callbacks from the queue without executing them.
  // If flushed_entries is provided, moved entries are appended to it so the
  // caller can destroy them after releasing outer locks.
  int FlushCallbacks(
      std::vector<std::shared_ptr<CallbackEntry>>* flushed_entries = nullptr) {
    std::vector<std::shared_ptr<CallbackEntry>> local_flushed;
    std::vector<std::shared_ptr<CallbackEntry>>& target =
        flushed_entries ? *flushed_entries : local_flushed;
    int flushed = 0;
    {
      MutexLock lock(*queue_.mutex());
      target.reserve(target.size() + queue_.size());
      while (!queue_.empty()) {
        target.push_back(std::move(queue_.front()));
        queue_.pop_front();
        flushed++;
      }
    }
    return flushed;
  }

 private:
  CallbackQueue queue_;
  // Mutex that synchronizes CallbackEntry::callback_ and executing_ state.
  Mutex execution_mutex_;
};

static std::shared_ptr<CallbackDispatcher>* g_callback_dispatcher =
    new std::shared_ptr<CallbackDispatcher>();
// Mutex that controls access to g_callback_dispatcher and g_callback_ref_count.
static Mutex* g_callback_mutex = new Mutex();
static int g_callback_ref_count = 0;
static int g_explicit_init_count = 0;
static Thread::Id g_callback_thread_id;
static bool g_callback_thread_id_initialized = false;

static bool IsCurrentCallbackThread() {
  MutexLock lock(*g_callback_mutex);
  return g_callback_thread_id_initialized &&
         Thread::IsCurrentThread(g_callback_thread_id);
}

static void AddReferenceLocked() {
  if (g_callback_ref_count == 0) {
    *g_callback_dispatcher = std::make_shared<CallbackDispatcher>();
  }
  ++g_callback_ref_count;
}

void Initialize() {
  MutexLock lock(*g_callback_mutex);
  AddReferenceLocked();
  ++g_explicit_init_count;
}

// Add a reference to the module if it's already initialized.
static std::shared_ptr<CallbackDispatcher> AcquireDispatcherIfInitialized() {
  MutexLock lock(*g_callback_mutex);
  if (g_callback_ref_count > 0) {
    AddReferenceLocked();
    return *g_callback_dispatcher;
  }
  return nullptr;
}

bool IsInitialized() {
  MutexLock lock(*g_callback_mutex);
  return g_callback_ref_count > 0;
}

// Remove number_of_references_to_remove from the module, clean up if the
// reference count reaches 0, do nothing if the reference count is already 0.
static void Terminate(int number_of_references_to_remove) {
  std::shared_ptr<CallbackDispatcher> dispatcher_to_destroy;
  {
    MutexLock lock(*g_callback_mutex);
    if (!g_callback_ref_count) {
      LogWarning("Callback module already shut down");
      return;
    }
    g_callback_ref_count =
        g_callback_ref_count - number_of_references_to_remove;
    if (g_callback_ref_count < 0) {
      LogDebug("WARNING: Callback module ref count = %d", g_callback_ref_count);
    }
    g_callback_ref_count = g_callback_ref_count < 0 ? 0 : g_callback_ref_count;
    if (g_callback_ref_count == 0) {
      g_explicit_init_count = 0;
      dispatcher_to_destroy = std::move(*g_callback_dispatcher);
      g_callback_dispatcher->reset();
    }
  }
  // Destroy outside the lock.
  dispatcher_to_destroy.reset();
}

void Terminate(bool flush_all) {
  std::shared_ptr<CallbackDispatcher> dispatcher;
  int ref_count = 0;
  {
    MutexLock lock(*g_callback_mutex);
    if (!g_callback_ref_count) {
      LogWarning("Callback module already shut down");
      return;
    }
    if (flush_all) {
      dispatcher = *g_callback_dispatcher;
      if (g_explicit_init_count > 0) {
        --g_explicit_init_count;
        ++ref_count;
      }
    } else {
      if (g_explicit_init_count > 0) {
        --g_explicit_init_count;
      }
      ref_count = 1;
    }
  }
  std::vector<std::shared_ptr<CallbackEntry>> flushed_entries;
  if (flush_all && dispatcher) {
    ref_count += dispatcher->FlushCallbacks(&flushed_entries);
  }
  dispatcher.reset();
  flushed_entries.clear();
  if (ref_count > 0) {
    Terminate(ref_count);
  }
}

void* AddCallback(Callback* callback) {
  if (!callback) {
    return nullptr;
  }
  MutexLock lock(*g_callback_mutex);
  AddReferenceLocked();
  return (*g_callback_dispatcher)->AddCallback(callback);
}

// TODO(chkuang): remove this once we properly implement C++->C# log callback.
void* AddCallbackWithThreadCheck(Callback* callback) {
  if (!callback) {
    return nullptr;
  }
  if (IsCurrentCallbackThread()) {
    // If we are on the callback thread, we can safely execute the callback
    // right away and blocking would be a deadlock, so we run it.
    callback->Run();
    delete callback;
    return nullptr;
  } else {
    return AddCallback(callback);
  }
}

// A blocking callback posts a semaphore after being done.
// This allows the caller to wait for its completion.
class BlockingCallback : public Callback {
 public:
  BlockingCallback(Callback* callback, Semaphore* sem)
      : callback_(callback), sem_(sem) {}
  ~BlockingCallback() override {
    delete callback_;
    // CallbackEntry does delete callbacks after running them.
    // By posting here after deleting callback_, we make sure that the
    // caller's stack-allocated Semaphore stays valid until destruction
    // finishes, even if the callback is removed from the queue.
    sem_->Post();
  }
  void Run() override {
    if (callback_) {
      callback_->Run();
    }
  }

 private:
  Callback* callback_;  // owned
  Semaphore* sem_;      // not owned
};

void AddBlockingCallback(Callback* callback) {
  if (!callback) {
    return;
  }
  if (IsCurrentCallbackThread()) {
    // If we are on the callback thread, we can safely execute the callback
    // right away and blocking would be a deadlock, so we run it.
    callback->Run();
    delete callback;
  } else {
    Semaphore sem(0);
    AddCallback(new BlockingCallback(callback, &sem));
    sem.Wait();
  }
}

void RemoveCallback(void* callback_reference) {
  // Increase the reference count for the module so that it isn't cleaned up
  // while dispatching callbacks.
  std::shared_ptr<CallbackDispatcher> dispatcher =
      AcquireDispatcherIfInitialized();
  if (dispatcher) {
    bool removed = dispatcher->DisableCallback(callback_reference);
    Terminate(removed ? 2 : 1);
  }
}

void PollCallbacks() {
  // Increase the reference count for the module so that it isn't cleaned up
  // while dispatching callbacks.
  std::shared_ptr<CallbackDispatcher> dispatcher =
      AcquireDispatcherIfInitialized();
  if (dispatcher) {
    {
      MutexLock lock(*g_callback_mutex);
      // We intentionally do NOT lazy-initialize the callback_thread_id, so that
      // it is updated in case the polling thread is destroyed and recreated.
      g_callback_thread_id = Thread::CurrentId();
      g_callback_thread_id_initialized = true;
    }
    // Execute callbacks.
    int dispatched = dispatcher->DispatchCallbacks();
    // +1 added to the references to remove as we added a reference in
    // AcquireDispatcherIfInitialized().
    Terminate(dispatched + 1);
  }
}

}  // namespace callback
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
