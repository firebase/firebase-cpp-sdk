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

#include "app/src/reference_counted_future_impl.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/intrusive_list.h"
#include "app/src/log.h"

// Set this to 1 to enable verbose logging in this module.
#if !defined(FIREBASE_FUTURE_TRACE_ENABLE)
#define FIREBASE_FUTURE_TRACE_ENABLE 0
#endif  // !defined(FIREBASE_FUTURE_TRACE_ENABLE)

#if FIREBASE_FUTURE_TRACE_ENABLE
#define FIREBASE_FUTURE_TRACE(...) LogDebug(__VA_ARGS__)
#else
#define FIREBASE_FUTURE_TRACE(...)
#endif  // FIREBASE_FUTURE_TRACE_ENABLE

namespace firebase {

// See warning at the top of FutureBase's declaration for details.
static_assert(sizeof(FutureBase) == sizeof(Future<int>),
              "Future should not introduce virtual functions or data members.");

typedef void DataDeleteFn(void* data_to_delete);
typedef std::pair<FutureHandleId, FutureBackingData*> BackingPair;

// NOLINTNEXTLINE
const FutureHandle ReferenceCountedFutureImpl::kInvalidHandle(
    kInvalidFutureHandle);

namespace {

// This class manages proxies to a Future.
// The goal is to allow LastResult to return a proxy to a Future, so that we
// don't have to duplicate the asynchronous call, but still have the Futures
// be independent from a user's perspective.
// - The subject Future is the Future that existed first, owns the data and
//   listens to the result of the asynchronous system call.
//   It must stay alive as long as there are clients. (For the data.)
// - There can be multiple client Futures, which complete when the subject
//   completes. They refer to the same data as the subject and they each have
//   their own completion callback.
// This class manages the link between the two.
class FutureProxyManager {
 public:
  FutureProxyManager(ReferenceCountedFutureImpl* api,
                     const FutureHandle& subject)
      : api_(api), subject_(subject.id()) {}

  ~FutureProxyManager() {
    std::vector<FutureHandle> clients_to_release;
    {
      MutexLock lock(mutex_);
      destroying_ = true;
      clients_to_release.swap(clients_);
    }
    for (FutureHandle& h : clients_to_release) {
      if (h != ReferenceCountedFutureImpl::kInvalidHandle) {
        api_->ForceReleaseFuture(h);
      }
    }
  }

  void RegisterClient(const FutureHandle& handle) {
    MutexLock lock(mutex_);
    // We create one reference per client to the Future.
    // This way the ReferenceCountedFutureImpl will do the right thing if one
    // thread tries to unregister the last client while adding a new one.
    // Store detached FutureHandles (api == nullptr) so FutureProxyManager and
    // UnregisterData do not form RAII reference cycles with subject or clients.
    api_->ReferenceFuture(subject_);
    clients_.emplace_back(handle.id());
  }

  struct UnregisterData {
    UnregisterData(FutureProxyManager* proxy, const FutureHandle& handle)
        : proxy(proxy), handle(handle.id()) {}
    FutureProxyManager* proxy;
    FutureHandle handle;
  };

  static void UnregisterCallback(void* data) {
    if (data == nullptr) {
      return;
    }
    UnregisterData* udata = static_cast<UnregisterData*>(data);
    udata->proxy->UnregisterClient(udata->handle);
    delete udata;
  }

  void UnregisterClient(const FutureHandle& handle) {
    ReferenceCountedFutureImpl* api = nullptr;
    FutureHandle subject;
    {
      MutexLock lock(mutex_);
      if (destroying_) {
        return;
      }
      for (FutureHandle& h : clients_) {
        if (h == handle) {
          h = ReferenceCountedFutureImpl::kInvalidHandle;
          api = api_;
          subject = subject_;
          break;
        }
      }
    }
    if (api != nullptr) {
      // Release one reference outside mutex_. This can delete subject_, which
      // in turn will delete `this`, as the subject owns the proxy.
      api->ReleaseFuture(subject);
    }
  }

  std::vector<FutureHandle> GetClients() const {
    MutexLock lock(mutex_);
    return clients_;
  }

