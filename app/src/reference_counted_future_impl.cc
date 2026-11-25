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
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/intrusive_list.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"

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

const FutureHandle& ReferenceCountedFutureImpl::kInvalidHandle =
    []() -> const FutureHandle& {
  static internal::NoDestructor<FutureHandle> handle(kInvalidFutureHandle);
  return *handle;
}();

const char ReferenceCountedFutureImpl::kErrorMessageFutureIsNoLongerValid[] =
    "Invalid Future";

namespace detail {

namespace {

struct CompletionCallbackData {
  intrusive_list_node node;
  FutureBase::CompletionCallback completion_callback;
  void* callback_user_data;
  void (*callback_user_data_delete_fn)(void*);

  CompletionCallbackData(FutureBase::CompletionCallback callback,
                         void* user_data, void (*user_data_delete_fn)(void*))
      : completion_callback(callback),
        callback_user_data(user_data),
        callback_user_data_delete_fn(user_data_delete_fn) {}

  ~CompletionCallbackData() {
    if (callback_user_data_delete_fn != nullptr) {
      callback_user_data_delete_fn(callback_user_data);
    }
  }
};

using intrusive_list_iterator =
    intrusive_list<CompletionCallbackData>::iterator;

class CompletionMatcher {
 public:
  CompletionMatcher(FutureBase::CompletionCallback callback, void* user_data,
                    void (*user_data_delete_fn)(void*))
      : callback_(callback),
        user_data_(user_data),
        delete_fn_(user_data_delete_fn) {}

  bool operator()(const CompletionCallbackData& data) const {
    return data.completion_callback == callback_ &&
           data.callback_user_data == user_data_ &&
           data.callback_user_data_delete_fn == delete_fn_;
  }

 private:
  FutureBase::CompletionCallback callback_;
  void* user_data_;
  void (*delete_fn_)(void*);
};

#ifdef FIREBASE_USE_STD_FUNCTION
void CallStdFunction(const FutureBase& future, void* function_void) {
  if (function_void) {
    auto* function =
        static_cast<std::function<void(const FutureBase&)>*>(function_void);
    (*function)(future);
  }
}

void DeleteStdFunction(void* function_void) {
  if (function_void) {
    delete static_cast<std::function<void(const FutureBase&)>*>(function_void);
  }
}
#endif  // FIREBASE_USE_STD_FUNCTION

}  // namespace

class FutureProxyManager;

struct FutureBackingData {
  FutureBackingData(void* data, DataDeleteFn* delete_data_fn)
      : status(kFutureStatusPending),
        error(0),
        reference_count(0),
        active_callbacks(0),
        invalidated_after_callbacks(false),
        data(data),
        data_delete_fn(delete_data_fn),
        context_data(nullptr),
        context_data_delete_fn(nullptr),
        completion_single_callback(nullptr),
        completion_multiple_callbacks(&CompletionCallbackData::node) {}

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
    proxy.reset();
  }

  void ClearExistingCallbacks() {
    if (completion_single_callback != nullptr) {
      delete completion_single_callback;
      completion_single_callback = nullptr;
      reference_count--;
    }
    auto it = completion_multiple_callbacks.begin();
    while (it != completion_multiple_callbacks.end()) {
      CompletionCallbackData* entry = &*it;
      it = completion_multiple_callbacks.erase(it);
      delete entry;
      reference_count--;
    }
  }

  // Installs a new single callback and returns any previously installed
  // callback so the caller can destroy it outside `mutex_`.
  std::unique_ptr<CompletionCallbackData> SetSingleCallbackData(
      std::unique_ptr<CompletionCallbackData> callback) {
    std::unique_ptr<CompletionCallbackData> old(completion_single_callback);
    completion_single_callback = nullptr;
    if (old) {
      reference_count--;
    }
    if (callback) {
      completion_single_callback = callback.release();
      reference_count++;
    }
    return old;
  }

  void AddCallbackData(std::unique_ptr<CompletionCallbackData> callback) {
    if (!callback) return;
    reference_count++;
    completion_multiple_callbacks.push_back(*callback.release());
  }

  FutureStatus status;
  int error;
  std::string error_msg;
  uint32_t reference_count;
  int active_callbacks;
  bool invalidated_after_callbacks;
  void* data;
  DataDeleteFn* data_delete_fn;
  void* context_data;
  DataDeleteFn* context_data_delete_fn;
  CompletionCallbackData* completion_single_callback;
  intrusive_list<CompletionCallbackData> completion_multiple_callbacks;
  std::shared_ptr<FutureProxyManager> proxy;
};

