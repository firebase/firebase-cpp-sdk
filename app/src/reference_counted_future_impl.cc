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

#include <cstdint>
#include <string>

#include "app/src/assert.h"
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

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// See warning at the top of FutureBase's declaration for details.
static_assert(sizeof(FutureBase) == sizeof(Future<int>),
              "Future should not introduce virtual functions or data members.");

typedef void DataDeleteFn(void* data_to_delete);
typedef std::pair<FutureHandle, FutureBackingData*> BackingPair;

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
  FutureProxyManager(ReferenceCountedFutureImpl* api, FutureHandle subject) :
    api_(api), subject_(subject) {}

  void RegisterClient(FutureHandle handle) {
    // We create one reference per client to the Future.
    // This way the ReferenceCountedFutureImpl will do the right thing if one
    // thread tries to unregister the last client while adding a new one.
    api_->ReferenceFuture(subject_);
    clients_.push_back(handle);
  }

  struct UnregisterData {
    UnregisterData(FutureProxyManager* proxy, FutureHandle handle) :
      proxy(proxy), handle(handle) {}
    FutureProxyManager* proxy;
    FutureHandle handle;
  };

  static void UnregisterCallback(void* data) {
    UnregisterData* udata = static_cast<UnregisterData*>(data);
    udata->proxy->UnregisterClient(udata->handle);
    delete udata;
  }

  void UnregisterClient(FutureHandle handle) {
    for (FutureHandle& h : clients_) {
      if (h == handle) {
        h = kInvalidFutureHandle;
        // Release one reference. This can delete subject_, which in turn will
        // delete `this`, as the subject owns the proxy. This is expected and
        // fine; as long as we don't do anything after the ReleaseFuture call.
        api_->ReleaseFuture(subject_);
        break;
      }
    }
  }

  void CompleteClients(int error, const char* error_msg) {
    for (const FutureHandle& h : clients_) {
      if (h != kInvalidFutureHandle) {
        api_->Complete(h, error, error_msg);
      }
    }
  }

 private:
  std::vector<FutureHandle> clients_;
  ReferenceCountedFutureImpl* api_;
  // We need to keep the subject alive, as it owns us and the data.
  FutureHandle subject_;
};

}  // namespace

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
        completion_callback(nullptr),
        callback_user_data(nullptr),
        callback_user_data_delete_fn(nullptr),
        proxy(nullptr) {}

  // Call the type-specific destructor on data.
  ~FutureBackingData() {
    if (callback_user_data_delete_fn) {
      callback_user_data_delete_fn(callback_user_data);
      callback_user_data_delete_fn = nullptr;
    }
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

    delete proxy;
  }

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

  // The function to call once the future is marked completed.
  FutureBase::CompletionCallback completion_callback;

  // The data to pass into `completion_callback`.
  void* callback_user_data;

  // If set, this function will be called to delete callback_user_data after the
  // callback runs or the Future is destroyed.
  void (*callback_user_data_delete_fn)(void*);

  FutureProxyManager* proxy;
};

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
  const FutureHandle handle = AllocHandle();
  FIREBASE_FUTURE_TRACE("API: Allocated handle %d", handle);
  backings_.insert(BackingPair(handle, backing));

  // Update the most recent Future for this function.
  if (0 <= fn_idx && fn_idx < static_cast<int>(last_results_.size())) {
    FIREBASE_FUTURE_TRACE("API: Future handle %d (fn %d) --> %08x", handle,
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
    backing->proxy->CompleteClients(backing->error,
                                    backing->error_msg.c_str());
  }
}

void ReferenceCountedFutureImpl::CompleteHandle(FutureHandle handle) {
  FutureBackingData* backing = BackingFromHandle(handle);
  // Ensure this Future is valid.
  FIREBASE_ASSERT(backing != nullptr);

  // Ensure we are only setting the status to complete once.
  FIREBASE_ASSERT(backing->status != kFutureStatusComplete);

  // Mark backing as complete.
  backing->status = kFutureStatusComplete;
}