 private:
  std::vector<FutureHandle> clients_;
  ReferenceCountedFutureImpl* api_;
  // Detached handle (id only) to the subject; one reference per active client
  // is managed explicitly in RegisterClient/UnregisterClient.
  FutureHandle subject_;
  bool destroying_ = false;
  // mutex to protect register/unregister operation.
  mutable Mutex mutex_;
};

struct CompletionCallbackData {
  // Pointers to the next and previous nodes in the list.
  intrusive_list_node node;

  // The function to call once the future is marked completed.
  FutureBase::CompletionCallback completion_callback;

  // The data to pass into `completion_callback`.
  void* callback_user_data;

  // If set, this function will be called to delete callback_user_data after the
  // callback runs or the Future is destroyed.
  void (*callback_user_data_delete_fn)(void*);

  CompletionCallbackData(FutureBase::CompletionCallback callback,
                         void* user_data, void (*user_data_delete_fn)(void*))
      : completion_callback(callback),
        callback_user_data(user_data),
        callback_user_data_delete_fn(user_data_delete_fn) {}
};

using intrusive_list_iterator =
    intrusive_list<CompletionCallbackData>::iterator;

}  // anonymous namespace

struct FutureBackingData {
  // Create with type-specific data.
  explicit FutureBackingData(void* data, DataDeleteFn* delete_data_fn)
      : status(kFutureStatusPending),
        error(0),
        reference_count(0),
        data(data),
        data_delete_fn(delete_data_fn),
        context_data(nullptr),
        context_data_delete_fn(nullptr),
        completion_single_callback(nullptr),
        completion_multiple_callbacks(&CompletionCallbackData::node),
        proxy(nullptr) {}

  // Call the type-specific destructor on data.
  // Also call the type-specific context data destructor on context_data.
  // Also deallocate the completion_callbacks and proxy.
  ~FutureBackingData();

  // Clear out any existing callback functions,
  // and deallocate the memory associated with them.
  void ClearExistingCallbacks();

  // Remove the specified callback from the list of callbacks,
  // and deallocate the memory associated with it.
  intrusive_list_iterator ClearCallbackData(intrusive_list_iterator it);

  // Add a new callback to the list of callbacks.
  void AddCallbackData(CompletionCallbackData* callback);

  // Deallocate the memory associated with a single callback and nullify its
  // pointer, and decrement the reference count.
  void ClearSingleCallbackData(CompletionCallbackData** field_to_clear);

  // Add a new single callback, clearing any previously-set single callback
  // first, and incrementing the reference count.
  void SetSingleCallbackData(CompletionCallbackData** field_to_set,
                             CompletionCallbackData* callback);

  // Status of the asynchronous call.
  FutureStatus status;

  // Error reported upon call completion.
  int error;

  // Error string reported upon call completion.
  std::string error_msg;

  // Number of outstanding futures referencing this asynchronous call.
  // When this count reaches zero, this class is removed from the `backings_`
  // map and deleted.
  uint32_t reference_count;

  // The call-specific result that is returned in Future<T>,
  // or nullptr if return value is Future<void>.
  void* data;

  // A function that can deletes data by calling its destructor.
  DataDeleteFn* data_delete_fn;

  // Temporary context data used to produce the result returned in Future<T>.
  // E.g., if the result of Future<T> depends on the results of multiple async
  // operations, context_data may be used to store objects that must exist for
  // the lifetime of the Future.
  void* context_data;

  // A function that deletes the context_data.
  DataDeleteFn* context_data_delete_fn;

  // A single function to call when the future completes.
  // Dynamically allocated with 'new'.
  CompletionCallbackData* completion_single_callback;

  // A list of functions to call when the future completes.
  // Note that the elements of this list are themselves dynamically allocated
  // using 'new', and must be deleted when removing them from the list.
  // (We can't use a list of pointers here, because intrusive_list requires
  // that the list element type must contain an instrusive_list_node.)
  intrusive_list<CompletionCallbackData> completion_multiple_callbacks;