class FutureImplCore : public FutureApiInterface,
                       public std::enable_shared_from_this<FutureImplCore> {
 public:
  FutureImplCore() : next_future_handle_(kInvalidFutureHandle + 1) {}

  ~FutureImplCore() override { Shutdown(); }

  std::shared_ptr<FutureApiInterface> SharedFromApi() override {
    return shared_from_this();
  }

  Mutex& mutex() { return mutex_; }

  void Shutdown() {
    std::vector<std::unique_ptr<FutureBackingData>> to_delete;
    {
      MutexLock lock(mutex_);
      if (is_shut_down_) return;
      is_shut_down_ = true;
      for (auto it = backings_.begin(); it != backings_.end();) {
        if (it->second->active_callbacks > 0) {
          // Keep backing valid while completion callbacks are executing; it
          // will be removed and destroyed immediately once they finish.
          it->second->invalidated_after_callbacks = true;
          ++it;
        } else {
          to_delete.push_back(std::move(it->second));
          it = backings_.erase(it);
        }
      }
      NotifyWaitersLocked();
    }
    // Destroy backings (and any unexecuted callback lambdas / result data)
    // outside `mutex_`.
    to_delete.clear();
  }

  FutureHandle AllocBacking(void* data, DataDeleteFn* delete_data_fn) {
    auto backing = std::make_unique<FutureBackingData>(data, delete_data_fn);
    MutexLock lock(mutex_);
    if (is_shut_down_) {
      return ReferenceCountedFutureImpl::kInvalidHandle;
    }
    const FutureHandleId id = AllocHandleIdLocked();
    FIREBASE_FUTURE_TRACE("API: Allocated handle id %d", id);
    backings_.emplace(id, std::move(backing));
    return FutureHandle(id, this);
  }

  bool ReferenceFuture(const FutureHandle& handle) override {
    MutexLock lock(mutex_);
    FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    if (backing == nullptr) {
      return false;
    }
    backing->reference_count++;
    FIREBASE_FUTURE_TRACE("API: Reference handle %d, ref count %d", handle.id(),
                          backing->reference_count);
    return true;
  }

  void ReleaseFuture(const FutureHandle& handle) override {
    std::unique_ptr<FutureBackingData> to_delete;
    {
      MutexLock lock(mutex_);
      FIREBASE_FUTURE_TRACE("API: Release future %d", (int)handle.id());
      auto it = backings_.find(handle.id());
      if (it == backings_.end()) {
        return;
      }
      FutureBackingData* backing = it->second.get();
      FIREBASE_ASSERT(backing->reference_count > 0);
      backing->reference_count--;
      if (backing->reference_count == 0 && backing->active_callbacks == 0) {
        to_delete = std::move(it->second);
        backings_.erase(it);
        NotifyWaitersLocked();
      }
    }
    // Destroy `to_delete` outside `mutex_`.
  }

  void ForceReleaseFuture(const FutureHandle& handle) {
    std::unique_ptr<FutureBackingData> to_delete;
    {
      MutexLock lock(mutex_);
      auto it = backings_.find(handle.id());
      if (it == backings_.end()) {
        return;
      }
      FutureBackingData* backing = it->second.get();
      backing->reference_count = 0;
      if (backing->active_callbacks == 0) {
        to_delete = std::move(it->second);
        backings_.erase(it);
        NotifyWaitersLocked();
      } else {
        backing->invalidated_after_callbacks = true;
      }
      FIREBASE_FUTURE_TRACE("API: ForceReleaseFuture handle %d", handle.id());
    }
  }

  FutureStatus GetFutureStatus(const FutureHandle& handle) const override {
    MutexLock lock(mutex_);
    const FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    return backing == nullptr ? kFutureStatusInvalid : backing->status;
  }

  int GetFutureError(const FutureHandle& handle) const override {
    MutexLock lock(mutex_);
    const FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    return backing == nullptr
               ? ReferenceCountedFutureImpl::kErrorFutureIsNoLongerValid
               : backing->error;
  }

  const char* GetFutureErrorMessage(const FutureHandle& handle) const override {
    MutexLock lock(mutex_);
    const FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    return backing == nullptr
               ? ReferenceCountedFutureImpl::kErrorMessageFutureIsNoLongerValid
               : backing->error_msg.c_str();
  }

  const void* GetFutureResult(const FutureHandle& handle) const override {
    MutexLock lock(mutex_);
    const FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    return backing == nullptr || backing->status != kFutureStatusComplete
               ? nullptr
               : backing->data;
  }

  bool WaitForFuture(const FutureHandle& handle,
                     int timeout_milliseconds) override {
    const bool infinite =
        (timeout_milliseconds == FutureBase::kWaitTimeoutInfinite);
    const auto deadline =
        infinite ? std::chrono::steady_clock::time_point::max()
                 : std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(
                           timeout_milliseconds < 0 ? 0 : timeout_milliseconds);
    for (;;) {
      uint64_t gen = 0;
      {
        MutexLock lock(mutex_);
        const FutureBackingData* backing = BackingFromHandleLocked(handle.id());
        if (backing == nullptr || backing->status == kFutureStatusInvalid) {
          return false;
        }
        if (backing->status == kFutureStatusComplete) {
          return true;
        }
        std::lock_guard<std::mutex> wl(wait_mutex_);
        gen = wait_generation_;
      }
      std::unique_lock<std::mutex> wl(wait_mutex_);
      if (wait_generation_ != gen) {
        continue;
      }
      if (infinite) {
        wait_cv_.wait(wl, [this, gen]() { return wait_generation_ != gen; });
      } else {
        if (!wait_cv_.wait_until(wl, deadline, [this, gen]() {
              return wait_generation_ != gen;
            })) {
          wl.unlock();
          return GetFutureStatus(handle) == kFutureStatusComplete;
        }
      }
    }
  }

  CompletionCallbackHandle AddCompletionCallback(
      const FutureHandle& handle, FutureBase::CompletionCallback callback,
      void* user_data, void (*user_data_delete_fn_ptr)(void*),
      bool single_completion) override {
    auto callback_data = std::make_unique<CompletionCallbackData>(
        callback, user_data, user_data_delete_fn_ptr);
    std::unique_ptr<CompletionCallbackData> displaced;

    mutex_.Acquire();
    FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    if (backing == nullptr) {
      mutex_.Release();
      return CompletionCallbackHandle();
    }

    if (single_completion) {
      displaced = backing->SetSingleCallbackData(std::move(callback_data));
    } else {
      backing->AddCallbackData(std::move(callback_data));
    }

    if (backing->status == kFutureStatusComplete) {
      ReleaseMutexAndRunCallbacks(handle);
      displaced.reset();
      return CompletionCallbackHandle();
    }
    mutex_.Release();
    displaced.reset();
    return CompletionCallbackHandle(callback, user_data,
                                    user_data_delete_fn_ptr);
  }

  void RemoveCompletionCallback(
      const FutureHandle& handle,
      CompletionCallbackHandle callback_handle) override {
    std::vector<std::unique_ptr<CompletionCallbackData>> to_delete_callbacks;
    std::unique_ptr<FutureBackingData> to_delete_backing;
    {
      MutexLock lock(mutex_);
      auto it_backing = backings_.find(handle.id());
      if (it_backing == backings_.end()) {
        return;
      }
      FutureBackingData* backing = it_backing->second.get();
      CompletionMatcher matches(callback_handle.callback_,
                                callback_handle.user_data_,
                                callback_handle.user_data_delete_fn_);
      if (backing->completion_single_callback != nullptr &&
          matches(*backing->completion_single_callback)) {
        to_delete_callbacks.emplace_back(backing->completion_single_callback);
        backing->completion_single_callback = nullptr;
        backing->reference_count--;
      }
      auto it = backing->completion_multiple_callbacks.begin();
      while (it != backing->completion_multiple_callbacks.end() &&
             !matches(*it)) {
        ++it;
      }
      if (it != backing->completion_multiple_callbacks.end()) {
        CompletionCallbackData* entry = &*it;
        backing->completion_multiple_callbacks.erase(it);
        to_delete_callbacks.emplace_back(entry);
        backing->reference_count--;
      }
      if (backing->reference_count == 0 && backing->active_callbacks == 0) {
        to_delete_backing = std::move(it_backing->second);
        backings_.erase(it_backing);
        NotifyWaitersLocked();
      }
    }
    // Destroy removed callback(s) and backing outside `mutex_`.
  }

#ifdef FIREBASE_USE_STD_FUNCTION
  CompletionCallbackHandle AddCompletionCallbackLambda(
      const FutureHandle& handle,
      std::function<void(const FutureBase&)> callback,
      bool single_completion) override {
    auto* fn_copy =
        new std::function<void(const FutureBase&)>(std::move(callback));
    return AddCompletionCallback(handle, CallStdFunction, fn_copy,
                                 DeleteStdFunction, single_completion);
  }
#endif  // FIREBASE_USE_STD_FUNCTION

  void CompleteWithPopulator(const FutureHandle& handle, int error,
                             const char* error_msg,
                             void (*populate_fn)(void* raw_data, void* ctx),
                             void* ctx);

  bool ValidFuture(FutureHandleId id) const {
    MutexLock lock(mutex_);
    return BackingFromHandleLocked(id) != nullptr;
  }

  bool IsSafeToDelete() const {
    MutexLock lock(mutex_);
    for (const auto& pair : backings_) {
      if (pair.second->status == kFutureStatusPending) return false;
    }
    return active_callbacks_ == 0;
  }

  bool IsRunningCallback() const {
    MutexLock lock(mutex_);
    return active_callbacks_ > 0;
  }

  int TotalReferenceCount() const {
    MutexLock lock(mutex_);
    int total = 0;
    for (const auto& pair : backings_) {
      total += static_cast<int>(pair.second->reference_count);
    }
    return total;
  }

  void SetContextData(const FutureHandle& handle, void* context_data,
                      void (*delete_context_data_fn)(void* data_to_delete)) {
    MutexLock lock(mutex_);
    FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    if (backing == nullptr) return;
    FIREBASE_ASSERT((delete_context_data_fn != nullptr) ||
                    (context_data == nullptr));
    backing->context_data = context_data;
    backing->context_data_delete_fn = delete_context_data_fn;
  }

#if defined(INTERNAL_EXPERIMENTAL)
  FutureBase MakeProxyForPending(const FutureBase& subject_future);
#endif  // defined(INTERNAL_EXPERIMENTAL)

 private:
  FutureHandleId AllocHandleIdLocked() {
    const FutureHandleId id = next_future_handle_++;
    if (next_future_handle_ == kInvalidFutureHandle) next_future_handle_++;
    return id;
  }

  FutureBackingData* BackingFromHandleLocked(FutureHandleId id) {
    auto it = backings_.find(id);
    return it == backings_.end() ? nullptr : it->second.get();
  }

  const FutureBackingData* BackingFromHandleLocked(FutureHandleId id) const {
    auto it = backings_.find(id);
    return it == backings_.end() ? nullptr : it->second.get();
  }

  void NotifyWaitersLocked() {
    {
      std::lock_guard<std::mutex> wl(wait_mutex_);
      ++wait_generation_;
    }
    wait_cv_.notify_all();
  }

  // Must be called with `mutex_` held via Acquire(); always releases `mutex_`.
  void ReleaseMutexAndRunCallbacks(const FutureHandle& handle) {
    auto self = shared_from_this();
    FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    if (backing == nullptr) {
      mutex_.Release();
      return;
    }

    std::optional<FutureBase> future_base;
    std::unique_ptr<FutureBackingData> to_delete_backing;

    if (backing->completion_single_callback != nullptr ||
        !backing->completion_multiple_callbacks.empty()) {
      ++active_callbacks_;
      ++backing->active_callbacks;
      future_base.emplace(this, handle);

      for (;;) {
        std::unique_ptr<CompletionCallbackData> data;
        if (backing->completion_single_callback != nullptr) {
          data.reset(backing->completion_single_callback);
          backing->completion_single_callback = nullptr;
        } else if (!backing->completion_multiple_callbacks.empty()) {
          CompletionCallbackData* front =
              &backing->completion_multiple_callbacks.front();
          backing->completion_multiple_callbacks.pop_front();
          data.reset(front);
        } else {
          break;
        }
        auto callback = data->completion_callback;
        auto user_data = data->callback_user_data;

        // Release `mutex_` before running the callback AND before destroying
        // `data` (its deleter may destroy a captured lambda holding the last
        // App/product handle).
        mutex_.Release();
        if (callback != nullptr) {
          callback(*future_base, user_data);
        }
        data.reset();
        mutex_.Acquire();

        FIREBASE_ASSERT(backing->reference_count > 0);
        backing->reference_count--;
      }

      --backing->active_callbacks;
      --active_callbacks_;
      if (backing->active_callbacks == 0 &&
          (backing->reference_count == 0 ||
           backing->invalidated_after_callbacks || is_shut_down_)) {
        auto it = backings_.find(handle.id());
        if (it != backings_.end() && it->second.get() == backing) {
          to_delete_backing = std::move(it->second);
          backings_.erase(it);
          NotifyWaitersLocked();
        }
      }
    }

    mutex_.Release();
    future_base.reset();
    to_delete_backing.reset();
  }

  mutable Mutex mutex_;
  std::map<FutureHandleId, std::unique_ptr<FutureBackingData>> backings_;
  FutureHandleId next_future_handle_;
  int active_callbacks_ = 0;
  bool is_shut_down_ = false;

  std::mutex wait_mutex_;
  std::condition_variable wait_cv_;
  uint64_t wait_generation_ = 0;
};

