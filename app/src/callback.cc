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

#include <list>

#include "app/memory/shared_ptr.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace callback {

class CallbackEntry;

class CallbackQueue : public std::list<SharedPtr<CallbackEntry>> {
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
  // Construct a entry with a reference to the specified callback object.
  // callback_mutex_ is used to enforce a critical section for callback
  // execution and destruction.
  CallbackEntry(Callback* callback, Mutex* callback_mutex)
      : callback_(callback), mutex_(callback_mutex), executing_(false) {}

  // Destroy the callback.  This blocks if the callback is currently
  // executing.
  ~CallbackEntry() { DisableCallback(); }

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

  // Remove the callback method from this entry.
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
  // A flag set to true when callback_ is about to be called.
  bool executing_;
};

// Dispatches a queue of callbacks.
class CallbackDispatcher {
 public:
  CallbackDispatcher() {}

  ~CallbackDispatcher() {
    MutexLock lock(*queue_.mutex());
    // Destroy all callbacks in this dispatcher's queue.
    size_t remaining_callbacks = queue_.size();
    if (remaining_callbacks) {
      LogWarning("Callback dispatcher shut down with %d pending callbacks",
                 remaining_callbacks);
    }
    while (!queue_.empty()) {
      queue_.back().reset();
      queue_.pop_back();
    }
  }

  // Add a callback to the dispatch queue returning a reference
  // to the entry which can be optionally be removed prior to dispatch.
  void* AddCallback(Callback* callback) {
    auto entry = MakeShared<CallbackEntry>(callback, &execution_mutex_);
    MutexLock lock(*queue_.mutex());
    queue_.push_back(entry);
    return entry.get();
  }

  // Remove the callback reference from the specified entry.
  // Returns true if a reference to the callback was removed, false if it
  // was already not present on the specified entry.
  // NOTE: This does not remove the callback from the execution queue.
  // The queue is flushed on a call to DispatchCallbacks().
  bool DisableCallback(void* callback_reference) {
    MutexLock lock(*queue_.mutex());
    CallbackEntry* callback_entry =
        static_cast<CallbackEntry*>(callback_reference);
    return callback_entry->DisableCallback();
  }

  // Dispatch queued callbacks returning the number of callbacks that were
  // dispatched and removed from the queue.
  int DispatchCallbacks() {
    int dispatched = 0;
    Mutex* queue_mutex = queue_.mutex();
    queue_mutex->Acquire();
    while (!queue_.empty()) {
      // Make a copy of the SharedPtr in case FlushCallbacks() is called
      // currently.
      SharedPtr<CallbackEntry> callback_entry = queue_.front();
      queue_.pop_front();
      queue_mutex->Release();
      callback_entry->Execute();
      dispatched++;
      queue_mutex->Acquire();
      callback_entry.reset();
    }
    queue_mutex->Release();
    return dispatched;
  }

  // Flush pending callbacks from the queue without executing them.
  int FlushCallbacks() {
    int flushed = 0;
    MutexLock lock(*queue_.mutex());
    while (!queue_.empty()) {
      queue_.front().reset();
      queue_.pop_front();
      flushed++;
    }
    return flushed;
  }

 private:
  CallbackQueue queue_;
  // Mutex that is held for the duration of each callback.  This prevents the
  // destruction of a callback until execution is complete.
  Mutex execution_mutex_;
};

static CallbackDispatcher* g_callback_dispatcher = nullptr;
// Mutex that controls access to g_callback_dispatcher and g_callback_ref_count.
static Mutex g_callback_mutex;  // NOLINT
static int g_callback_ref_count = 0;
static Thread::Id g_callback_thread_id;
static bool g_callback_thread_id_initialized = false;

void Initialize() {
  MutexLock lock(g_callback_mutex);
  if (g_callback_ref_count == 0) {
    g_callback_dispatcher = new CallbackDispatcher();
  }
  ++g_callback_ref_count;
}

// Add a reference to the module if it's already initialized.
static bool InitializeIfInitialized() {
  MutexLock lock(g_callback_mutex);
  if (IsInitialized()) {
    Initialize();
    return true;
  }
  return false;
}

bool IsInitialized() { return g_callback_ref_count > 0; }

// Remove number_of_references_to_remove from the module, clean up if the
// reference count reaches 0, do nothing if the reference count is already 0.
static void Terminate(int number_of_references_to_remove) {
  CallbackDispatcher* dispatcher_to_destroy = nullptr;
  {
    MutexLock lock(g_callback_mutex);
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
      dispatcher_to_destroy = g_callback_dispatcher;
      g_callback_dispatcher = nullptr;
    }
  }
  // This method can be called a "lot" so only call delete if we really have
  // something to delete.
  if (dispatcher_to_destroy) delete dispatcher_to_destroy;
}

void Terminate(bool flush_all) {
  MutexLock lock(g_callback_mutex);
  int ref_count = 1;
  // g_callback_ref_count is used to track the number of current references to
  // g_callback_dispatcher so we need to decrement the reference count by just
  // the outstanding number of items in the queue.  In particular,
  // PollDispatcher() could be executing at this point since g_callback_mutex
  // isn't held by the ref count is > 0 for the duration of the function.
  if (flush_all && g_callback_dispatcher) {
    ref_count += g_callback_dispatcher->FlushCallbacks();
  }
  Terminate(ref_count);
}

void* AddCallback(Callback* callback) {
  MutexLock lock(g_callback_mutex);
  Initialize();
  return g_callback_dispatcher->AddCallback(callback);
}

// TODO(chkuang): remove this once we properly implement C++->C# log callback.
void* AddCallbackWithThreadCheck(Callback* callback) {
  if (g_callback_thread_id_initialized &&
      Thread::IsCurrentThread(g_callback_thread_id)) {
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
    // CallbackEntry does delete callbacks after running them.
    // By posting here, we make sure that the semaphore does get posted even
    // if the callback is removed from the queue for some other reason.
    sem_->Post();
    delete callback_;
  }
  void Run() override {
    callback_->Run();
  }
 private:
  Callback* callback_;  // owned
  Semaphore* sem_;  // not owned
};

void AddBlockingCallback(Callback* callback) {
  if (g_callback_thread_id_initialized &&
      Thread::IsCurrentThread(g_callback_thread_id)) {
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
  if (InitializeIfInitialized()) {
    // This only removes the Callback from the CallbackEntry and does *not*
    // remove the CallbackEntry from the queue so we don't need an additional
    // Terminate() here to decrement the reference count that was added by
    // AddCallback().
    g_callback_dispatcher->DisableCallback(callback_reference);
    Terminate(false);
  }
}

void PollCallbacks() {
  // Increase the reference count for the module so that it isn't cleaned up
  // while dispatching callbacks.
  if (InitializeIfInitialized()) {
    // We intentionally do NOT lazy-initialize the callback_thread_id, so that
    // it is updated in case the polling thread is destroyed and recreated.
    // Caveat: if that happens, there's a possibility that AddBlockingCallback
    // does not realize that it's running on the callback thread and deadlocks.
    g_callback_thread_id = Thread::CurrentId();
    g_callback_thread_id_initialized = true;
    // Execute callbacks.
    int dispatched = g_callback_dispatcher->DispatchCallbacks();
    // +1 added to the references to remove as we added a reference in
    // InitializeIfInitialized().
    Terminate(dispatched + 1);
  }
}

}  // namespace callback
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