  FutureProxyManager* proxy;
};

FutureBackingData::~FutureBackingData() {
  ClearExistingCallbacks();
  if (data != nullptr) {
    FIREBASE_ASSERT(data_delete_fn != nullptr);
    data_delete_fn(data);
    data = nullptr;
  }

  if (context_data != nullptr) {
    FIREBASE_ASSERT(context_data_delete_fn != nullptr);
    context_data_delete_fn(context_data);
    context_data = nullptr;
  }

  if (proxy != nullptr) {
    delete proxy;
    proxy = nullptr;
  }
}

void FutureBackingData::ClearExistingCallbacks() {
  // Clear out any existing callbacks.
  ClearSingleCallbackData(&completion_single_callback);
  auto it = completion_multiple_callbacks.begin();
  while (it != completion_multiple_callbacks.end()) {
    it = ClearCallbackData(it);
  }
}

intrusive_list_iterator FutureBackingData::ClearCallbackData(
    intrusive_list_iterator it) {
  CompletionCallbackData* data = &*it;
  it = completion_multiple_callbacks.erase(it);
  ClearSingleCallbackData(&data);
  return it;
}

void FutureBackingData::AddCallbackData(CompletionCallbackData* callback) {
  if (callback == nullptr) {
    return;
  }
  reference_count++;
  completion_multiple_callbacks.push_back(*callback);
  // Add new callback to reference count. It will be removed via
  // ClearSingleCallbackData later.
}

void FutureBackingData::ClearSingleCallbackData(
    CompletionCallbackData** field_to_clear) {
  CompletionCallbackData* data = *field_to_clear;
  if (data == nullptr) {
    return;
  }
  *field_to_clear = nullptr;
  if (reference_count > 0) {
    reference_count--;
  }
  if (data->callback_user_data_delete_fn != nullptr) {
    data->callback_user_data_delete_fn(data->callback_user_data);
  }
  delete data;
}

void FutureBackingData::SetSingleCallbackData(
    CompletionCallbackData** field_to_set, CompletionCallbackData* callback) {
  CompletionCallbackData* old_callback = *field_to_set;
  *field_to_set = callback;
  if (callback != nullptr) {
    // Add new callback to reference count before clearing any previous callback.
    reference_count++;
  }
  ClearSingleCallbackData(&old_callback);
}

namespace detail {

// Non-inline implementation of FutureApiInterface's virtual destructor
// to prevent its vtable being emitted in each translation unit.
FutureApiInterface::~FutureApiInterface() {}

}  // namespace detail

const char ReferenceCountedFutureImpl::kErrorMessageFutureIsNoLongerValid[] =
    "Invalid Future";

ReferenceCountedFutureImpl::~ReferenceCountedFutureImpl() {
  MutexLock lock(mutex_);
  // All futures should be released before we destroy ourselves.
  for (size_t i = 0; i < last_results_.size(); ++i) {
    last_results_[i].Release();
  }

  // Invalidate any externally-held futures.
  cleanup_.CleanupAll();
  cleanup_handles_.CleanupAll();

  // TODO(jsanmiya): Change this to use unique_ptr so deletion is automatic.
  while (!backings_.empty()) {
    auto it = backings_.begin();
    LogWarning(
        "Future with handle %zu still exists though its backing API"
        " %p is being deleted. Please call Future::Release() before"
        " deleting the backing API.",
        static_cast<size_t>(it->first), static_cast<void*>(this));

    FutureBackingData* backing = it->second;
    backings_.erase(it);
    delete backing;
  }
}

FutureHandle ReferenceCountedFutureImpl::AllocInternal(
    int fn_idx, void* data, void (*delete_data_fn)(void* data_to_delete)) {
  // Backings get deleted in ReleaseFuture() and ~ReferenceCountedFutureImpl().
  FutureBackingData* backing = new FutureBackingData(data, delete_data_fn);

  // Allocate a unique handle and insert the new backing into the map.
  // Note that it's theoretically possible to have a handle collision if we
  // allocate four billion more handles before releasing one. We ignore this
  // possibility.
  MutexLock lock(mutex_);
  const FutureHandleId id = AllocHandleId();
  FIREBASE_FUTURE_TRACE("API: Allocated handle id %zu",
                        static_cast<size_t>(id));
  backings_.insert(BackingPair(id, backing));
  const FutureHandle handle(id, this);

  // Update the most recent Future for this function.
  if (0 <= fn_idx && fn_idx < static_cast<int>(last_results_.size())) {
    FIREBASE_FUTURE_TRACE("API: Future handle %zu (fn %d) --> %p",
                          static_cast<size_t>(handle.id()), fn_idx,
                          static_cast<void*>(&last_results_[fn_idx]));
    last_results_[fn_idx] = FutureBase(this, handle);
  }
  FIREBASE_FUTURE_TRACE("API: Alloc complete.");
  return handle;
}

void* ReferenceCountedFutureImpl::BackingData(FutureBackingData* backing) {
  return backing->data;
}

void ReferenceCountedFutureImpl::SetBackingError(FutureBackingData* backing,
                                                 int error,
                                                 const char* error_msg) {
  // This function is in the cpp instead of the header because
  // FutureBackingData is only declared in the cpp.
  backing->error = error;
  backing->error_msg = error_msg == nullptr ? "" : error_msg;
}

void ReferenceCountedFutureImpl::CompleteHandle(const FutureHandle& handle) {
  FutureBackingData* backing = BackingFromHandle(handle.id());
  // Ensure this Future is valid.
  FIREBASE_ASSERT(backing != nullptr);

  // Ensure we are only setting the status to complete once.
  FIREBASE_ASSERT(backing->status != kFutureStatusComplete);

  // Mark backing as complete.
  backing->status = kFutureStatusComplete;
}

void ReferenceCountedFutureImpl::ReleaseMutexAndRunCallbacks(
    const FutureHandle& handle) {
  // Hold a running-callback count across the entire callback + proxy-completion
  // pass so that `this` cannot be deleted by another thread while callbacks or
  // proxy completions are in flight.
  ++running_callback_count_;

  FutureBackingData* backing = BackingFromHandle(handle.id());
  FIREBASE_ASSERT(backing != nullptr);

  int proxy_error = 0;
  std::string proxy_error_msg;
  std::vector<FutureHandle> proxy_clients;
  if (backing != nullptr && backing->proxy != nullptr) {
    proxy_error = backing->error;
    proxy_error_msg = backing->error_msg;
    proxy_clients = backing->proxy->GetClients();
  }

  // Call the completion callbacks, if any have been registered,
  // removing them from the list as we go.
  if (backing != nullptr &&
      (backing->completion_single_callback != nullptr ||
       !backing->completion_multiple_callbacks.empty())) {
    FutureBase future_base(this, handle);
    if (backing->completion_single_callback != nullptr) {
      CompletionCallbackData* data = backing->completion_single_callback;
      auto callback = data->completion_callback;
      auto user_data = data->callback_user_data;
      backing->completion_single_callback = nullptr;
      RunCallback(&future_base, callback, user_data);
      backing = BackingFromHandle(handle.id());
      if (backing != nullptr) {
        backing->ClearSingleCallbackData(&data);
      } else {
        if (data->callback_user_data_delete_fn != nullptr) {
          data->callback_user_data_delete_fn(data->callback_user_data);
        }
        delete data;
      }
    }
    while ((backing = BackingFromHandle(handle.id())) != nullptr &&
           !backing->completion_multiple_callbacks.empty()) {
      CompletionCallbackData* data =
          &backing->completion_multiple_callbacks.front();
      auto callback = data->completion_callback;
      auto user_data = data->callback_user_data;
      backing->completion_multiple_callbacks.pop_front();
      RunCallback(&future_base, callback, user_data);
      backing = BackingFromHandle(handle.id());
      if (backing != nullptr) {
        backing->ClearSingleCallbackData(&data);
      } else {
        if (data->callback_user_data_delete_fn != nullptr) {
          data->callback_user_data_delete_fn(data->callback_user_data);
        }
        delete data;
      }
    }
  }

  if (!proxy_clients.empty()) {
    mutex_.Release();
    for (const FutureHandle& client_handle : proxy_clients) {
      if (client_handle != kInvalidHandle) {
        Complete(client_handle, proxy_error, proxy_error_msg.c_str());
      }
    }
    mutex_.Acquire();
  }

  --running_callback_count_;
  bool should_delete = false;
  if (is_orphaned_ && running_callback_count_ == 0 && !is_deleting_) {
    is_deleting_ = true;
    should_delete = true;
  }
  mutex_.Release();

  if (should_delete) {
    delete this;
  }
}

void ReferenceCountedFutureImpl::RunCallback(
    FutureBase* future_base, FutureBase::CompletionCallback callback,
    void* user_data) {
  // Make sure we're not deallocated while running the callback, because it
  // would make `future_base` invalid.
  ++running_callback_count_;

  // Release the lock, which is assumed to be obtained by the caller, before
  // calling the callback.
  mutex_.Release();
  if (callback != nullptr) {
    callback(*future_base, user_data);
  }
  mutex_.Acquire();

  --running_callback_count_;
}

void ReferenceCountedFutureImpl::CleanupBackingIfUnreferenced(
    FutureHandleId id) {
  auto it = backings_.find(id);
  if (it != backings_.end() && it->second->reference_count == 0) {
    FutureBackingData* backing = it->second;
    backings_.erase(it);
    delete backing;
  }
}

static void CleanupFuture(FutureBase* future) { future->Release(); }

void ReferenceCountedFutureImpl::RegisterFutureForCleanup(FutureBase* future) {
  cleanup_.RegisterObject(future, CleanupFuture);
}

void ReferenceCountedFutureImpl::UnregisterFutureForCleanup(
    FutureBase* future) {
  cleanup_.UnregisterObject(future);
}

void ReferenceCountedFutureImpl::ReferenceFuture(const FutureHandle& handle) {
  MutexLock lock(mutex_);
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing != nullptr) {
    backing->reference_count++;
    FIREBASE_FUTURE_TRACE("API: Reference handle %zu, ref count %u",
                          static_cast<size_t>(handle.id()),
                          static_cast<unsigned int>(backing->reference_count));
  }
}