class FutureProxyManager
    : public std::enable_shared_from_this<FutureProxyManager> {
 public:
  FutureProxyManager(std::weak_ptr<FutureImplCore> core,
                     const FutureHandle& subject)
      : core_(std::move(core)), subject_(subject) {}

  ~FutureProxyManager() {
    std::vector<FutureHandle> clients_to_release;
    {
      MutexLock lock(mutex_);
      clients_to_release.swap(clients_);
    }
    if (auto core = core_.lock()) {
      for (const FutureHandle& h : clients_to_release) {
        if (h != ReferenceCountedFutureImpl::kInvalidHandle) {
          core->ForceReleaseFuture(h);
        }
      }
    }
  }

  void RegisterClient(const FutureHandle& handle) {
    if (auto core = core_.lock()) {
      MutexLock lock(mutex_);
      core->ReferenceFuture(subject_);
      clients_.push_back(handle);
    }
  }

  struct UnregisterData {
    UnregisterData(std::weak_ptr<FutureProxyManager> proxy,
                   const FutureHandle& handle)
        : proxy(std::move(proxy)), handle(handle) {}
    std::weak_ptr<FutureProxyManager> proxy;
    FutureHandle handle;
  };

  static void UnregisterCallback(void* data) {
    if (data == nullptr) return;
    std::unique_ptr<UnregisterData> udata(static_cast<UnregisterData*>(data));
    if (auto proxy = udata->proxy.lock()) {
      proxy->UnregisterClient(udata->handle);
    }
  }

  void UnregisterClient(const FutureHandle& handle) {
    bool found = false;
    {
      MutexLock lock(mutex_);
      for (FutureHandle& h : clients_) {
        if (h == handle) {
          h = ReferenceCountedFutureImpl::kInvalidHandle;
          found = true;
          break;
        }
      }
    }
    if (found) {
      if (auto core = core_.lock()) {
        // Release outside `mutex_` because dropping the last subject reference
        // may destroy `this`.
        core->ReleaseFuture(subject_);
      }
    }
  }

  void CompleteClients(int error, const char* error_msg) {
    std::vector<FutureHandle> snapshot;
    {
      MutexLock lock(mutex_);
      snapshot = clients_;
    }
    if (auto core = core_.lock()) {
      for (const FutureHandle& h : snapshot) {
        if (h != ReferenceCountedFutureImpl::kInvalidHandle) {
          core->CompleteWithPopulator(
              h, error, error_msg, [](void*, void*) {}, nullptr);
        }
      }
    }
  }

 private:
  std::vector<FutureHandle> clients_;
  std::weak_ptr<FutureImplCore> core_;
  FutureHandle subject_;
  mutable Mutex mutex_;
};

