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

#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_

/// @cond FIREBASE_APP_INTERNAL

// You shouldn't include future_impl.h directly, since its just the inline
// implementation of the functions in future.h. Include future.h instead.
#include "firebase/future.h"

#include <functional>
#include <memory>
#include <utility>

namespace firebase {

class ReferenceCountedFutureImpl;

namespace detail {

class CompletionCallbackHandle;

/// Pure-virtual interface that APIs must implement to use Futures.
class FutureApiInterface {
 public:
  // typedef void FutureCallbackFn(const FutureBase* future);
  virtual ~FutureApiInterface();

  /// Increment the reference count on handle's asynchronous call.
  /// Called when the Future is copied.
  virtual void ReferenceFuture(const FutureHandle& handle) = 0;

  /// Decrement the reference count on handle's asynchronous call.
  /// Called when the Future is destroyed or moved.
  /// If the reference count drops to zero, the asynchronous call can be
  /// forgotten.
  virtual void ReleaseFuture(const FutureHandle& handle) = 0;

  /// Return the status of the asynchronous call.
  virtual FutureStatus GetFutureStatus(const FutureHandle& handle) const = 0;

  /// Return the API-specific error.
  /// Valid when GetFutureStatus() is kFutureStatusComplete, and undefined
  /// otherwise.
  virtual int GetFutureError(const FutureHandle& handle) const = 0;

  /// Return the API-specific error, in human-readable form, or "" if no message
  /// has been provided.
  /// Valid when GetFutureStatus() is kFutureStatusComplete, and undefined
  /// otherwise.
  virtual const char* GetFutureErrorMessage(
      const FutureHandle& handle) const = 0;

  /// Return a pointer to the completed asynchronous result, or NULL if
  /// result is still pending.
  /// After an asynchronous call is marked complete, the API should not
  /// modify the result (especially on a callback thread), since the threads
  /// owning the Future can reference the result memory via this function.
  virtual const void* GetFutureResult(const FutureHandle& handle) const = 0;

  /// Register a callback that will be called when this future's status is set
  /// to Complete. If clear_existing_callbacks is true, then the new callback
  /// will replace any existing callbacks, otherwise it will be added to the
  /// list of callbacks.
  ///
  /// The future's result data will be passed back when the callback is
  /// called, along with the user_data supplied here.
  ///
  /// After the callback has been called, if `user_data_delete_fn_ptr` is
  /// non-null, then `(*user_data_delete_fn_ptr)(user_data)` will be called.
  virtual CompletionCallbackHandle AddCompletionCallback(
      const FutureHandle& handle, FutureBase::CompletionCallback callback,
      void* user_data, void (*user_data_delete_fn)(void*),
      bool clear_existing_callbacks) = 0;

  /// Unregister a callback that was previously registered with
  /// `AddCompletionCallback`.
  virtual void RemoveCompletionCallback(
      const FutureHandle& handle, CompletionCallbackHandle callback_handle) = 0;

  /// Register a callback that will be called when this future's status is set
  /// to Complete.
  ///
  /// If `clear_existing_callbacks` is true, then the new callback
  /// will replace any existing callbacks, otherwise it will be added to the
  /// list of callbacks.
  ///
  /// The future's result data will be passed back when the callback is
  /// called.
  ///
  /// @return A handle that can be passed to `FutureBase::RemoveCompletion`.
  virtual CompletionCallbackHandle AddCompletionCallbackLambda(
      const FutureHandle& handle,
      std::function<void(const FutureBase&)> callback,
      bool clear_existing_callbacks) = 0;

  /// Register this Future instance to be cleaned up.
  virtual void RegisterFutureForCleanup(FutureBase* future) = 0;

  /// Unregister this Future instance from the cleanup list.
  virtual void UnregisterFutureForCleanup(FutureBase* future) = 0;
};

inline void RegisterForCleanup(FutureApiInterface* api, FutureBase* future) {
  if (api != nullptr) {
    api->RegisterFutureForCleanup(future);
  }
}

inline void UnregisterForCleanup(FutureApiInterface* api, FutureBase* future) {
  if (api != nullptr) {
    api->UnregisterFutureForCleanup(future);
  }
}

class CompletionCallbackHandle {
 public:
  // Construct a null CompletionCallbackHandle.
  CompletionCallbackHandle()
      : callback_(nullptr),
        user_data_(nullptr),
        user_data_delete_fn_(nullptr) {}

  bool is_valid() const { return callback_ != nullptr; }
  explicit operator bool() const { return is_valid(); }

  bool operator==(const CompletionCallbackHandle& rhs) const {
    return callback_ == rhs.callback_ && user_data_ == rhs.user_data_ &&
           user_data_delete_fn_ == rhs.user_data_delete_fn_;
  }
  bool operator!=(const CompletionCallbackHandle& rhs) const {
    return !(*this == rhs);
  }

