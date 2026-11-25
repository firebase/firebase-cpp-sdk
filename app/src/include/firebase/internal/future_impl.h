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

#if defined(FIREBASE_USE_MOVE_OPERATORS)
#include <utility>
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

namespace firebase {

class ReferenceCountedFutureImpl;

namespace detail {

class CompletionCallbackHandle;

/// Pure-virtual interface that APIs must implement to use Futures.
class FutureApiInterface {
 public:
  // typedef void FutureCallbackFn(const FutureBase* future);
  virtual ~FutureApiInterface();

  /// Returns the shared control block backing this FutureApiInterface.
  virtual std::shared_ptr<FutureApiInterface> SharedFromApi() = 0;

  /// Increment the reference count on handle's asynchronous call.
  /// Called when the Future is copied. Returns false if the handle is no
  /// longer valid.
  virtual bool ReferenceFuture(const FutureHandle& handle) = 0;

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

  /// Block until the future completes, becomes invalid/shut down, or times out.
  /// Returns true if the future completed, false on timeout or invalidation.
  virtual bool WaitForFuture(const FutureHandle& handle,
                             int timeout_milliseconds) = 0;

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

#if defined(FIREBASE_USE_STD_FUNCTION)
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
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
};

class CompletionCallbackHandle {
 public:
  // Construct a null CompletionCallbackHandle.
  CompletionCallbackHandle()
      : callback_(nullptr),
        user_data_(nullptr),
        user_data_delete_fn_(nullptr) {}

 private:
  friend class ::firebase::FutureBase;
  friend class ::firebase::ReferenceCountedFutureImpl;
  friend class FutureImplCore;
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

template <class T>
void Future<T>::OnCompletion(TypedCompletionCallback callback,
                             void* user_data) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    if (callback == nullptr) {
      api->AddCompletionCallback(
          handle, [](const FutureBase&, void*) {}, nullptr, nullptr,
          /*clear_existing_callbacks=*/true);
    } else {
      auto* data =
          new detail::TypedCompletionCallbackData<T>{callback, user_data};
      api->AddCompletionCallback(
          handle, detail::TypedCompletionCallbackTrampoline<T>, data,
          detail::DeleteTypedCompletionCallbackData<T>,
          /*clear_existing_callbacks=*/true);
    }
  }
}

#if defined(FIREBASE_USE_STD_FUNCTION)
template <class ResultType>
inline void Future<ResultType>::OnCompletion(
    std::function<void(const Future<ResultType>&)> callback) const {
  if (!callback) {
    FutureBase::OnCompletion([](const FutureBase&, void*) {}, nullptr);
  } else {
    FutureBase::OnCompletion([callback](const FutureBase& future) {
      callback(static_cast<const Future<ResultType>&>(future));
    });
  }
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION)

#if defined(INTERNAL_EXPERIMENTAL)
template <class T>
FutureBase::CompletionCallbackHandle Future<T>::AddOnCompletion(
    TypedCompletionCallback callback, void* user_data) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    if (callback == nullptr) {
      return CompletionCallbackHandle();
    }
    auto* data =
        new detail::TypedCompletionCallbackData<T>{callback, user_data};
    return api->AddCompletionCallback(
        handle, detail::TypedCompletionCallbackTrampoline<T>, data,
        detail::DeleteTypedCompletionCallbackData<T>,
        /*clear_existing_callbacks=*/false);
  }
  return CompletionCallbackHandle();
}

#if defined(FIREBASE_USE_STD_FUNCTION)
template <class ResultType>
inline FutureBase::CompletionCallbackHandle Future<ResultType>::AddOnCompletion(
    std::function<void(const Future<ResultType>&)> callback) const {
  if (!callback) {
    return CompletionCallbackHandle();
  }
  return FutureBase::AddOnCompletion([callback](const FutureBase& future) {
    callback(static_cast<const Future<ResultType>&>(future));
  });
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION)

#endif  // defined(INTERNAL_EXPERIMENTAL)

inline FutureBase::FutureBase()
    : mutex_(Mutex::Mode::kModeNonRecursive), api_(), handle_(0) {}

inline FutureBase::FutureBase(detail::FutureApiInterface* api,
                              const FutureHandle& handle)
    : mutex_(Mutex::Mode::kModeNonRecursive),
      api_(api != nullptr ? api->SharedFromApi() : nullptr),
      handle_(handle) {
  if (api_ && !api_->ReferenceFuture(handle_)) {
    api_.reset();
  }
  // Once the FutureBase has reference, we don't need extra handle reference.
  handle_.Detach();
}

inline FutureBase::~FutureBase() { Release(); }

