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
#include <list>
#include <optional>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"

namespace firebase {

static_assert(sizeof(FutureBase) == sizeof(Future<int>),
              "Future should not introduce virtual functions or data members.");

typedef void DataDeleteFn(void* data_to_delete);
typedef std::pair<FutureHandleId, FutureBackingData*> BackingPair;

const FutureHandle ReferenceCountedFutureImpl::kInvalidHandle(
    kInvalidFutureHandle);

namespace {

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
    if (data == nullptr) return;
    auto* udata = static_cast<UnregisterData*>(data);
    udata->proxy->UnregisterClient(udata->handle);
    delete udata;
  }

  void UnregisterClient(const FutureHandle& handle) {
    ReferenceCountedFutureImpl* api = nullptr;
    FutureHandle subject;
    {
      MutexLock lock(mutex_);
      if (destroying_) return;
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
  FutureHandle subject_;
  bool destroying_ = false;
  mutable Mutex mutex_;
};

struct CompletionCallbackData {
  FutureBase::CompletionCallback completion_callback;
  void* callback_user_data;
  void (*callback_user_data_delete_fn)(void*);

  CompletionCallbackData(FutureBase::CompletionCallback callback,
                         void* user_data, void (*user_data_delete_fn)(void*))
      : completion_callback(callback),
        callback_user_data(user_data),
        callback_user_data_delete_fn(user_data_delete_fn) {}
};

using CallbackListIterator = std::list<CompletionCallbackData*>::iterator;

}  // anonymous namespace

struct FutureBackingData {
  explicit FutureBackingData(void* data, DataDeleteFn* delete_data_fn)
      : status(kFutureStatusPending),
        error(0),
        reference_count(0),
        data(data),
        data_delete_fn(delete_data_fn),
        context_data(nullptr),
        context_data_delete_fn(nullptr),
        completion_single_callback(nullptr),
        proxy(nullptr) {}

  ~FutureBackingData() {
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
    delete proxy;
    proxy = nullptr;
  }

  void ClearExistingCallbacks() {
    ClearSingleCallbackData(&completion_single_callback);
    auto it = completion_multiple_callbacks.begin();
    while (it != completion_multiple_callbacks.end()) {
      it = ClearCallbackData(it);
    }
  }

  CallbackListIterator ClearCallbackData(CallbackListIterator it) {
    CompletionCallbackData* data = *it;
    it = completion_multiple_callbacks.erase(it);
    ClearSingleCallbackData(&data);
    return it;
  }

  void AddCallbackData(CompletionCallbackData* callback) {
    if (callback == nullptr) return;
    reference_count++;
    completion_multiple_callbacks.push_back(callback);
  }

  void ClearSingleCallbackData(CompletionCallbackData** field_to_clear) {
    CompletionCallbackData* data = *field_to_clear;
    if (data == nullptr) return;
    *field_to_clear = nullptr;
    if (reference_count > 0) reference_count--;
    if (data->callback_user_data_delete_fn != nullptr) {
      data->callback_user_data_delete_fn(data->callback_user_data);
    }
    delete data;
  }

  void SetSingleCallbackData(CompletionCallbackData** field_to_set,
                             CompletionCallbackData* callback) {
    CompletionCallbackData* old_callback = *field_to_set;
    *field_to_set = callback;
    if (callback != nullptr) reference_count++;
    ClearSingleCallbackData(&old_callback);
  }

