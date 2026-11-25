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
#include <memory>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/no_destructor.h"

namespace firebase {

namespace detail {
class FutureImplCore;
}  // namespace detail

// Value for an invalid future handle. Default futures (which don't reference
// any real operation) have this handle ID.
const FutureHandleId kInvalidFutureHandle = 0;

/// @brief Type-safe future handle.
///
/// Trying to complete a SafeFutureHandle<T> with an incompatible result type
/// won't compile instead of leading to undefined behavior.
template <typename T>
class SafeFutureHandle {
 public:
  inline SafeFutureHandle() : handle_(kInvalidFutureHandle) {}
  inline explicit SafeFutureHandle(const FutureHandle& handle)
      : handle_(handle) {}
  inline const FutureHandle& get() const { return handle_; }
  // See FutureHandle::Detach.
  void Detach() { handle_.Detach(); }

  static const SafeFutureHandle& kInvalidHandle;

 private:
  FutureHandle handle_;
};

// Type-safe version of kInvalidFutureHandle (backed by NoDestructor to avoid
// exit-time destructors).
template <typename T>
const SafeFutureHandle<T>& SafeFutureHandle<T>::kInvalidHandle =
    []() -> const SafeFutureHandle<T>& {
  static internal::NoDestructor<SafeFutureHandle<T>> instance;
  return *instance;
}();

/// @brief Backing class for Futures that allows a Future to have multiple
///        copies. When no copies remain, the Future is invalidated.
///
/// Internally delegates all Future storage and callback execution to a
/// heap-allocated `std::shared_ptr<detail::FutureImplCore>`, so external
/// `Future<T>`, `FutureHandle`, and in-flight `OnCompletion` callbacks remain
/// completely memory-safe even if the owning `ReferenceCountedFutureImpl` or
/// `AppState` is destroyed concurrently or re-entrantly from a callback.
class ReferenceCountedFutureImpl : public detail::FutureApiInterface {
 public:
  /// This handle will never be returned by @ref Alloc, so you can use it
  /// to signify an uninitialized or invalid value.
  static const FutureHandle& kInvalidHandle;

  /// Returned by @ref GetFutureError when the passed in handle is invalid.
  static constexpr int kErrorFutureIsNoLongerValid = -1;

  /// Returned by @ref GetFutureErrorMessage when the passed in handle is
  /// invalid.
  static const char kErrorMessageFutureIsNoLongerValid[];

  /// Pass into @ref Alloc for `fn_idx` when you don't want to update any
  /// function.
  static constexpr int kNoFunctionIndex = -1;

  explicit ReferenceCountedFutureImpl(size_t last_result_count);
  ~ReferenceCountedFutureImpl() override;

  ReferenceCountedFutureImpl(const ReferenceCountedFutureImpl&) = delete;
  ReferenceCountedFutureImpl& operator=(const ReferenceCountedFutureImpl&) =
      delete;

  // Implementation of detail::FutureApiInterface.
  std::shared_ptr<detail::FutureApiInterface> SharedFromApi() override;
  bool ReferenceFuture(const FutureHandle& handle) override;
  void ReleaseFuture(const FutureHandle& handle) override;
  FutureStatus GetFutureStatus(const FutureHandle& handle) const override;
  int GetFutureError(const FutureHandle& handle) const override;
  const char* GetFutureErrorMessage(const FutureHandle& handle) const override;
  const void* GetFutureResult(const FutureHandle& handle) const override;
  bool WaitForFuture(const FutureHandle& handle,
                     int timeout_milliseconds) override;

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

  /// @deprecated use SafeAlloc instead.
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc(int fn_idx, const T& initial_data) {
    return AllocInternal(fn_idx, new T(initial_data), DeleteT<T>);
  }

  /// Safe version of Alloc.
  template <typename T>
  SafeFutureHandle<T> SafeAlloc(int fn_idx, const T& initial_data) {
    return SafeFutureHandle<T>(AllocInternal(fn_idx, initial_data));
  }

  /// @deprecated use SafeAlloc instead.
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc(int fn_idx) {
    return AllocInternal(fn_idx, new T, DeleteT<T>);
  }

  /// Safe version of Alloc.
  template <typename T>
  SafeFutureHandle<T> SafeAlloc(int fn_idx) {
    return SafeFutureHandle<T>(AllocInternal<T>(fn_idx));
  }

  /// @deprecated use SafeAlloc instead.
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc() {
    return AllocInternal<T>(kNoFunctionIndex);
  }