void ReferenceCountedFutureImpl::ReleaseFuture(const FutureHandle& handle) {
  MutexLock lock(mutex_);
  FIREBASE_FUTURE_TRACE("API: Release future %zu",
                        static_cast<size_t>(handle.id()));

  // If a Future exists with a handle, then the backing should still exist for
  // it, too. However it might be possible during the deallocate phase when
  // FutureBase and FutureHandle and FutureProxyManager are still having
  // dependencies.
  auto it = backings_.find(handle.id());
  if (it == backings_.end()) {
    return;
  }

  // Decrement the reference count.
  FutureBackingData* backing = it->second;
  FIREBASE_ASSERT(backing->reference_count > 0);
  backing->reference_count--;

  FIREBASE_FUTURE_TRACE("API: Release handle %zu, ref count %u",
                        static_cast<size_t>(handle.id()),
                        static_cast<unsigned int>(backing->reference_count));

  // If asynchronous call is no longer referenced, delete the backing struct.
  if (backing->reference_count == 0) {
    backings_.erase(it);
    delete backing;
    backing = nullptr;
  }
}

FutureStatus ReferenceCountedFutureImpl::GetFutureStatus(
    const FutureHandle& handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle.id());
  return backing == nullptr ? kFutureStatusInvalid : backing->status;
}