  FutureStatus status;
  int error;
  std::string error_msg;
  uint32_t reference_count;
  void* data;
  DataDeleteFn* data_delete_fn;
  void* context_data;
  DataDeleteFn* context_data_delete_fn;
  CompletionCallbackData* completion_single_callback;
  std::list<CompletionCallbackData*> completion_multiple_callbacks;
  FutureProxyManager* proxy;
};

namespace detail {
FutureApiInterface::~FutureApiInterface() {}
}  // namespace detail

const char ReferenceCountedFutureImpl::kErrorMessageFutureIsNoLongerValid[] =
    "Invalid Future";

ReferenceCountedFutureImpl::~ReferenceCountedFutureImpl() {
  {
    MutexLock lock(last_results_mutex_);
    for (size_t i = 0; i < last_results_.size(); ++i) {
      last_results_[i].Release();
    }
  }
  cleanup_.CleanupAll();
  cleanup_handles_.CleanupAll();

  MutexLock lock(mutex_);
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
  auto* backing = new FutureBackingData(data, delete_data_fn);
  auto insert_backing = [&]() {
    MutexLock lock(mutex_);
    const FutureHandleId id = AllocHandleId();
    backings_.insert(BackingPair(id, backing));
    return FutureHandle(id, this);
  };

  if (fn_idx < 0 || fn_idx >= static_cast<int>(last_results_.size())) {
    return insert_backing();
  }

  MutexLock last_results_lock(last_results_mutex_);
  FutureHandle handle = insert_backing();
  last_results_[fn_idx] = FutureBase(this, handle);
  return handle;
}

void* ReferenceCountedFutureImpl::BackingData(FutureBackingData* backing) {
  return backing->data;
}

void ReferenceCountedFutureImpl::SetBackingError(FutureBackingData* backing,
                                                 int error,
                                                 const char* error_msg) {
  backing->error = error;
  backing->error_msg = error_msg == nullptr ? "" : error_msg;
}

void ReferenceCountedFutureImpl::CompleteHandle(const FutureHandle& handle) {
  FutureBackingData* backing = BackingFromHandle(handle.id());
  FIREBASE_ASSERT(backing != nullptr);
  FIREBASE_ASSERT(backing->status != kFutureStatusComplete);
  backing->status = kFutureStatusComplete;
}

void ReferenceCountedFutureImpl::ReleaseMutexAndRunCallbacks(
    const FutureHandle& handle) {
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

  std::optional<FutureBase> future_base_storage;
  auto invoke_and_clear = [&](CompletionCallbackData* data,
                              FutureBase& future_base) {
    auto callback = data->completion_callback;
    auto user_data = data->callback_user_data;
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
  };

  if (backing != nullptr &&
      (backing->completion_single_callback != nullptr ||
       !backing->completion_multiple_callbacks.empty())) {
    FutureBase& future_base = future_base_storage.emplace(this, handle);
    if (backing->completion_single_callback != nullptr) {
      CompletionCallbackData* data = backing->completion_single_callback;
      backing->completion_single_callback = nullptr;
      invoke_and_clear(data, future_base);
    }
    while ((backing = BackingFromHandle(handle.id())) != nullptr &&
           !backing->completion_multiple_callbacks.empty()) {
      CompletionCallbackData* data =
          backing->completion_multiple_callbacks.front();
      backing->completion_multiple_callbacks.pop_front();
      invoke_and_clear(data, future_base);
    }
  }
  if (future_base_storage) {
    mutex_.Release();
    future_base_storage.reset();
    mutex_.Acquire();
    CleanupBackingIfUnreferenced(handle.id());
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
  ++running_callback_count_;
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
  cleanup_.UnregisterObject(future, &future->mutex_);
}

void ReferenceCountedFutureImpl::ReferenceFuture(const FutureHandle& handle) {
  MutexLock lock(mutex_);
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing != nullptr) {
    backing->reference_count++;
  }
}

void ReferenceCountedFutureImpl::ReleaseFuture(const FutureHandle& handle) {
  MutexLock lock(mutex_);
  auto it = backings_.find(handle.id());
  if (it == backings_.end()) return;
  FutureBackingData* backing = it->second;
  FIREBASE_ASSERT(backing->reference_count > 0);
  if (--backing->reference_count == 0) {
    backings_.erase(it);
    delete backing;
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

  auto* callback_data =
      new CompletionCallbackData(callback, user_data, user_data_delete_fn_ptr);
  mutex_.Acquire();
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

  if (backing->status == kFutureStatusComplete) {
    ReleaseMutexAndRunCallbacks(handle);
    return detail::CompletionCallbackHandle();
  }
  mutex_.Release();
  return detail::CompletionCallbackHandle(callback, user_data,
                                          user_data_delete_fn_ptr);
}

void ReferenceCountedFutureImpl::RemoveCompletionCallback(
    const FutureHandle& handle,
    detail::CompletionCallbackHandle callback_handle) {
  MutexLock lock(mutex_);
  FutureBackingData* backing = BackingFromHandle(handle.id());
  if (backing == nullptr) return;
  auto matches = [&](const CompletionCallbackData* data) {
    return data->completion_callback == callback_handle.callback_ &&
           data->callback_user_data == callback_handle.user_data_ &&
           data->callback_user_data_delete_fn ==
               callback_handle.user_data_delete_fn_;
  };
  if (backing->completion_single_callback != nullptr &&
      matches(backing->completion_single_callback)) {
    backing->ClearSingleCallbackData(&backing->completion_single_callback);
    CleanupBackingIfUnreferenced(handle.id());
  } else {
    auto it = std::find_if(backing->completion_multiple_callbacks.begin(),
                           backing->completion_multiple_callbacks.end(),
                           matches);
    if (it != backing->completion_multiple_callbacks.end()) {
      backing->ClearCallbackData(it);
      CleanupBackingIfUnreferenced(handle.id());
    }
  }
}

#ifdef FIREBASE_USE_STD_FUNCTION

static void CallStdFunction(const FutureBase& future, void* function_void) {
  if (function_void) {
    (*reinterpret_cast<std::function<void(const FutureBase&)>*>(function_void))(
        future);
  }
}

static void DeleteStdFunction(void* function_void) {
  delete reinterpret_cast<std::function<void(const FutureBase&)>*>(
      function_void);
}

detail::CompletionCallbackHandle
ReferenceCountedFutureImpl::AddCompletionCallbackLambda(
    const FutureHandle& handle, std::function<void(const FutureBase&)> callback,
    bool single_completion) {
  if (!callback) {
    return AddCompletionCallback(handle, nullptr, nullptr, nullptr,
                                 single_completion);
  }
  return AddCompletionCallback(
      handle, CallStdFunction,
      new std::function<void(const FutureBase&)>(std::move(callback)),
      DeleteStdFunction, single_completion);
}

#endif  // FIREBASE_USE_STD_FUNCTION

bool ReferenceCountedFutureImpl::IsSafeToDelete() const {
  MutexLock lock(mutex_);
  for (const auto& entry : backings_) {
    if (entry.second->status == kFutureStatusPending) return false;
  }
  return running_callback_count_ == 0;
}

bool ReferenceCountedFutureImpl::IsRunningCallback() const {
  MutexLock lock(mutex_);
  return running_callback_count_ > 0;
}

bool ReferenceCountedFutureImpl::IsReferencedExternally() const {
  MutexLock last_results_lock(last_results_mutex_);
  int internal_references = 0;
  for (const auto& last_result : last_results_) {
    if (last_result.status() != kFutureStatusInvalid) internal_references++;
  }
  MutexLock lock(mutex_);
  int total_references = 0;
  for (const auto& entry : backings_) {
    total_references += entry.second->reference_count;
  }
  return total_references > internal_references;
}

void ReferenceCountedFutureImpl::SetContextData(
    const FutureHandle& handle, void* context_data,
    void (*delete_context_data_fn)(void* data_to_delete)) {
  MutexLock lock(mutex_);
  FIREBASE_ASSERT((delete_context_data_fn != nullptr) ||
                  (context_data == nullptr));
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
  if (fn_idx < 0 || static_cast<size_t>(fn_idx) >= last_results_.size()) {
    return FutureBase();
  }
  FutureBase future;
  {
    MutexLock last_results_lock(last_results_mutex_);
    future = last_results_[fn_idx];
  }
  if (future.status() != kFutureStatusPending) {
    return future;
  }

  FutureHandle client_handle;
  {
    MutexLock lock(mutex_);
    const FutureHandle handle = future.GetHandle();
    FutureBackingData* backing = BackingFromHandle(handle.id());
    if (backing == nullptr) return FutureBase();
    if (!backing->proxy) {
      backing->proxy = new FutureProxyManager(this, handle);
    }
    client_handle =
        AllocInternal(kNoFunctionIndex, backing->data, [](void*) {});
    SetContextData(
        client_handle,
        new FutureProxyManager::UnregisterData(backing->proxy, client_handle),
        FutureProxyManager::UnregisterCallback);
    backing->proxy->RegisterClient(client_handle);
  }
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
}

void ReferenceCountedFutureImpl::MarkOrphaned() {
  MutexLock lock(mutex_);
  is_orphaned_ = true;
}

bool ReferenceCountedFutureImpl::MarkOrphanedIfRunningCallback() {
  MutexLock lock(mutex_);
  if (running_callback_count_ == 0) return false;
  is_orphaned_ = true;
  return true;
}

FutureHandle::FutureHandle() : id_(0), api_(nullptr) {}

FutureHandle::FutureHandle(FutureHandleId id, detail::FutureApiInterface* api)
    : id_(id), api_(api) {
  if (api != nullptr) {
    api->ReferenceFuture(*this);
    CleanupMgr(api).RegisterObject(this, CleanupFutureHandle);
  }
}

FutureHandle::~FutureHandle() { Detach(); }

FutureHandle::FutureHandle(const FutureHandle& rhs)
    : FutureHandle(rhs.id_, rhs.api_) {}

FutureHandle& FutureHandle::operator=(const FutureHandle& rhs) {
  if (this == &rhs) return *this;
  Detach();
  detail::FutureApiInterface* new_api = rhs.api_;
  id_ = rhs.id_;
  api_ = new_api;
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
  if (this == &rhs) return *this;
  Detach();
  detail::FutureApiInterface* rhs_api = rhs.api_;
  id_ = rhs.id_;
  api_ = rhs_api;
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
