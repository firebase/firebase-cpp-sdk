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
#include <cstdint>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/intrusive_list.h"
#include "app/src/log.h"
#include "app/src/mutex.h"

// Set this to 1 to enable verbose logging in this module.
#if !defined(FIREBASE_FUTURE_TRACE_ENABLE)
#define FIREBASE_FUTURE_TRACE_ENABLE 0
#endif  // !defined(FIREBASE_FUTURE_TRACE_ENABLE)

#if FIREBASE_FUTURE_TRACE_ENABLE
#define FIREBASE_FUTURE_TRACE(...) LogDebug(__VA_ARGS__)
#else
#define FIREBASE_FUTURE_TRACE(...)
#endif  // FIREBASE_FUTURE_TRACE_ENABLE

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

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
      : api_(api), subject_(subject) {}

  ~FutureProxyManager() {
    MutexLock lock(mutex_);
    for (FutureHandle& h : clients_) {
      api_->ForceReleaseFuture(h);
      h = ReferenceCountedFutureImpl::kInvalidHandle;
    }
    clients_.clear();
  }

  void RegisterClient(const FutureHandle& handle) {
    MutexLock lock(mutex_);
    // We create one reference per client to the Future.
    // This way the ReferenceCountedFutureImpl will do the right thing if one
    // thread tries to unregister the last client while adding a new one.
    api_->ReferenceFuture(subject_);
    clients_.push_back(handle);
  }

  struct UnregisterData {
    UnregisterData(FutureProxyManager* proxy, const FutureHandle& handle)
        : proxy(proxy), handle(handle) {}
    FutureProxyManager* proxy;
    FutureHandle handle;
  };

  static void UnregisterCallback(void* data) {
    if (data == nullptr) {
      return;
    }
    UnregisterData* udata = static_cast<UnregisterData*>(data);
    if (udata != nullptr) {
      udata->proxy->UnregisterClient(udata->handle);
      delete udata;
    }
  }

  void UnregisterClient(const FutureHandle& handle) {
    MutexLock lock(mutex_);
    for (FutureHandle& h : clients_) {
      if (h == handle) {
        h = ReferenceCountedFutureImpl::kInvalidHandle;
        // Release one reference. This can delete subject_, which in turn will
        // delete `this`, as the subject owns the proxy. This is expected and
        // fine; as long as we don't do anything after the ReleaseFuture call.
        api_->ReleaseFuture(subject_);
        break;
      }
    }
  }

  void CompleteClients(int error, const char* error_msg) {
    MutexLock lock(mutex_);
    for (const FutureHandle& h : clients_) {
      if (h != ReferenceCountedFutureImpl::kInvalidHandle) {
        api_->Complete(h, error, error_msg);
      }
    }
  }

 private:
  std::vector<FutureHandle> clients_;
  ReferenceCountedFutureImpl* api_;
  // We need to keep the subject alive, as it owns us and the data.
  FutureHandle subject_;
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
  if (*field_to_clear == nullptr) {
    return;
  }
  if ((*field_to_clear)->callback_user_data_delete_fn != nullptr) {
    (*field_to_clear)
        ->callback_user_data_delete_fn((*field_to_clear)->callback_user_data);
  }
  delete *field_to_clear;
  *field_to_clear = nullptr;
  reference_count--;
}

void FutureBackingData::SetSingleCallbackData(
    CompletionCallbackData** field_to_set, CompletionCallbackData* callback) {
  ClearSingleCallbackData(field_to_set);  // Remove any existing callback.
  if (callback != nullptr) {
    // Add new callback to reference count.
    reference_count++;
  }
  (*field_to_set) = callback;
}

namespace detail {

// Non-inline implementation of FutureApiInterface's virtual destructor
// to prevent its vtable being emitted in each translation unit.
FutureApiInterface::~FutureApiInterface() {}

}  // namespace detail

const char ReferenceCountedFutureImpl::kErrorMessageFutureIsNoLongerValid[] =
    "Invalid Future";