  /// Safe version of Alloc.
  template <typename T>
  SafeFutureHandle<T> SafeAlloc() {
    return SafeFutureHandle<T>(AllocInternal<T>(kNoFunctionIndex));
  }

  /// @deprecated use safe overload instead.
  template <typename T, typename F>
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const char* error_msg,
                                    const F& populate_data_fn) {
    CompleteInternal<T>(handle, error, error_msg, populate_data_fn);
  }

  /// Safe overload of Complete.
  template <typename T, typename PopulateFn>
  void Complete(SafeFutureHandle<T> handle, int error, const char* error_msg,
                const PopulateFn& populate_data_fn) {
    CompleteInternal<T>(handle.get(), error, error_msg, populate_data_fn);
  }

  /// @deprecated use safe overload instead.
  template <typename T, typename F>
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const F& populate_data_fn) {
    CompleteInternal<T, F>(handle, error, nullptr, populate_data_fn);
  }

  /// Safe overload of Complete.
  template <typename T, typename PopulateFn>
  void Complete(SafeFutureHandle<T> handle, int error,
                const PopulateFn& populate_data_fn) {
    CompleteInternal<T>(handle.get(), error, nullptr, populate_data_fn);
  }

  /// @deprecated use safe overload instead.
  template <typename T>
  FIREBASE_DEPRECATED void CompleteWithResult(const FutureHandle& handle,
                                              int error, const char* error_msg,
                                              const T& result) {
    CompleteWithResultInternal(handle, error, error_msg, result);
  }

  /// Safe overload of CompleteWithResult.
  template <typename T>
  void CompleteWithResult(SafeFutureHandle<T> handle, int error,
                          const char* error_msg, const T& result) {
    CompleteWithResultInternal(handle.get(), error, error_msg, result);
  }

  /// @deprecated use safe overload instead.
  template <typename T>
  FIREBASE_DEPRECATED void CompleteWithResult(const FutureHandle& handle,
                                              int error, const T& result) {
    CompleteWithResultInternal(handle, error, nullptr, result);
  }

  /// Safe overload of CompleteWithResult.
  template <typename T>
  void CompleteWithResult(SafeFutureHandle<T> handle, int error,
                          const T& result) {
    CompleteWithResultInternal(handle.get(), error, nullptr, result);
  }

  /// @deprecated use safe overload instead.
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const char* error_msg) {
    CompleteInternal<void>(handle, error, error_msg);
  }

  /// Safe overload of Complete.
  template <typename T>
  void Complete(SafeFutureHandle<T> handle, int error, const char* error_msg) {
    CompleteInternal<T>(handle.get(), error, error_msg);
  }

  /// @deprecated use safe overload instead.
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error) {
    CompleteInternal<void>(handle, error, nullptr);
  }

  /// Safe overload of Complete.
  template <typename T>
  void Complete(SafeFutureHandle<T> handle, int error) {
    CompleteInternal<T>(handle.get(), error, nullptr);
  }

  /// @deprecated Use safe overload instead.
  FIREBASE_DEPRECATED bool ValidFuture(const FutureHandle& handle) const {
    return ValidFuture(handle.id());
  }

  template <typename T>
  bool ValidFuture(SafeFutureHandle<T> handle) const {
    return ValidFuture(handle.get().id());
  }

  bool ValidFuture(FutureHandleId id) const;

#if defined(INTERNAL_EXPERIMENTAL)
  /// Returns a proxy to the last result for `fn_idx`.
  FutureBase LastResultProxy(int fn_idx);