void FutureImplCore::CompleteWithPopulator(
    const FutureHandle& handle, int error, const char* error_msg,
    void (*populate_fn)(void* raw_data, void* ctx), void* ctx) {
  auto self = shared_from_this();
  std::shared_ptr<FutureProxyManager> proxy;
  std::string error_msg_copy;

  mutex_.Acquire();
  FutureBackingData* backing = BackingFromHandleLocked(handle.id());
  if (backing == nullptr) {
    mutex_.Release();
    return;
  }

  FIREBASE_ASSERT(backing->status == kFutureStatusPending);
  backing->error = error;
  backing->error_msg = error_msg == nullptr ? "" : error_msg;
  if (populate_fn != nullptr) {
    populate_fn(backing->data, ctx);
  }
  backing->status = kFutureStatusComplete;
  proxy = backing->proxy;
  if (proxy) {
    error_msg_copy = backing->error_msg;
  }
  NotifyWaitersLocked();

  // Releases `mutex_` and executes registered callbacks safely outside
  // `mutex_`.
  ReleaseMutexAndRunCallbacks(handle);

  // Complete proxy clients outside `mutex_` so proxy callbacks also run
  // unlocked.
  if (proxy) {
    proxy->CompleteClients(error, error_msg_copy.c_str());
  }
}