inline FutureBase::FutureBase(const FutureBase& rhs)
    : mutex_(Mutex::Mode::kModeNonRecursive), api_() {
  *this = rhs;
}

inline FutureBase& FutureBase::operator=(const FutureBase& rhs) {
  if (this == &rhs) return *this;

  std::shared_ptr<detail::FutureApiInterface> new_api;
  FutureHandle new_handle;
  {
    MutexLock lock(rhs.mutex_);
    new_api = rhs.api_;
    new_handle = rhs.handle_;
  }

  if (new_api && !new_api->ReferenceFuture(new_handle)) {
    new_api.reset();
    new_handle = FutureHandle();
  }

  std::shared_ptr<detail::FutureApiInterface> old_api;
  FutureHandle old_handle;
  {
    MutexLock lock(mutex_);
    old_api = std::move(api_);
    old_handle = std::move(handle_);
    api_ = std::move(new_api);
    handle_ = std::move(new_handle);
  }

  if (old_api) {
    old_api->ReleaseFuture(old_handle);
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
inline FutureBase::FutureBase(FutureBase&& rhs) noexcept
    : mutex_(Mutex::Mode::kModeNonRecursive), api_() {
  *this = std::move(rhs);
}

inline FutureBase& FutureBase::operator=(FutureBase&& rhs) noexcept {
  if (this == &rhs) return *this;

  std::shared_ptr<detail::FutureApiInterface> new_api;
  FutureHandle new_handle;
  {
    MutexLock lock(rhs.mutex_);
    new_api = std::move(rhs.api_);
    new_handle = std::move(rhs.handle_);
  }

  std::shared_ptr<detail::FutureApiInterface> old_api;
  FutureHandle old_handle;
  {
    MutexLock lock(mutex_);
    old_api = std::move(api_);
    old_handle = std::move(handle_);
    api_ = std::move(new_api);
    handle_ = std::move(new_handle);
  }

  if (old_api) {
    old_api->ReleaseFuture(old_handle);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

inline void FutureBase::Release() {
  std::shared_ptr<detail::FutureApiInterface> old_api;
  FutureHandle old_handle;
  {
    MutexLock lock(mutex_);
    old_api = std::move(api_);
    old_handle = std::move(handle_);
  }
  if (old_api) {
    old_api->ReleaseFuture(old_handle);
  }
}

inline FutureStatus FutureBase::status() const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  return !api ? kFutureStatusInvalid : api->GetFutureStatus(handle);
}

inline int FutureBase::error() const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  return !api ? -1 : api->GetFutureError(handle);
}

inline const char* FutureBase::error_message() const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  return !api ? nullptr : api->GetFutureErrorMessage(handle);
}

inline const void* FutureBase::result_void() const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  return !api ? nullptr : api->GetFutureResult(handle);
}

inline void FutureBase::OnCompletion(CompletionCallback callback,
                                     void* user_data) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    api->AddCompletionCallback(handle, callback, user_data, nullptr,
                               /*clear_existing_callbacks=*/true);
  }
}

#if defined(INTERNAL_EXPERIMENTAL)
inline FutureBase::CompletionCallbackHandle FutureBase::AddOnCompletion(
    CompletionCallback callback, void* user_data) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    return api->AddCompletionCallback(handle, callback, user_data, nullptr,
                                      /*clear_existing_callbacks=*/false);
  }
  return CompletionCallbackHandle();
}

inline void FutureBase::RemoveOnCompletion(
    CompletionCallbackHandle completion_handle) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    api->RemoveCompletionCallback(handle, completion_handle);
  }
}
#endif  // defined(INTERNAL_EXPERIMENTAL)

#if defined(FIREBASE_USE_STD_FUNCTION)
inline void FutureBase::OnCompletion(
    std::function<void(const FutureBase&)> callback) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    api->AddCompletionCallbackLambda(handle, std::move(callback),
                                     /*clear_existing_callbacks=*/true);
  }
}

#if defined(INTERNAL_EXPERIMENTAL)
inline FutureBase::CompletionCallbackHandle FutureBase::AddOnCompletion(
    std::function<void(const FutureBase&)> callback) const {
  std::shared_ptr<detail::FutureApiInterface> api;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    api = api_;
    handle = handle_;
  }
  if (api) {
    return api->AddCompletionCallbackLambda(handle, std::move(callback),
                                            /*clear_existing_callbacks=*/false);
  }
  return CompletionCallbackHandle();
}
#endif  // defined(INTERNAL__EXPERIMENTAL)

#endif  // defined(FIREBASE_USE_STD_FUNCTION)

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

/// @endcond

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_