void ReferenceCountedFutureImpl::ReleaseMutexAndRunCallback(
    FutureHandle handle) {
  FutureBackingData* backing = BackingFromHandle(handle);
  FIREBASE_ASSERT(backing != nullptr);

  // Call callback, if one has been registered.
  if (backing->completion_callback != nullptr) {
    FutureBase future_base(this, handle);
    auto callback = backing->completion_callback;
    auto user_data = backing->callback_user_data;
    auto delete_fn = backing->callback_user_data_delete_fn;

    // Consume the callback and user_data, so the callback is only called once.
    backing->completion_callback = nullptr;
    backing->callback_user_data_delete_fn = nullptr;
    backing->callback_user_data = nullptr;
    // Release the lock, which is assumed to be obtained by the caller, before
    // calling the callback.
    mutex_.Release();

    callback(future_base, user_data);

    if (delete_fn) {
      // Lock again, as we can't assume that the callback is thread-safe from
      // the user's perspective.
      MutexLock lock(mutex_);
      delete_fn(user_data);
    }
  } else {
    mutex_.Release();
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

void ReferenceCountedFutureImpl::ReferenceFuture(FutureHandle handle) {
  MutexLock lock(mutex_);
  BackingFromHandle(handle)->reference_count++;
  FIREBASE_FUTURE_TRACE("API: Reference handle %d, ref count %d", handle,
                        BackingFromHandle(handle)->reference_count);
}

void ReferenceCountedFutureImpl::ReleaseFuture(FutureHandle handle) {
  MutexLock lock(mutex_);
  FIREBASE_FUTURE_TRACE("API: Release future %d", (int)handle);

  // Assert if the handle isn't registered.
  // If a Future exists with a handle, then the backing should still exist for
  // it, too.
  auto it = backings_.find(handle);
  FIREBASE_ASSERT(it != backings_.end());

  // Decrement the reference count.
  FutureBackingData* backing = it->second;
  FIREBASE_ASSERT(backing->reference_count > 0);
  backing->reference_count--;

  FIREBASE_FUTURE_TRACE("API: Release handle %d, ref count %d", handle,
                        BackingFromHandle(handle)->reference_count);

  // If asynchronous call is no longer referenced, delete the backing struct.
  if (backing->reference_count == 0) {
    backings_.erase(it);
    delete backing;
    backing = nullptr;
  }
}

FutureStatus ReferenceCountedFutureImpl::GetFutureStatus(
    FutureHandle handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle);
  return backing == nullptr ? kFutureStatusInvalid : backing->status;
}

int ReferenceCountedFutureImpl::GetFutureError(FutureHandle handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle);
  return backing == nullptr ? kErrorFutureIsNoLongerValid : backing->error;
}

const char* ReferenceCountedFutureImpl::GetFutureErrorMessage(
    FutureHandle handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle);
  return backing == nullptr ? kErrorMessageFutureIsNoLongerValid
                            : backing->error_msg.c_str();
}

const void* ReferenceCountedFutureImpl::GetFutureResult(
    FutureHandle handle) const {
  MutexLock lock(mutex_);
  const FutureBackingData* backing = BackingFromHandle(handle);
  return backing == nullptr || backing->status != kFutureStatusComplete
             ? nullptr
             : backing->data;
}

FutureBackingData* ReferenceCountedFutureImpl::BackingFromHandle(
    FutureHandle handle) {
  MutexLock lock(mutex_);
  auto it = backings_.find(handle);
  return it == backings_.end() ? nullptr : it->second;
}

void ReferenceCountedFutureImpl::SetCompletionCallback(
    FutureHandle handle, FutureBase::CompletionCallback callback,
    void* user_data) {
  // To handle the case where the future is already complete and we want to
  // call the callback immediately, we acquire the mutex directly, so that
  // it can be freed in ReleaseMutexAndRunCallback, prior to calling the
  // callback.
  mutex_.Acquire();

  // If the handle is no longer valid, don't do anything.
  FutureBackingData* backing = BackingFromHandle(handle);
  if (backing == nullptr) {
    mutex_.Release();
    return;
  }

  // Record the callback parameters.
  backing->completion_callback = callback;
  if (backing->callback_user_data_delete_fn) {
    backing->callback_user_data_delete_fn(backing->callback_user_data);
  }
  backing->callback_user_data = user_data;
  backing->callback_user_data_delete_fn = nullptr;

  // If the future was already completed, call the callback now.
  if (backing->status == kFutureStatusComplete) {
    // ReleaseMutexAndRunCallback is in charge of releasing the mutex.
    ReleaseMutexAndRunCallback(handle);
  } else {
    mutex_.Release();
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

void ReferenceCountedFutureImpl::SetCompletionCallbackLambda(
    FutureHandle handle, std::function<void(const FutureBase&)> callback) {
  // To handle the case where the future is already complete and we want to
  // call the callback immediately, we acquire the mutex directly, so that
  // it can be freed in ReleaseMutexAndRunCallback, prior to calling the
  // callback.
  mutex_.Acquire();

  // If the handle is no longer valid, don't do anything.
  FutureBackingData* backing = BackingFromHandle(handle);
  if (backing == nullptr) {
    mutex_.Release();
    return;
  }

  // Record the callback parameters.
  backing->completion_callback = CallStdFunction;
  if (backing->callback_user_data_delete_fn) {
    backing->callback_user_data_delete_fn(backing->callback_user_data);
  }
  backing->callback_user_data =
      new std::function<void(const FutureBase&)>(callback);
  backing->callback_user_data_delete_fn = DeleteStdFunction;
  // If the future was already completed, call the callback now.
  if (backing->status == kFutureStatusComplete) {
    // ReleaseMutexAndRunCallback is in charge of releasing the mutex.
    ReleaseMutexAndRunCallback(handle);
  } else {
    mutex_.Release();
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
  return true;
}

void ReferenceCountedFutureImpl::SetContextData(
    FutureHandle handle, void* context_data,
    void (*delete_context_data_fn)(void* data_to_delete)) {
  MutexLock lock(mutex_);

  // If the handle is no longer valid, don't do anything.
  FutureBackingData* backing = BackingFromHandle(handle);
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
  FutureHandle handle = future.GetHandle();
  FutureBackingData* backing = BackingFromHandle(handle);
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
#endif   // defined(INTERNAL_EXPERIMENTAL)

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