int ReferenceCountedFutureImpl::GetFutureError(
    const FutureHandle& handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle.id());
  return backing == nullptr ? kErrorFutureIsNoLongerValid : backing->error;
}

const char* ReferenceCountedFutureImpl::GetFutureErrorMessage(
    const FutureHandle& handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle.id());
  return backing == nullptr ? kErrorMessageFutureIsNoLongerValid
                            : backing->error_msg.c_str();
}

const void* ReferenceCountedFutureImpl::GetFutureResult(
    const FutureHandle& handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle.id());
  return backing == nullptr || backing->status != kFutureStatusComplete
             ? nullptr
             : backing->data;
}

FutureBackingData* ReferenceCountedFutureImpl::BackingFromHandle(
    FutureHandleId id) {
  MutexLock lock(mutex_);
  auto it = backings_.find(id);
  return it == backings_.end() ? nullptr : it->second;
}

detail::CompletionCallbackHandle
ReferenceCountedFutureImpl::AddCompletionCallback(
    const FutureHandle& handle, FutureBase::CompletionCallback callback,
    void* user_data, void (*user_data_delete_fn_ptr)(void*),
    bool single_completion) {
  if (callback == nullptr) {
    if (user_data_delete_fn_ptr != nullptr) {
      user_data_delete_fn_ptr(user_data);
    }
    if (single_completion) {
      MutexLock lock(mutex_);
      FutureBackingData* backing = BackingFromHandle(handle.id());
      if (backing != nullptr) {
        backing->ClearSingleCallbackData(&backing->completion_single_callback);
        CleanupBackingIfUnreferenced(handle.id());
      }
    }
    return detail::CompletionCallbackHandle();
  }

  // Record the callback parameters.
  CompletionCallbackData* callback_data =
      new CompletionCallbackData(callback, user_data, user_data_delete_fn_ptr);

  // To handle the case where the future is already complete and we want to
  // call the callback immediately, we acquire the mutex directly, so that
  // it can be freed in ReleaseMutexAndRunCallbacks, prior to calling the
  // callback.
  mutex_.Acquire();

  // If the handle is no longer valid, don't do anything.
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing == nullptr) {
    mutex_.Release();
    if (callback_data->callback_user_data_delete_fn != nullptr) {
      callback_data->callback_user_data_delete_fn(
          callback_data->callback_user_data);
    }
    delete callback_data;
    return detail::CompletionCallbackHandle();
  }

  if (single_completion) {
    backing->SetSingleCallbackData(&backing->completion_single_callback,
                                   callback_data);
  } else {
    backing->AddCallbackData(callback_data);
  }

  // If the future was already completed, call the callback now.
  if (backing->status == kFutureStatusComplete) {
    // ReleaseMutexAndRunCallbacks is in charge of releasing the mutex.
    ReleaseMutexAndRunCallbacks(handle);
    return detail::CompletionCallbackHandle();
  } else {
    mutex_.Release();
    return detail::CompletionCallbackHandle(callback, user_data,
                                            user_data_delete_fn_ptr);
  }
}