#endif  // defined(INTERNAL_EXPERIMENTAL)

  /// Return internally-held future to the last result for `fn_idx`.
  const FutureBase& LastResult(int fn_idx) const {
    return last_results_[fn_idx];
  }

  /// The Future for `LastResult(fn_idx)` will return kFutureStatusInvalid.
  void InvalidateLastResult(int fn_idx) {
    last_results_[fn_idx] = FutureBase();
  }

  /// The synchronization mutex, for data that's accessed in both in and out
  /// of callbacks.
  Mutex& mutex();

  /// Get the number of LastResult functions.
  size_t GetLastResultCount() const { return last_results_.size(); }

  /// Check if it's safe to delete this API. True if no futures are Pending and
  /// no completion callbacks are running.
  bool IsSafeToDelete() const;

  /// Returns whether this API is currently running a callback.
  bool IsRunningCallback() const;

  /// Check if any Future is being referenced by something other than
  /// last_results_.
  bool IsReferencedExternally() const;

  /// Sets temporary context data associated with a FutureHandle that will be
  /// deallocated alongside the FutureBackingData.
  void SetContextData(const FutureHandle& handle, void* context_data,
                      void (*delete_context_data_fn)(void* data_to_delete));

  /// Force reset the ref count and release the handle.
  void ForceReleaseFuture(const FutureHandle& handle);

  /// Returns the underlying ref-counted future core so async workers can
  /// complete futures after releasing locks without touching `this`.
  std::shared_ptr<detail::FutureImplCore> core() const { return core_; }

  /// Completes a future handle (without populating a result value) directly
  /// through a pinned `FutureImplCore`. Safe to call even if the owning
  /// `ReferenceCountedFutureImpl` has already been destroyed.
  static void CompleteCore(const std::shared_ptr<detail::FutureImplCore>& core,
                           const FutureHandle& handle, int error,
                           const char* error_msg = nullptr) {
    if (!core) return;
    CompleteWithPopulator(core, handle, error, error_msg, nullptr, nullptr);
  }

  template <typename T>
  static void CompleteCore(const std::shared_ptr<detail::FutureImplCore>& core,
                           SafeFutureHandle<T> handle, int error,
                           const char* error_msg = nullptr) {
    CompleteCore(core, handle.get(), error, error_msg);
  }

  /// Completes a typed future handle with `result` directly through a pinned
  /// `FutureImplCore`.
  template <typename T>
  static void CompleteCoreWithResult(
      const std::shared_ptr<detail::FutureImplCore>& core,
      SafeFutureHandle<T> handle, int error, const char* error_msg,
      const T& result) {
    if (!core) return;
    struct ResultCtx {
      const T* value;
    } ctx{&result};
    CompleteWithPopulator(
        core, handle.get(), error, error_msg,
        [](void* raw_data, void* raw_ctx) {
          *static_cast<T*>(raw_data) = *static_cast<ResultCtx*>(raw_ctx)->value;
        },
        &ctx);
  }

 private:
  template <typename T>
  static void DeleteT(void* ptr_to_delete) {
    delete static_cast<T*>(ptr_to_delete);
  }

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

  static void CompleteWithPopulator(
      const std::shared_ptr<detail::FutureImplCore>& core,
      const FutureHandle& handle, int error, const char* error_msg,
      void (*populate_fn)(void* raw_data, void* ctx), void* ctx);

  template <typename T, typename F>
  void CompleteInternal(const FutureHandle& handle, int error,
                        const char* error_msg, const F& populate_data_fn) {
    // Pin `core_` locally so that even if a completion callback destroys `this`
    // (or the owning AppState), the completion machinery never touches `this`
    // after this line.
    auto core = core_;
    struct PopulatorCtx {
      const F* fn;
    } ctx{&populate_data_fn};
    CompleteWithPopulator(
        core, handle, error, error_msg,
        [](void* raw_data, void* raw_ctx) {
          (*static_cast<PopulatorCtx*>(raw_ctx)->fn)(static_cast<T*>(raw_data));
        },
        &ctx);
  }

  template <typename T>
  void CompleteWithResultInternal(const FutureHandle& handle, int error,
                                  const char* error_msg, const T& result) {
    CompleteInternal<T>(handle, error, error_msg,
                        [&result](T* data) { *data = result; });
  }

  template <typename T>
  void CompleteInternal(const FutureHandle& handle, int error,
                        const char* error_msg) {
    CompleteInternal<void>(handle, error, error_msg, [](void*) {});
  }

  std::shared_ptr<detail::FutureImplCore> core_;
  std::vector<FutureBase> last_results_;
};

/// Specialize the case where the data is void since we don't need to
/// allocate any data.
template <>
inline FutureHandle ReferenceCountedFutureImpl::AllocInternal<void>(
    int fn_idx) {
  return AllocInternal(fn_idx, nullptr, nullptr);
}

template <>
inline FutureHandle ReferenceCountedFutureImpl::Alloc<void>(int fn_idx) {
  return AllocInternal<void>(fn_idx);
}

/// Safe version of Alloc<void>.
template <>
inline SafeFutureHandle<void> ReferenceCountedFutureImpl::SafeAlloc<void>(
    int fn_idx) {
  return SafeFutureHandle<void>(AllocInternal<void>(fn_idx));
}

// Makes a future of the appropriate type given a SafeFutureHandle.
// This helps ensure there is no type mismatch when making Futures.
template <typename T>
Future<T> MakeFuture(ReferenceCountedFutureImpl* api,
                     const SafeFutureHandle<T>& handle) {
  return Future<T>(api, handle.get());
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_