#if defined(INTERNAL_EXPERIMENTAL)
FutureBase FutureImplCore::MakeProxyForPending(
    const FutureBase& subject_future) {
  if (subject_future.status() != kFutureStatusPending) {
    return subject_future;
  }
  const FutureHandle handle = subject_future.GetHandle();

  FutureHandle client_handle;
  {
    MutexLock lock(mutex_);
    FutureBackingData* backing = BackingFromHandleLocked(handle.id());
    if (backing == nullptr || backing->status != kFutureStatusPending) {
      return subject_future;
    }
    if (!backing->proxy) {
      backing->proxy =
          std::make_shared<FutureProxyManager>(weak_from_this(), handle);
    }

    client_handle = AllocBacking(backing->data, [](void*) {});
    if (client_handle == ReferenceCountedFutureImpl::kInvalidHandle) {
      return subject_future;
    }
    SetContextData(
        client_handle,
        new FutureProxyManager::UnregisterData(backing->proxy, client_handle),
        FutureProxyManager::UnregisterCallback);
    backing->proxy->RegisterClient(client_handle);
  }

  return FutureBase(this, client_handle);
}
#endif  // defined(INTERNAL_EXPERIMENTAL)

FutureApiInterface::~FutureApiInterface() {}

}  // namespace detail

