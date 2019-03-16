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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_

/// @cond FIREBASE_APP_INTERNAL

// You shouldn't include future_impl.h directly, since its just the inline
// implementation of the functions in future.h. Include future.h instead.
// This is here to ensure that presubmit tests pass.
#include "firebase/future.h"

#if defined(FIREBASE_USE_MOVE_OPERATORS)
#include <utility>
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace detail {

/// Pure-virtual interface that APIs must implement to use Futures.
class FutureApiInterface {
 public:
  // typedef void FutureCallbackFn(const FutureBase* future);
  virtual ~FutureApiInterface();

  /// Increment the reference count on handle's asynchronous call.
  /// Called when the Future is copied.
  virtual void ReferenceFuture(FutureHandle handle) = 0;

  /// Decrement the reference count on handle's asynchronous call.
  /// Called when the Future is destroyed or moved.
  /// If the reference count drops to zero, the asynchronous call can be
  /// forgotten.
  virtual void ReleaseFuture(FutureHandle handle) = 0;

  /// Return the status of the asynchronous call.
  virtual FutureStatus GetFutureStatus(FutureHandle handle) const = 0;

  /// Return the API-specific error.
  /// Valid when GetFutureStatus() is kFutureStatusComplete, and undefined
  /// otherwise.
  virtual int GetFutureError(FutureHandle handle) const = 0;

  /// Return the API-specific error, in human-readable form, or "" if no message
  /// has been provided.
  /// Valid when GetFutureStatus() is kFutureStatusComplete, and undefined
  /// otherwise.
  virtual const char* GetFutureErrorMessage(FutureHandle handle) const = 0;

  /// Return a pointer to the completed asynchronous result, or NULL if
  /// result is still pending.
  /// After an asynchronous call is marked complete, the API should not
  /// modify the result (especially on a callback thread), since the threads
  /// owning the Future can reference the result memory via this function.
  virtual const void* GetFutureResult(FutureHandle handle) const = 0;

  /// Register a callback that will be called when this future's status is set
  /// to Complete. The result data will be passed back when the callback is
  /// called, along with the user_data supplied here.
  virtual void SetCompletionCallback(FutureHandle handle,
                                     FutureBase::CompletionCallback callback,
                                     void* user_data) = 0;

#ifdef FIREBASE_USE_STD_FUNCTION
  /// Register a callback that will be called when this future's status is set
  /// to Complete. The result data will be passed back when the callback is
  /// called.
  virtual void SetCompletionCallbackLambda(
      FutureHandle handle, std::function<void(const FutureBase&)> callback) = 0;
#endif  // FIREBASE_USE_STD_FUNCTION

  /// Register this Future instance to be cleaned up.
  virtual void RegisterFutureForCleanup(FutureBase* future) = 0;

  /// Unregister this Future instance from the cleanup list.
  virtual void UnregisterFutureForCleanup(FutureBase* future) = 0;
};

inline void RegisterForCleanup(FutureApiInterface* api, FutureBase* future) {
  if (api != NULL) {  // NOLINT
    api->RegisterFutureForCleanup(future);
  }
}

inline void UnregisterForCleanup(FutureApiInterface* api, FutureBase* future) {
  if (api != NULL) {  // NOLINT
    api->UnregisterFutureForCleanup(future);
  }
}

}  // namespace detail

inline FutureBase::FutureBase() : api_(NULL), handle_(0) {}  // NOLINT

inline FutureBase::FutureBase(detail::FutureApiInterface* api,
                              FutureHandle handle)
    : api_(api), handle_(handle) {
  api_->ReferenceFuture(handle_);
  detail::RegisterForCleanup(api_, this);
}

inline FutureBase::~FutureBase() { Release(); }

inline FutureBase::FutureBase(const FutureBase& rhs)
    : api_(NULL)  // NOLINT
{                 // NOLINT
  *this = rhs;
  detail::RegisterForCleanup(api_, this);
}

inline FutureBase& FutureBase::operator=(const FutureBase& rhs) {
  Release();
  api_ = rhs.api_;
  handle_ = rhs.handle_;
  if (api_ != NULL) {  // NOLINT
    api_->ReferenceFuture(handle_);
  }
  detail::RegisterForCleanup(api_, this);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
inline FutureBase::FutureBase(FutureBase&& rhs)
    : api_(NULL)  // NOLINT
{
  detail::UnregisterForCleanup(rhs.api_, &rhs);
  *this = std::move(rhs);
  detail::RegisterForCleanup(api_, this);
}

inline FutureBase& FutureBase::operator=(FutureBase&& rhs) {
  Release();
  detail::UnregisterForCleanup(rhs.api_, &rhs);
  api_ = rhs.api_;
  handle_ = rhs.handle_;
  rhs.api_ = NULL;  // NOLINT
  detail::RegisterForCleanup(api_, this);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

inline void FutureBase::Release() {
  if (api_ != NULL) {  // NOLINT
    detail::UnregisterForCleanup(api_, this);
    api_->ReleaseFuture(handle_);
    api_ = NULL;  // NOLINT
  }
}

inline FutureStatus FutureBase::status() const {
  return api_ == NULL ?  // NOLINT
             kFutureStatusInvalid
                      : api_->GetFutureStatus(handle_);
}

inline int FutureBase::error() const {
  return api_ == NULL ? -1 : api_->GetFutureError(handle_);  // NOLINT
}

inline const char* FutureBase::error_message() const {
  return api_ == NULL ? NULL : api_->GetFutureErrorMessage(handle_);  // NOLINT
}

inline const void* FutureBase::result_void() const {
  return api_ == NULL ? NULL : api_->GetFutureResult(handle_);  // NOLINT
}

inline void FutureBase::OnCompletion(CompletionCallback callback,
                                     void* user_data) const {
  if (api_ != NULL) {  // NOLINT
    api_->SetCompletionCallback(handle_, callback, user_data);
  }
}

#ifdef FIREBASE_USE_STD_FUNCTION
inline void FutureBase::OnCompletion(
    std::function<void(const FutureBase&)> callback) const {
  if (api_ != NULL) {  // NOLINT
    api_->SetCompletionCallbackLambda(handle_, callback);
  }
}
#endif  // FIREBASE_USE_STD_FUNCTION

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

/// @endcond

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INCLUDE_FIREBASE_INTERNAL_FUTURE_IMPL_H_
