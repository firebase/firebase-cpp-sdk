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

#ifndef FIREBASE_APP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_
#define FIREBASE_APP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_

#include <cassert>
#include <functional>
#include <map>
#include <vector>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {

struct FutureBackingData;

const FutureHandleId kInvalidFutureHandle = 0;

template <typename T>
class SafeFutureHandle {
 public:
  inline SafeFutureHandle() : handle_(kInvalidFutureHandle) {}
  inline explicit SafeFutureHandle(const FutureHandle& handle)
      : handle_(handle) {}
  inline const FutureHandle& get() const { return handle_; }
  void Detach() { handle_.Detach(); }

  static const SafeFutureHandle kInvalidHandle;

 private:
  FutureHandle handle_;
};

template <typename T>
const SafeFutureHandle<T> SafeFutureHandle<T>::kInvalidHandle;

class ReferenceCountedFutureImpl : public detail::FutureApiInterface {
 public:
  static const FutureHandle kInvalidHandle;
  static constexpr int kErrorFutureIsNoLongerValid = -1;
  static const char kErrorMessageFutureIsNoLongerValid[];
  static constexpr int kNoFunctionIndex = -1;

  explicit ReferenceCountedFutureImpl(size_t last_result_count)
      : next_future_handle_(kInvalidFutureHandle + 1),
        last_results_(last_result_count) {}
  ~ReferenceCountedFutureImpl() override;

  void ReferenceFuture(const FutureHandle& handle) override;
  void ReleaseFuture(const FutureHandle& handle) override;
  FutureStatus GetFutureStatus(const FutureHandle& handle) const override;
  int GetFutureError(const FutureHandle& handle) const override;
  const char* GetFutureErrorMessage(const FutureHandle& handle) const override;
  const void* GetFutureResult(const FutureHandle& handle) const override;

  detail::CompletionCallbackHandle AddCompletionCallback(
      const FutureHandle& handle, FutureBase::CompletionCallback callback,
      void* user_data, void (*user_data_delete_fn_ptr)(void*),
      bool single_completion) override;
  void RemoveCompletionCallback(
      const FutureHandle& handle,
      detail::CompletionCallbackHandle callback_handle) override;
#ifdef FIREBASE_USE_STD_FUNCTION
  detail::CompletionCallbackHandle AddCompletionCallbackLambda(
      const FutureHandle& handle,
      std::function<void(const FutureBase&)> callback,
      bool single_completion) override;
#endif  // FIREBASE_USE_STD_FUNCTION
  void RegisterFutureForCleanup(FutureBase* future) override;
  void UnregisterFutureForCleanup(FutureBase* future) override;

  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc(int fn_idx, const T& initial_data) {
    return AllocInternal(fn_idx, new T(initial_data), DeleteT<T>);
  }
  template <typename T>
  SafeFutureHandle<T> SafeAlloc(int fn_idx, const T& initial_data) {
    return SafeFutureHandle<T>(AllocInternal(fn_idx, initial_data));
  }
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc(int fn_idx) {
    return AllocInternal(fn_idx, new T, DeleteT<T>);
  }
  template <typename T>
  SafeFutureHandle<T> SafeAlloc(int fn_idx) {
    return SafeFutureHandle<T>(AllocInternal<T>(fn_idx));
  }
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc() {
    return AllocInternal<T>(kNoFunctionIndex);
  }
  template <typename T>
  SafeFutureHandle<T> SafeAlloc() {
    return SafeFutureHandle<T>(AllocInternal<T>(kNoFunctionIndex));
  }