 private:
  friend class ::firebase::FutureBase;
  friend class ::firebase::ReferenceCountedFutureImpl;
  CompletionCallbackHandle(FutureBase::CompletionCallback callback,
                           void* user_data, void (*user_data_delete_fn)(void*))
      : callback_(callback),
        user_data_(user_data),
        user_data_delete_fn_(user_data_delete_fn) {}

  FutureBase::CompletionCallback callback_;
  void* user_data_;
  void (*user_data_delete_fn_)(void*);
};

template <typename T>
struct TypedCompletionCallbackData {
  typename Future<T>::TypedCompletionCallback callback;
  void* user_data;
};

template <typename T>
inline void TypedCompletionCallbackTrampoline(const FutureBase& future,
                                              void* data_ptr) {
  auto* data = static_cast<TypedCompletionCallbackData<T>*>(data_ptr);
  if (data != nullptr && data->callback != nullptr) {
    data->callback(static_cast<const Future<T>&>(future), data->user_data);
  }
}

template <typename T>
inline void DeleteTypedCompletionCallbackData(void* data_ptr) {
  delete static_cast<TypedCompletionCallbackData<T>*>(data_ptr);
}

}  // namespace detail

template <typename R, typename Fn>
inline R FutureBase::CallWithPinnedApi(Fn&& fn) const {
  // `pinned` is registered with the API's CleanupNotifier, and the API's
  // destructor must lock `pinned.mutex_` to release it, so holding that
  // private mutex keeps the API alive. `mutex_` itself stays unlocked, since a
  // completion callback run synchronously by `fn` may use `*this`.
  FutureBase pinned(*this);
  MutexLock lock(pinned.mutex_);
  if (pinned.api_ == nullptr) {
    return R();
  }
  return fn(pinned.api_, pinned.handle_);
}

template <class T>
void Future<T>::OnCompletion(TypedCompletionCallback callback,
                             void* user_data) const {
  CallWithPinnedApi<void>([&](detail::FutureApiInterface* api,
                              const FutureHandle& handle) {
    if (callback == nullptr) {
      api->AddCompletionCallback(handle, nullptr, nullptr, nullptr,
                                 /*clear_existing_callbacks=*/true);
      return;
    }
    auto data = std::make_unique<detail::TypedCompletionCallbackData<T>>(
        detail::TypedCompletionCallbackData<T>{callback, user_data});
    api->AddCompletionCallback(
        handle, detail::TypedCompletionCallbackTrampoline<T>, data.release(),
        detail::DeleteTypedCompletionCallbackData<T>,
        /*clear_existing_callbacks=*/true);
  });
}

template <class ResultType>
inline void Future<ResultType>::OnCompletion(
    std::function<void(const Future<ResultType>&)> callback) const {
  if (!callback) {
    FutureBase::OnCompletion(nullptr, nullptr);
  } else {
    FutureBase::OnCompletion(
        [callback = std::move(callback)](const FutureBase& future) {
          callback(static_cast<const Future<ResultType>&>(future));
        });
  }
}

template <class T>
FutureBase::CompletionCallbackHandle Future<T>::AddOnCompletion(
    TypedCompletionCallback callback, void* user_data) const {
  if (callback == nullptr) {
    return CompletionCallbackHandle();
  }
  return CallWithPinnedApi<CompletionCallbackHandle>(
      [&](detail::FutureApiInterface* api, const FutureHandle& handle) {
        auto data = std::make_unique<detail::TypedCompletionCallbackData<T>>(
            detail::TypedCompletionCallbackData<T>{callback, user_data});
        return api->AddCompletionCallback(
            handle, detail::TypedCompletionCallbackTrampoline<T>,
            data.release(), detail::DeleteTypedCompletionCallbackData<T>,
            /*clear_existing_callbacks=*/false);
      });
}

template <class ResultType>
inline FutureBase::CompletionCallbackHandle Future<ResultType>::AddOnCompletion(
    std::function<void(const Future<ResultType>&)> callback) const {
  if (!callback) {
    return CompletionCallbackHandle();
  }
  return FutureBase::AddOnCompletion(
      [callback = std::move(callback)](const FutureBase& future) {
        callback(static_cast<const Future<ResultType>&>(future));
      });
}

inline FutureBase::FutureBase()
    : mutex_(Mutex::Mode::kModeNonRecursive), api_(nullptr), handle_(0) {}

inline FutureBase::FutureBase(detail::FutureApiInterface* api,
                              const FutureHandle& handle)
    : mutex_(Mutex::Mode::kModeNonRecursive), api_(api), handle_(handle) {
  if (api_ != nullptr) {
    api_->ReferenceFuture(handle_);
  }
  // Once the FutureBase has reference, we don't need extra handle reference.
  handle_.Detach();
  if (api_ == nullptr) {
    handle_ = FutureHandle();
  }
  detail::RegisterForCleanup(api_, this);
}

inline FutureBase::~FutureBase() { Release(); }

// Lock order: FutureBase::mutex_ -> CleanupNotifier -> API mutex. Only
// Release() unregisters from the API's CleanupNotifier, since that may have to
// wait for a concurrent CleanupAll() and must not hold any other FutureBase's
// mutex while doing so.

inline FutureBase::FutureBase(const FutureBase& rhs)
    : mutex_(Mutex::Mode::kModeNonRecursive), api_(nullptr), handle_(0) {
  MutexLock lock(rhs.mutex_);
  if (rhs.api_ != nullptr) {
    api_ = rhs.api_;
    handle_ = rhs.handle_;
    api_->ReferenceFuture(handle_);
    detail::RegisterForCleanup(api_, this);
  }
}

inline FutureBase& FutureBase::operator=(const FutureBase& rhs) {
  if (this == &rhs) {
    return *this;
  }
  while (true) {
    Release();
    Mutex& first_mutex =
        std::less<const FutureBase*>()(this, &rhs) ? mutex_ : rhs.mutex_;
    Mutex& second_mutex =
        std::less<const FutureBase*>()(this, &rhs) ? rhs.mutex_ : mutex_;
    MutexLock first_lock(first_mutex);
    MutexLock second_lock(second_mutex);
    if (api_ != nullptr) {
      continue;  // A concurrent assignment to *this won; release it too.
    }
    if (rhs.api_ != nullptr) {
      api_ = rhs.api_;
      handle_ = rhs.handle_;
      api_->ReferenceFuture(handle_);
      detail::RegisterForCleanup(api_, this);
    }
    return *this;
  }
}

inline FutureBase::FutureBase(FutureBase&& rhs) noexcept
    : FutureBase(static_cast<const FutureBase&>(rhs)) {
  rhs.Release();
}

inline FutureBase& FutureBase::operator=(FutureBase&& rhs) noexcept {
  if (this != &rhs) {
    *this = static_cast<const FutureBase&>(rhs);
    rhs.Release();
  }
  return *this;
}

inline void FutureBase::Release() {
  MutexLock lock(mutex_);
  if (api_ != nullptr) {
    // If a concurrent CleanupAll() is already cleaning up this future, this
    // waits for it with `mutex_` temporarily released, and that cleanup
    // releases the future on our behalf.
    detail::UnregisterForCleanup(api_, this);
    if (api_ != nullptr) {
      api_->ReleaseFuture(handle_);
      api_ = nullptr;
      handle_ = FutureHandle();
    }
  }
}

inline FutureStatus FutureBase::status() const {
  MutexLock lock(mutex_);
  return api_ == nullptr ? kFutureStatusInvalid : api_->GetFutureStatus(handle_);
}

inline int FutureBase::error() const {
  MutexLock lock(mutex_);
  return api_ == nullptr ? -1 : api_->GetFutureError(handle_);
}

inline const char* FutureBase::error_message() const {
  MutexLock lock(mutex_);
  return api_ == nullptr ? nullptr : api_->GetFutureErrorMessage(handle_);
}

inline const void* FutureBase::result_void() const {
  MutexLock lock(mutex_);
  return api_ == nullptr ? nullptr : api_->GetFutureResult(handle_);
}

inline void FutureBase::OnCompletion(CompletionCallback callback,
                                     void* user_data) const {
  CallWithPinnedApi<void>(
      [&](detail::FutureApiInterface* api, const FutureHandle& handle) {
        api->AddCompletionCallback(handle, callback, user_data, nullptr,
                                   /*clear_existing_callbacks=*/true);
      });
}

inline FutureBase::CompletionCallbackHandle FutureBase::AddOnCompletion(
    CompletionCallback callback, void* user_data) const {
  if (callback == nullptr) {
    return CompletionCallbackHandle();
  }
  return CallWithPinnedApi<CompletionCallbackHandle>(
      [&](detail::FutureApiInterface* api, const FutureHandle& handle) {
        return api->AddCompletionCallback(handle, callback, user_data, nullptr,
                                          /*clear_existing_callbacks=*/false);
      });
}

inline void FutureBase::RemoveOnCompletion(
    CompletionCallbackHandle completion_handle) const {
  CallWithPinnedApi<void>(
      [&](detail::FutureApiInterface* api, const FutureHandle& handle) {
        api->RemoveCompletionCallback(handle, completion_handle);
      });
}

inline void FutureBase::OnCompletion(
    std::function<void(const FutureBase&)> callback) const {
  CallWithPinnedApi<void>(
      [&](detail::FutureApiInterface* api, const FutureHandle& handle) {
        api->AddCompletionCallbackLambda(handle, std::move(callback),
                                         /*clear_existing_callbacks=*/true);
      });
}

inline FutureBase::CompletionCallbackHandle FutureBase::AddOnCompletion(
    std::function<void(const FutureBase&)> callback) const {
  if (!callback) {
    return CompletionCallbackHandle();
  }
  return CallWithPinnedApi<CompletionCallbackHandle>(
      [&](detail::FutureApiInterface* api, const FutureHandle& handle) {
        return api->AddCompletionCallbackLambda(
            handle, std::move(callback),
            /*clear_existing_callbacks=*/false);
      });
}

}  // namespace firebase

/// @endcond

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_