ReferenceCountedFutureImpl::~ReferenceCountedFutureImpl() {
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
        "Future with handle %d still exists though its backing API"
        " 0x%X is being deleted. Please call Future::Release() before"
        " deleting the backing API.",
        it->first, static_cast<int>(reinterpret_cast<uintptr_t>(this)));

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
  FIREBASE_FUTURE_TRACE("API: Allocated handle id %d", id);
  backings_.insert(BackingPair(id, backing));
  const FutureHandle handle(id, this);

  // Update the most recent Future for this function.
  if (0 <= fn_idx && fn_idx < static_cast<int>(last_results_.size())) {
    FIREBASE_FUTURE_TRACE("API: Future handle %d (fn %d) --> %08x", handle.id(),
                          fn_idx, &last_results_[fn_idx]);
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

void ReferenceCountedFutureImpl::CompleteProxy(FutureBackingData* backing) {
  // This function is in the cpp instead of the header because
  // FutureBackingData is only declared in the cpp.
  if (backing->proxy) {
    backing->proxy->CompleteClients(backing->error, backing->error_msg.c_str());
  }
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
  FutureBackingData* backing = BackingFromHandle(handle.id());
  FIREBASE_ASSERT(backing != nullptr);

  // Call the completion callbacks, if any have been registered,
  // removing them from the list as we go.
  if (backing->completion_single_callback != nullptr ||
      !backing->completion_multiple_callbacks.empty()) {
    FutureBase future_base(this, handle);
    if (backing->completion_single_callback != nullptr) {
      CompletionCallbackData* data = backing->completion_single_callback;
      auto callback = data->completion_callback;
      auto user_data = data->callback_user_data;
      backing->completion_single_callback = nullptr;
      RunCallback(&future_base, callback, user_data);
      // ClearSingleCallbackData calls delete_fn, deletes data, and decrements
      // refcount.
      backing->ClearSingleCallbackData(&data);
    }
    while (!backing->completion_multiple_callbacks.empty()) {
      CompletionCallbackData* data =
          &backing->completion_multiple_callbacks.front();
      auto callback = data->completion_callback;
      auto user_data = data->callback_user_data;
      backing->completion_multiple_callbacks.pop_front();
      RunCallback(&future_base, callback, user_data);
      // ClearSingleCallbackData calls delete_fn, deletes data, and decrements
      // refcount.
      backing->ClearSingleCallbackData(&data);
    }
  }
  mutex_.Release();
}

void ReferenceCountedFutureImpl::RunCallback(
    FutureBase* future_base, FutureBase::CompletionCallback callback,
    void* user_data) {
  // Make sure we're not deallocated while running the callback, because it
  // would make `future_base` invalid.
  is_running_callback_ = true;

  // Release the lock, which is assumed to be obtained by the caller, before
  // calling the callback.
  mutex_.Release();
  callback(*future_base, user_data);
  mutex_.Acquire();

  is_running_callback_ = false;
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
  BackingFromHandle(handle.id())->reference_count++;
  FIREBASE_FUTURE_TRACE("API: Reference handle %d, ref count %d", handle.id(),
                        BackingFromHandle(handle.id())->reference_count);
}

void ReferenceCountedFutureImpl::ReleaseFuture(const FutureHandle& handle) {
  MutexLock lock(mutex_);
  FIREBASE_FUTURE_TRACE("API: Release future %d", (int)handle.id());

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

  FIREBASE_FUTURE_TRACE("API: Release handle %d, ref count %d", handle.id(),
                        BackingFromHandle(handle.id())->reference_count);

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
    }
    auto it = backing->completion_multiple_callbacks.begin();
    while (it != backing->completion_multiple_callbacks.end() &&
           !matches_callback_handle(*it)) {
      ++it;
    }
    if (it != backing->completion_multiple_callbacks.end()) {
      backing->ClearCallbackData(it);
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

  if (is_running_callback_) {
    return false;
  }

  return true;
}

bool ReferenceCountedFutureImpl::IsReferencedExternally() const {
  MutexLock lock(mutex_);

  int total_references = 0;
  int internal_references = 0;
  for (auto i = backings_.begin(); i != backings_.end(); ++i) {
    // Count the total number of references to all valid Futures.
    total_references += i->second->reference_count;
  }
  for (int i = 0; i < last_results_.size(); i++) {
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

  // If the handle is no longer valid, don't do anything.
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing == nullptr) return;

  FIREBASE_ASSERT((delete_context_data_fn != nullptr) ||
                  (context_data == nullptr));

  backing->context_data = context_data;
  backing->context_data_delete_fn = delete_context_data_fn;
}

// We need to have this define because FutureBase::GetHandle() is only
// available when build INTERNAL_EXPERIMENTAL.
#if defined(INTERNAL_EXPERIMENTAL)
FutureBase ReferenceCountedFutureImpl::LastResultProxy(int fn_idx) {
  MutexLock lock(mutex_);
  const FutureBase& future = last_results_[fn_idx];
  // We only do this complicated dance if the Future is pending.
  if (future.status() != kFutureStatusPending) {
    return future;
  }

  // Get the subject backing and (if needed) allocate the ProxyManager.
  const FutureHandle handle = future.GetHandle();
  FutureBackingData* backing = BackingFromHandle(handle.id());
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
#endif  // defined(INTERNAL_EXPERIMENTAL)

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
  FIREBASE_FUTURE_TRACE("API: ForceReleaseFuture handle %d", handle.id());
}

// Implementation of FutureHandle from future.h
FutureHandle::FutureHandle() : id_(0), api_(nullptr) {}

FutureHandle::FutureHandle(FutureHandleId id, detail::FutureApiInterface* api)
    : id_(id), api_(api) {
  if (api_ != nullptr) {
    api_->ReferenceFuture(*this);
    CleanupMgr(api_).RegisterObject(this, CleanupFutureHandle);
  }
}

FutureHandle::~FutureHandle() {
  if (api_ != nullptr) {
    CleanupMgr(api_).UnregisterObject(this);
    detail::FutureApiInterface* api = api_;
    api_ = nullptr;
    api->ReleaseFuture(*this);
  }
}

FutureHandle::FutureHandle(const FutureHandle& rhs) {
  this->id_ = rhs.id_;
  this->api_ = rhs.api_;
  if (api_ != nullptr) {
    api_->ReferenceFuture(*this);
    CleanupMgr(api_).RegisterObject(this, CleanupFutureHandle);
  }
}

FutureHandle& FutureHandle::operator=(const FutureHandle& rhs) {
  if (api_ != nullptr) {
    CleanupMgr(api_).UnregisterObject(this);
    api_->ReleaseFuture(*this);
    api_ = nullptr;
  }

  this->id_ = rhs.id_;
  this->api_ = rhs.api_;
  if (api_ != nullptr) {
    api_->ReferenceFuture(*this);
    CleanupMgr(api_).RegisterObject(this, CleanupFutureHandle);
  }

  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
FutureHandle::FutureHandle(FutureHandle&& rhs) noexcept {
  this->id_ = rhs.id_;
  this->api_ = rhs.api_;
  rhs.id_ = 0;
  if (rhs.api_ != nullptr) {
    CleanupMgr(api_).RegisterObject(this, CleanupFutureHandle);
    CleanupMgr(rhs.api_).UnregisterObject(&rhs);
  }
  rhs.api_ = nullptr;
}

FutureHandle& FutureHandle::operator=(FutureHandle&& rhs) noexcept {
  if (api_ != nullptr) {
    CleanupMgr(api_).UnregisterObject(this);
    api_->ReleaseFuture(*this);
    api_ = nullptr;
  }

  this->id_ = rhs.id_;
  this->api_ = rhs.api_;
  rhs.id_ = 0;
  if (rhs.api_ != nullptr) {
    CleanupMgr(api_).RegisterObject(this, CleanupFutureHandle);
    CleanupMgr(rhs.api_).UnregisterObject(&rhs);
  }
  rhs.api_ = nullptr;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

void FutureHandle::Detach() {
  if (api_ != nullptr) {
    CleanupMgr(api_).UnregisterObject(this);
    api_->ReleaseFuture(*this);
    api_ = nullptr;
  }
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