  template <typename T, typename F>
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const char* error_msg,
                                    const F& populate_data_fn) {
    CompleteInternal<T>(handle, error, error_msg, populate_data_fn);
  }
  template <typename T, typename PopulateFn>
  void Complete(SafeFutureHandle<T> handle, int error, const char* error_msg,
                const PopulateFn& populate_data_fn) {
    CompleteInternal<T>(handle.get(), error, error_msg, populate_data_fn);
  }
  template <typename T, typename F>
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const F& populate_data_fn) {
    CompleteInternal<T, F>(handle, error, nullptr, populate_data_fn);
  }
  template <typename T, typename PopulateFn>
  void Complete(SafeFutureHandle<T> handle, int error,
                const PopulateFn& populate_data_fn) {
    CompleteInternal<T>(handle.get(), error, nullptr, populate_data_fn);
  }
  template <typename T>
  FIREBASE_DEPRECATED void CompleteWithResult(const FutureHandle& handle,
                                              int error, const char* error_msg,
                                              const T& result) {
    CompleteWithResultInternal(handle, error, error_msg, result);
  }
  template <typename T>
  void CompleteWithResult(SafeFutureHandle<T> handle, int error,
                          const char* error_msg, const T& result) {
    CompleteWithResultInternal(handle.get(), error, error_msg, result);
  }
  template <typename T>
  FIREBASE_DEPRECATED void CompleteWithResult(const FutureHandle& handle,
                                              int error, const T& result) {
    CompleteWithResultInternal(handle, error, nullptr, result);
  }
  template <typename T>
  void CompleteWithResult(SafeFutureHandle<T> handle, int error,
                          const T& result) {
    CompleteWithResultInternal(handle.get(), error, nullptr, result);
  }
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const char* error_msg) {
    CompleteInternal(handle, error, error_msg);
  }
  template <typename T>
  void Complete(SafeFutureHandle<T> handle, int error, const char* error_msg) {
    CompleteInternal(handle.get(), error, error_msg);
  }
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error) {
    CompleteInternal(handle, error, nullptr);
  }
  template <typename T>
  void Complete(SafeFutureHandle<T> handle, int error) {
    CompleteInternal(handle.get(), error, nullptr);
  }

  FIREBASE_DEPRECATED bool ValidFuture(const FutureHandle& handle) const {
    return ValidFuture(handle.id());
  }
  template <typename T>
  bool ValidFuture(SafeFutureHandle<T> handle) const {
    return ValidFuture(handle.get().id());
  }
  bool ValidFuture(FutureHandleId id) const {
    MutexLock lock(mutex_);
    return BackingFromHandle(id) != nullptr;
  }

  FutureBase LastResultProxy(int fn_idx);

  const FutureBase& LastResult(int fn_idx) const {
    MutexLock lock(mutex_);
    if (fn_idx < 0 || static_cast<size_t>(fn_idx) >= last_results_.size()) {
      static const FutureBase* kInvalidFuture = new FutureBase();
      return *kInvalidFuture;
    }
    return last_results_[fn_idx];
  }

  void InvalidateLastResult(int fn_idx) {
    if (0 <= fn_idx && static_cast<size_t>(fn_idx) < last_results_.size()) {
      MutexLock lock(last_results_mutex_);
      last_results_[fn_idx] = FutureBase();
    }
  }

  Mutex& mutex() const { return mutex_; }
  size_t GetLastResultCount() const {
    MutexLock lock(mutex_);
    return last_results_.size();
  }
  bool IsSafeToDelete() const;
  bool IsRunningCallback() const;
  bool IsReferencedExternally() const;
  void SetContextData(const FutureHandle& handle, void* context_data,
                      void (*delete_context_data_fn)(void* data_to_delete));

  TypedCleanupNotifier<FutureBase>& cleanup() { return cleanup_; }
  TypedCleanupNotifier<FutureHandle>& cleanup_handles() {
    return cleanup_handles_;
  }
  void ForceReleaseFuture(const FutureHandle& handle);
  void MarkOrphaned();
  bool MarkOrphanedIfRunningCallback();

 private:
  template <typename T>
  static void DeleteT(void* ptr_to_delete) {
    delete static_cast<T*>(ptr_to_delete);
  }

  FutureHandleId AllocHandleId() {
    const FutureHandleId id = next_future_handle_++;
    if (next_future_handle_ == kInvalidFutureHandle) next_future_handle_++;
    return id;
  }

  const FutureBackingData* BackingFromHandle(FutureHandleId id) const {
    return const_cast<ReferenceCountedFutureImpl*>(this)->BackingFromHandle(id);
  }
  FutureBackingData* BackingFromHandle(FutureHandleId id);

  FutureHandle AllocInternal(int fn_idx, void* data,
                             void (*delete_data_fn)(void* data_to_delete));
  template <typename T>
  FutureHandle AllocInternal(int fn_idx) {
    return AllocInternal(fn_idx, new T, DeleteT<T>);
  }
  template <typename T>
  FutureHandle AllocInternal(int fn_idx, const T& initial_data) {
    return AllocInternal(fn_idx, new T(initial_data), DeleteT<T>);
  }

  void* BackingData(FutureBackingData* backing);
  void SetBackingError(FutureBackingData* backing, int error,
                       const char* error_msg);
  void CompleteHandle(const FutureHandle& handle);

  template <typename T, typename F>
  void CompleteInternal(const FutureHandle& handle, int error,
                        const char* error_msg, const F& populate_data_fn) {
    mutex_.Acquire();
    FutureBackingData* backing = BackingFromHandle(handle.id());
    if (backing == nullptr) {
      mutex_.Release();
      return;
    }
    FIREBASE_ASSERT(GetFutureStatus(handle) == kFutureStatusPending);
    SetBackingError(backing, error, error_msg);
    populate_data_fn(static_cast<T*>(BackingData(backing)));
    CompleteHandle(handle);
    ReleaseMutexAndRunCallbacks(handle);
  }

  template <typename T>
  void CompleteWithResultInternal(const FutureHandle& handle, int error,
                                  const char* error_msg, const T& result) {
    CompleteInternal<T>(handle, error, error_msg,
                        [&result](T* data) { *data = result; });
  }

  void CompleteInternal(const FutureHandle& handle, int error,
                        const char* error_msg) {
    CompleteInternal<void>(handle, error, error_msg, [](void*) {});
  }

  void ReleaseMutexAndRunCallbacks(const FutureHandle& handle);
  void RunCallback(FutureBase* future_base,
                   FutureBase::CompletionCallback callback, void* user_data);
  void CleanupBackingIfUnreferenced(FutureHandleId id);

  mutable Mutex mutex_;
  mutable Mutex last_results_mutex_;
  std::map<FutureHandleId, FutureBackingData*> backings_;
  FutureHandleId next_future_handle_;
  std::vector<FutureBase> last_results_;
  TypedCleanupNotifier<FutureBase> cleanup_;
  TypedCleanupNotifier<FutureHandle> cleanup_handles_;
  int running_callback_count_ = 0;
  bool is_orphaned_ = false;
  bool is_deleting_ = false;
};

template <>
inline FutureHandle ReferenceCountedFutureImpl::AllocInternal<void>(
    int fn_idx) {
  return AllocInternal(fn_idx, nullptr, nullptr);
}

template <>
inline FutureHandle ReferenceCountedFutureImpl::Alloc<void>(int fn_idx) {
  return AllocInternal<void>(fn_idx);
}

template <>
inline SafeFutureHandle<void> ReferenceCountedFutureImpl::SafeAlloc<void>(
    int fn_idx) {
  return SafeFutureHandle<void>(AllocInternal<void>(fn_idx));
}

template <typename T>
Future<T> MakeFuture(ReferenceCountedFutureImpl* api,
                     const SafeFutureHandle<T>& handle) {
  return Future<T>(api, handle.get());
}

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_