ReferenceCountedFutureImpl::ReferenceCountedFutureImpl(size_t last_result_count)
    : core_(std::make_shared<detail::FutureImplCore>()),
      last_results_(last_result_count) {}

ReferenceCountedFutureImpl::~ReferenceCountedFutureImpl() {
  for (size_t i = 0; i < last_results_.size(); ++i) {
    last_results_[i].Release();
  }
  core_->Shutdown();
}

std::shared_ptr<detail::FutureApiInterface>
ReferenceCountedFutureImpl::SharedFromApi() {
  return core_;
}

bool ReferenceCountedFutureImpl::ReferenceFuture(const FutureHandle& handle) {
  return core_->ReferenceFuture(handle);
}

void ReferenceCountedFutureImpl::ReleaseFuture(const FutureHandle& handle) {
  core_->ReleaseFuture(handle);
}

FutureStatus ReferenceCountedFutureImpl::GetFutureStatus(
    const FutureHandle& handle) const {
  return core_->GetFutureStatus(handle);
}

int ReferenceCountedFutureImpl::GetFutureError(
    const FutureHandle& handle) const {
  return core_->GetFutureError(handle);
}

const char* ReferenceCountedFutureImpl::GetFutureErrorMessage(
    const FutureHandle& handle) const {
  return core_->GetFutureErrorMessage(handle);
}

const void* ReferenceCountedFutureImpl::GetFutureResult(
    const FutureHandle& handle) const {
  return core_->GetFutureResult(handle);
}

bool ReferenceCountedFutureImpl::WaitForFuture(const FutureHandle& handle,
                                               int timeout_milliseconds) {
  return core_->WaitForFuture(handle, timeout_milliseconds);
}

detail::CompletionCallbackHandle
ReferenceCountedFutureImpl::AddCompletionCallback(
    const FutureHandle& handle, FutureBase::CompletionCallback callback,
    void* user_data, void (*user_data_delete_fn_ptr)(void*),
    bool single_completion) {
  return core_->AddCompletionCallback(
      handle, callback, user_data, user_data_delete_fn_ptr, single_completion);
}

void ReferenceCountedFutureImpl::RemoveCompletionCallback(
    const FutureHandle& handle,
    detail::CompletionCallbackHandle callback_handle) {
  core_->RemoveCompletionCallback(handle, callback_handle);
}

#ifdef FIREBASE_USE_STD_FUNCTION
detail::CompletionCallbackHandle
ReferenceCountedFutureImpl::AddCompletionCallbackLambda(
    const FutureHandle& handle, std::function<void(const FutureBase&)> callback,
    bool single_completion) {
  return core_->AddCompletionCallbackLambda(handle, std::move(callback),
                                            single_completion);
}
#endif  // FIREBASE_USE_STD_FUNCTION