class CompletionMatcher {
 private:
  CompletionCallbackData match_;

 public:
  CompletionMatcher(FutureBase::CompletionCallback callback, void* user_data,
                    void (*user_data_delete_fn)(void*))
      : match_(callback, user_data, user_data_delete_fn) {}
  bool operator()(const CompletionCallbackData& data) const {
    return data.completion_callback == match_.completion_callback &&
           data.callback_user_data == match_.callback_user_data &&
           data.callback_user_data_delete_fn ==
               match_.callback_user_data_delete_fn;
  }
};

void ReferenceCountedFutureImpl::RemoveCompletionCallback(
    const FutureHandle& handle,
    detail::CompletionCallbackHandle callback_handle) {
  MutexLock lock(mutex_);
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing != nullptr) {
    CompletionMatcher matches_callback_handle(
        callback_handle.callback_, callback_handle.user_data_,
        callback_handle.user_data_delete_fn_);
    if (backing->completion_single_callback != nullptr &&
        matches_callback_handle(*backing->completion_single_callback)) {
      backing->ClearSingleCallbackData(&backing->completion_single_callback);
      CleanupBackingIfUnreferenced(handle.id());
    } else {
      auto it = backing->completion_multiple_callbacks.begin();
      while (it != backing->completion_multiple_callbacks.end() &&
             !matches_callback_handle(*it)) {
        ++it;
      }
      if (it != backing->completion_multiple_callbacks.end()) {
        backing->ClearCallbackData(it);
        CleanupBackingIfUnreferenced(handle.id());
      }
    }
  }
}

#ifdef FIREBASE_USE_STD_FUNCTION

static void CallStdFunction(const FutureBase& future, void* function_void) {
  if (function_void) {
    std::function<void(const FutureBase&)>* function =
        reinterpret_cast<std::function<void(const FutureBase&)>*>(
            function_void);
    (*function)(future);
  }
}

static void DeleteStdFunction(void* function_void) {
  if (function_void) {
    std::function<void(const FutureBase&)>* function =
        reinterpret_cast<std::function<void(const FutureBase&)>*>(
            function_void);
    delete function;
  }
}