FutureHandle ReferenceCountedFutureImpl::AllocInternal(
    int fn_idx, void* data, void (*delete_data_fn)(void* data_to_delete)) {
  FutureHandle handle = core_->AllocBacking(data, delete_data_fn);
  if (handle != kInvalidHandle && 0 <= fn_idx &&
      fn_idx < static_cast<int>(last_results_.size())) {
    FIREBASE_FUTURE_TRACE("API: Future handle %d (fn %d) --> %08x", handle.id(),
                          fn_idx, &last_results_[fn_idx]);
    last_results_[fn_idx] = FutureBase(this, handle);
  }
  FIREBASE_FUTURE_TRACE("API: Alloc complete.");
  return handle;
}

void ReferenceCountedFutureImpl::CompleteWithPopulator(
    const std::shared_ptr<detail::FutureImplCore>& core,
    const FutureHandle& handle, int error, const char* error_msg,
    void (*populate_fn)(void* raw_data, void* ctx), void* ctx) {
  core->CompleteWithPopulator(handle, error, error_msg, populate_fn, ctx);
}

bool ReferenceCountedFutureImpl::ValidFuture(FutureHandleId id) const {
  return core_->ValidFuture(id);
}

Mutex& ReferenceCountedFutureImpl::mutex() { return core_->mutex(); }

bool ReferenceCountedFutureImpl::IsSafeToDelete() const {
  return core_->IsSafeToDelete();
}

bool ReferenceCountedFutureImpl::IsRunningCallback() const {
  return core_->IsRunningCallback();
}

bool ReferenceCountedFutureImpl::IsReferencedExternally() const {
  int internal_references = 0;
  for (size_t i = 0; i < last_results_.size(); i++) {
    if (last_results_[i].status() != kFutureStatusInvalid) {
      internal_references++;
    }
  }
  return core_->TotalReferenceCount() > internal_references;
}

void ReferenceCountedFutureImpl::SetContextData(
    const FutureHandle& handle, void* context_data,
    void (*delete_context_data_fn)(void* data_to_delete)) {
  core_->SetContextData(handle, context_data, delete_context_data_fn);
}

void ReferenceCountedFutureImpl::ForceReleaseFuture(
    const FutureHandle& handle) {
  core_->ForceReleaseFuture(handle);
}

#if defined(INTERNAL_EXPERIMENTAL)
FutureBase ReferenceCountedFutureImpl::LastResultProxy(int fn_idx) {
  const FutureBase future = last_results_[fn_idx];
  return core_->MakeProxyForPending(future);
}
#endif  // defined(INTERNAL_EXPERIMENTAL)

// Implementation of FutureHandle from future.h.
FutureHandle::FutureHandle() : id_(0) {}

FutureHandle::FutureHandle(FutureHandleId id, detail::FutureApiInterface* api)
    : id_(id), api_(api != nullptr ? api->SharedFromApi() : nullptr) {
  if (api_ && !api_->ReferenceFuture(*this)) {
    api_.reset();
  }
}

FutureHandle::~FutureHandle() { Detach(); }

FutureHandle::FutureHandle(const FutureHandle& rhs)
    : id_(rhs.id_), api_(rhs.api_) {
  if (api_ && !api_->ReferenceFuture(*this)) {
    api_.reset();
  }
}

FutureHandle& FutureHandle::operator=(const FutureHandle& rhs) {
  if (this == &rhs) return *this;
  Detach();
  id_ = rhs.id_;
  api_ = rhs.api_;
  if (api_ && !api_->ReferenceFuture(*this)) {
    api_.reset();
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
FutureHandle::FutureHandle(FutureHandle&& rhs) noexcept
    : id_(rhs.id_), api_(std::move(rhs.api_)) {
  rhs.id_ = 0;
}

FutureHandle& FutureHandle::operator=(FutureHandle&& rhs) noexcept {
  if (this == &rhs) return *this;
  Detach();
  id_ = rhs.id_;
  api_ = std::move(rhs.api_);
  rhs.id_ = 0;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

void FutureHandle::Detach() {
  if (api_) {
    auto old_api = std::move(api_);
    old_api->ReleaseFuture(*this);
  }
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