detail::CompletionCallbackHandle
ReferenceCountedFutureImpl::AddCompletionCallbackLambda(
    const FutureHandle& handle, std::function<void(const FutureBase&)> callback,
    bool single_completion) {
  if (!callback) {
    if (single_completion) {
      MutexLock lock(mutex_);
      FutureBackingData* backing = BackingFromHandle(handle.id());
      if (backing != nullptr) {
        backing->ClearSingleCallbackData(&backing->completion_single_callback);
        CleanupBackingIfUnreferenced(handle.id());
      }
    }
    return detail::CompletionCallbackHandle();
  }

  // Record the callback parameters.
  CompletionCallbackData* completion_callback_data = new CompletionCallbackData(
      /*callback=*/CallStdFunction,
      /*user_data=*/new std::function<void(const FutureBase&)>(callback),
      /*user_data_delete_fn=*/DeleteStdFunction);

  // To handle the case where the future is already complete and we want to
  // call the callback immediately, we acquire the mutex directly, so that
  // it can be freed in ReleaseMutexAndRunCallbacks, prior to calling the
  // callback.
  mutex_.Acquire();

  // If the handle is no longer valid, don't do anything.
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing == nullptr) {
    mutex_.Release();
    if (completion_callback_data->callback_user_data_delete_fn != nullptr) {
      completion_callback_data->callback_user_data_delete_fn(
          completion_callback_data->callback_user_data);
    }
    delete completion_callback_data;
    return detail::CompletionCallbackHandle();
  }

  if (single_completion) {
    backing->SetSingleCallbackData(&backing->completion_single_callback,
                                   completion_callback_data);
  } else {
    backing->AddCallbackData(completion_callback_data);
  }

  // If the future was already completed, call the callback(s) now.
  if (backing->status == kFutureStatusComplete) {
    // ReleaseMutexAndRunCallbacks is in charge of releasing the mutex.
    ReleaseMutexAndRunCallbacks(handle);
    return detail::CompletionCallbackHandle();
  } else {
    mutex_.Release();
    return detail::CompletionCallbackHandle(
        completion_callback_data->completion_callback,
        completion_callback_data->callback_user_data,
        completion_callback_data->callback_user_data_delete_fn);
  }
}

#endif  // FIREBASE_USE_STD_FUNCTION

bool ReferenceCountedFutureImpl::IsSafeToDelete() const {
  MutexLock lock(mutex_);
  // Check if any Futures we have are still pending.
  for (auto i = backings_.begin(); i != backings_.end(); ++i) {
    // If any Future is still pending, not safe to delete.
    if (i->second->status == kFutureStatusPending) return false;
  }

  if (running_callback_count_ > 0) {
    return false;
  }

  return true;
}

bool ReferenceCountedFutureImpl::IsRunningCallback() const {
  MutexLock lock(mutex_);
  return running_callback_count_ > 0;
}

bool ReferenceCountedFutureImpl::IsReferencedExternally() const {
  MutexLock lock(mutex_);

  int total_references = 0;
  int internal_references = 0;
  for (auto i = backings_.begin(); i != backings_.end(); ++i) {
    // Count the total number of references to all valid Futures.
    total_references += i->second->reference_count;
  }
  for (size_t i = 0; i < last_results_.size(); i++) {
    if (last_results_[i].status() != kFutureStatusInvalid) {
      // If the status is not invalid, this entry is using up a reference.
      // Count up the internal references.
      internal_references++;
    }
  }
  // If there are more references than the internal ones, someone is holding
  // onto a Future.
  return total_references > internal_references;
}

void ReferenceCountedFutureImpl::SetContextData(
    const FutureHandle& handle, void* context_data,
    void (*delete_context_data_fn)(void* data_to_delete)) {
  MutexLock lock(mutex_);

  FIREBASE_ASSERT((delete_context_data_fn != nullptr) ||
                  (context_data == nullptr));

  // If the handle is no longer valid, clean up the passed-in context_data.
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing == nullptr) {
    if (context_data != nullptr && delete_context_data_fn != nullptr) {
      delete_context_data_fn(context_data);
    }
    return;
  }

  if (backing->context_data != nullptr &&
      backing->context_data != context_data &&
      backing->context_data_delete_fn != nullptr) {
    backing->context_data_delete_fn(backing->context_data);
  }

  backing->context_data = context_data;
  backing->context_data_delete_fn = delete_context_data_fn;
}

FutureBase ReferenceCountedFutureImpl::LastResultProxy(int fn_idx) {
  MutexLock lock(mutex_);
  if (fn_idx < 0 || static_cast<size_t>(fn_idx) >= last_results_.size()) {
    return FutureBase();
  }
  const FutureBase& future = last_results_[fn_idx];
  // We only do this complicated dance if the Future is pending.
  if (future.status() != kFutureStatusPending) {
    return future;
  }

  // Get the subject backing and (if needed) allocate the ProxyManager.
  const FutureHandle handle = future.GetHandle();
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing == nullptr) {
    return FutureBase();
  }
  if (!backing->proxy) {
    backing->proxy = new FutureProxyManager(this, handle);
  }

  // Allocate the client backing. We reuse the subject data, with a noop
  // delete function, because the subject owns the data.
  FutureHandle client_handle =
      AllocInternal(kNoFunctionIndex, backing->data, [](void*) {});
  // Use the context data to inform the proxy manager when the client dies.
  SetContextData(
      client_handle,
      new FutureProxyManager::UnregisterData(backing->proxy, client_handle),
      FutureProxyManager::UnregisterCallback);
  backing->proxy->RegisterClient(client_handle);

  return FutureBase(this, client_handle);
}

static void CleanupFutureHandle(FutureHandle* handle) { handle->Cleanup(); }

TypedCleanupNotifier<FutureHandle>& CleanupMgr(
    detail::FutureApiInterface* api) {
  return static_cast<ReferenceCountedFutureImpl*>(api)->cleanup_handles();
}

void ReferenceCountedFutureImpl::ForceReleaseFuture(
    const FutureHandle& handle) {
  MutexLock lock(mutex_);
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing != nullptr) {
    backing->reference_count = 1;
    ReleaseFuture(handle);
  }
  FIREBASE_FUTURE_TRACE("API: ForceReleaseFuture handle %zu",
                        static_cast<size_t>(handle.id()));
}

void ReferenceCountedFutureImpl::MarkOrphaned() {
  MutexLock lock(mutex_);
  is_orphaned_ = true;
}

// Implementation of FutureHandle from future.h
FutureHandle::FutureHandle() : id_(0), api_(nullptr) {}

FutureHandle::FutureHandle(FutureHandleId id, detail::FutureApiInterface* api)
    : id_(id), api_(api) {
  if (api != nullptr) {
    api->ReferenceFuture(*this);
    CleanupMgr(api).RegisterObject(this, CleanupFutureHandle);
  }
}

FutureHandle::~FutureHandle() {
  detail::FutureApiInterface* api = api_;
  if (api != nullptr) {
    api_ = nullptr;
    CleanupMgr(api).UnregisterObject(this);
    api->ReleaseFuture(*this);
  }
}

FutureHandle::FutureHandle(const FutureHandle& rhs)
    : id_(rhs.id_), api_(rhs.api_) {
  detail::FutureApiInterface* api = api_;
  if (api != nullptr) {
    api->ReferenceFuture(*this);
    CleanupMgr(api).RegisterObject(this, CleanupFutureHandle);
  }
}

FutureHandle& FutureHandle::operator=(const FutureHandle& rhs) {
  if (this == &rhs) {
    return *this;
  }
  detail::FutureApiInterface* old_api = api_;
  if (old_api != nullptr) {
    api_ = nullptr;
    CleanupMgr(old_api).UnregisterObject(this);
    old_api->ReleaseFuture(*this);
  }

  detail::FutureApiInterface* new_api = rhs.api_;
  this->id_ = rhs.id_;
  this->api_ = new_api;
  if (new_api != nullptr) {
    new_api->ReferenceFuture(*this);
    CleanupMgr(new_api).RegisterObject(this, CleanupFutureHandle);
  }

  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
FutureHandle::FutureHandle(FutureHandle&& rhs) noexcept
    : id_(rhs.id_), api_(rhs.api_) {
  detail::FutureApiInterface* rhs_api = rhs.api_;
  rhs.id_ = 0;
  rhs.api_ = nullptr;
  if (rhs_api != nullptr) {
    CleanupMgr(rhs_api).RegisterObject(this, CleanupFutureHandle);
    CleanupMgr(rhs_api).UnregisterObject(&rhs);
  }
}

FutureHandle& FutureHandle::operator=(FutureHandle&& rhs) noexcept {
  if (this == &rhs) {
    return *this;
  }
  detail::FutureApiInterface* old_api = api_;
  if (old_api != nullptr) {
    api_ = nullptr;
    CleanupMgr(old_api).UnregisterObject(this);
    old_api->ReleaseFuture(*this);
  }

  detail::FutureApiInterface* rhs_api = rhs.api_;
  this->id_ = rhs.id_;
  this->api_ = rhs_api;
  rhs.id_ = 0;
  rhs.api_ = nullptr;
  if (rhs_api != nullptr) {
    CleanupMgr(rhs_api).RegisterObject(this, CleanupFutureHandle);
    CleanupMgr(rhs_api).UnregisterObject(&rhs);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

void FutureHandle::Detach() {
  detail::FutureApiInterface* api = api_;
  if (api != nullptr) {
    api_ = nullptr;
    CleanupMgr(api).UnregisterObject(this);
    api->ReleaseFuture(*this);
  }
}

}  // namespace firebase
