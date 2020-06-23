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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_

#include <functional>
#include <map>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/mutex.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

#ifdef USE_PLAYBILLING_FUTURE
#include "playbillingclient/future.h"
#else
#include "app/src/include/firebase/future.h"
#endif

namespace FIREBASE_NAMESPACE {

// FutureBackingData holds the important data for each Future. These are held by
// ReferenceCountedFutureImpl and indexed by FutureHandleId.
struct FutureBackingData;

// Value for an invalid future handle. Default futures (which don't reference
// any real operation) have this handle ID.
const FutureHandleId kInvalidFutureHandle = 0;

/// @brief Type-safe future handle.
///
/// Trying to complete a SafeFutureHandle<T> with an incompatible result type
/// won't compile instead of leading to undefined behavior, e.g.:
///
/// @code{.cpp}
///   SafeFutureHandle<int> handle = api.SafeAlloc<int>();
///   std::string value("hello");
///   api.CompleteWithResult(handle, kSuccess, value); // doesn't compile.
/// @endcode
///
/// Compare to:
///
/// @code{.cpp}
///   FutureHandle handle = api.Alloc<int>();
///   std::string value("hello");
///   api.CompleteWithResult(handle, kSuccess, value); // SEGFAULT.
/// @endcode
template <typename T>
class SafeFutureHandle {
 public:
  inline SafeFutureHandle() : handle_(kInvalidFutureHandle) {}
  inline explicit SafeFutureHandle(const FutureHandle& handle)
      : handle_(handle) {}
  inline const FutureHandle& get() const { return handle_; }
  // See FutureHandle::Detach.
  void Detach() { handle_.Detach(); }

  static const SafeFutureHandle kInvalidHandle;

 private:
  FutureHandle handle_;
};

// Type-safe version of kInvalidFutureHandle.
template <typename T>
const SafeFutureHandle<T> SafeFutureHandle<T>::kInvalidHandle;

/// @brief Backing class for Futures that allows a Future to have multiple
///        copies. When no copies remain, the Future is invalidated.
///
/// To use this class, create an instance of it, and then call @ref Alloc
/// whenever you want to create a Future. Alloc returns a FutureHandle from
/// which you can create a Future to return to the user with,
///     `Future(&ref_counted_future_impl, handle)`
///
/// After the asynchronous call has completed call @ref Complete. If the
/// supplied FutureHandle is still valid (i.e. at least one Future references
/// it), then the Future's error will be set, the Future's data will be set via
/// a lambda, and the Future will be marked as Completed.
///
/// As an optional convenience, this class also stores the last Future for a
/// user-defined index. The index will most likely be an enum for all the APIs
/// in a user's library. It's nice to keep around the last Future so that the
/// client of the user's library doesn't have to.
///
// TODO(b/65282379): Migrate all uses of the class to SafeAlloc and remove
// unsafe methods.
class ReferenceCountedFutureImpl : public detail::FutureApiInterface {
 public:
  /// This handle will never be returned by @ref Alloc, so you can use it
  /// to signify an uninitialized or invalid value.
  static const FutureHandle kInvalidHandle;

  /// Returned by @ref GetFutureError when the passed in handle is invalid.
  static constexpr int kErrorFutureIsNoLongerValid = -1;

  /// Returned by @ref GetFutureErrorMessage when the passed in handle is
  /// invalid.
  static const char kErrorMessageFutureIsNoLongerValid[];

  /// Pass into @ref Alloc for `fn_idx` when you don't want to update any
  /// function.
  static constexpr int kNoFunctionIndex = -1;

  explicit ReferenceCountedFutureImpl(size_t last_result_count)
      : next_future_handle_(kInvalidFutureHandle + 1),
        last_results_(last_result_count) {}
  ~ReferenceCountedFutureImpl() override;

  // Implementation of detail::FutureApiInterface.
  void ReferenceFuture(const FutureHandle& handle) override;
  void ReleaseFuture(const FutureHandle& handle) override;
  FutureStatus GetFutureStatus(const FutureHandle& handle) const override;
  int GetFutureError(const FutureHandle& handle) const override;
  const char* GetFutureErrorMessage(const FutureHandle& handle) const override;
  const void* GetFutureResult(const FutureHandle& handle) const override;

  // Add a callback to run when the Future is completed. If user_data requires
  // some form of deletion after the callback is executed (or is removed), you
  // can specify the deletion function as well.
  detail::CompletionCallbackHandle AddCompletionCallback(
      const FutureHandle& handle, FutureBase::CompletionCallback callback,
      void* user_data, void (*user_data_delete_fn_ptr)(void*),
      bool single_completion) override;

  // Remove a callback that was previously registered via AddCompletionCallback.
  // If it has a user data deletion function it will be called.
  void RemoveCompletionCallback(
      const FutureHandle& handle,
      detail::CompletionCallbackHandle callback_handle) override;
#ifdef FIREBASE_USE_STD_FUNCTION
  // std::function version of AddCompletionCallback, which supports lambda
  // capture.
  detail::CompletionCallbackHandle AddCompletionCallbackLambda(
      const FutureHandle& handle,
      std::function<void(const FutureBase&)> callback,
      bool single_completion) override;
#endif  // FIREBASE_USE_STD_FUNCTION
  // When a Future instance is created, register it with the cleanup manager so
  // it will be properly invalidated upon destruction of
  // ReferenceCountedFutureImpl.
  void RegisterFutureForCleanup(FutureBase* future) override;

  // When a Future instance is deleted, unregister it from the cleanup manager.
  void UnregisterFutureForCleanup(FutureBase* future) override;

  /// Allocate backing data for a Future with result of type T.
  /// The initial value of T is specified in `initial_data`.
  /// For this overload, it is nonsensical for T to be void.
  ///
  /// If fn_idx is kNoFunctionIndex, the initial reference count of the
  /// FutureHandle will be 0. Every Future that is created will increment the
  /// reference count, but if no Futures are created, the backing data will
  /// not be deleted until this ReferenceCountedFutureImpl class is destroyed.
  /// Therefore, if you use kNoFunctionIndex, be sure to create at least one
  /// Future with the returned FutureHandle.
  ///
  /// If `fn_idx` is specified, we update the internal Future at index fn_idx
  /// to refer to the newly allocated FutureHandle. To access this Future,
  /// call @ref LastResult(fn_idx). To eschew this optional feature, specify
  /// `kNoFunctionIndex` for `fn_idx`.
  ///
  /// @code{.cpp}
  ///   const FutureHandle handle =
  ///       future_impl.Alloc<DoSomethingResult>(
  ///           kDoSomethingFnIdx, DoSomethingResult(kDefaultResultValue));
  /// @endcode
  ///
  /// @deprecated use SafeAlloc instead.
  ///
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc(int fn_idx, const T& initial_data) {
    return AllocInternal(fn_idx, new T(initial_data), DeleteT<T>);
  }

  /// Safe version of Alloc.
  template <typename T>
  SafeFutureHandle<T> SafeAlloc(int fn_idx, const T& initial_data) {
    return SafeFutureHandle<T>(AllocInternal(fn_idx, initial_data));
  }

  /// Same as above but use default constructor for data.
  /// Use this overload when T is of type void.
  ///
  /// @code{.cpp}
  ///   const FutureHandle handle =
  ///       future_impl.Alloc<void>(kDoSomethingVoidResultFnIdx);
  /// @endcode
  ///
  /// @deprecated use SafeAlloc instead.
  ///
  template <typename T>
  FIREBASE_DEPRECATED FutureHandle Alloc(int fn_idx) {
    return AllocInternal(fn_idx, new T, DeleteT<T>);
  }

  /// Safe version of Alloc.
  template <typename T>
  SafeFutureHandle<T> SafeAlloc(int fn_idx) {
    return SafeFutureHandle<T>(AllocInternal<T>(fn_idx));
  }

  /// Same as above but don't record a Future in the @ref LastResult array.
  ///
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

  /// Call when the asynchronous process completes.
  /// Marks the Future as complete and calls the completion callback, if one is
  /// registered.
  /// The Future's result data is generated by the `populate_data_fn`, if one
  /// is supplied.
  ///
  /// @code{.cpp}
  ///   future_impl.Complete<DoSomethingResult>(
  ///       handle_from_alloc, kSuccess, nullptr,
  ///       [](DoSomethingResult* data) { data->value = 1; });
  /// @endcode
  ///
  /// @deprecated use safe overload instead.
  ///
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

  /// Same as above, but with no error message.
  ///
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

  /// Same as above but pass-in the result data instead of populating with a
  /// lambda. Handy when the result type is very simple.
  ///
  /// @code{.cpp}
  ///   DoSomethingResult result;
  ///   result.value = 1;
  ///   future_impl.CompleteWithResult<DoSomethingResult>(
  ///       handle_from_alloc, kSuccess, nullptr, result);
  /// @endcode
  ///
  /// @deprecated use safe overload instead.
  ///
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

  /// Same as above, but with no error message.
  ///
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

  /// Same as above but don't set the Future's result data.
  ///
  /// @code{.cpp}
  ///   future_impl.Complete(handle_from_alloc, kSuccess);
  /// @endcode
  ///
  /// @deprecated use safe overload instead.
  ///
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error,
                                    const char* error_msg) {
    CompleteInternal<void>(handle, error, error_msg);
  }

  /// Safe overload of Complete.
  template <typename T>
  void Complete(SafeFutureHandle<T> handle, int error, const char* error_msg) {
    CompleteInternal<T>(handle.get(), error, error_msg);
  }

  /// Same as above, but with no error message.
  ///
  /// @deprecated use safe overload instead.
  FIREBASE_DEPRECATED void Complete(const FutureHandle& handle, int error) {
    CompleteInternal<void>(handle, error, nullptr);
  }

  /// Safe overload of Complete.
  template <typename T>
  void Complete(SafeFutureHandle<T> handle, int error) {
    CompleteInternal<T>(handle.get(), error, nullptr);
  }

  /// Return true if at least one extant Future still holds a reference to
  /// `handle`. Return false if this handle is no longer (or was never)
  /// reference by any Futures.
  ///
  /// @deprecated Use safe overload instead.
  FIREBASE_DEPRECATED bool ValidFuture(const FutureHandle& handle) const {
    return BackingFromHandle(handle.id()) != nullptr;
  }

  /// Return true if at least one extant Future still holds a reference to
  /// `handle`. Return false if this handle is no longer (or was never)
  /// reference by any Futures.
  template <typename T>
  bool ValidFuture(SafeFutureHandle<T> handle) const {
    return BackingFromHandle(handle.get().id()) != nullptr;
  }

  /// Return true if at least one extant Future still holds a reference to
  /// this handle ID. Return false if this handle is no longer (or was never)
  /// reference by any Futures or FutureHandles.
  bool ValidFuture(FutureHandleId id) const {
    return BackingFromHandle(id) != nullptr;
  }

#if defined(INTERNAL_EXPERIMENTAL)
  /// Returns a proxy to the last result for `fn_idx`.
  FutureBase LastResultProxy(int fn_idx);
#endif  // defined(INTERNAL_EXPERIMENTAL)

  /// Return internally-held future to the last result for `fn_idx`.
  const FutureBase& LastResult(int fn_idx) const {
    MutexLock lock(mutex_);
    return last_results_[fn_idx];
  }

  /// The Future for `LastResult(fn_idx)` will return kFutureStatusInvalid.
  void InvalidateLastResult(int fn_idx) {
    MutexLock lock(mutex_);
    last_results_[fn_idx] = FutureBase();
  }

  /// The synchronization mutex, for data that's accessed in both in and out
  /// of callbacks.
  Mutex& mutex() { return mutex_; }

  /// Get the number of LastResult functions.
  size_t GetLastResultCount() { return last_results_.size(); }

  /// Check if it's safe to delete this API. It's only safe to delete this if
  /// no futures are Pending.
  bool IsSafeToDelete() const;

  /// Check if the Future is being referenced by something other than
  /// last_results_.
  bool IsReferencedExternally() const;

  /// Sets temporary context data associated with a FutureHandle that will be
  /// deallocated alongside the FutureBackingData. This will occur when there
  /// are no more Futures referencing it.
  void SetContextData(const FutureHandle& handle, void* context_data,
                      void (*delete_context_data_fn)(void* data_to_delete));

  /// CleanupNotifier will invalidate any stale Future instances that
  /// are held by outside code, when this is deleted.
  TypedCleanupNotifier<FutureBase>& cleanup() { return cleanup_; }
  TypedCleanupNotifier<FutureHandle>& cleanup_handles() {
    return cleanup_handles_;
  }

  /// Force reset the ref count and release the handle.
  void ForceReleaseFuture(const FutureHandle& handle);

 private:
  template <typename T>
  static void DeleteT(void* ptr_to_delete) {
    delete static_cast<T*>(ptr_to_delete);
  }

  /// Allocate a new handle. It's unlikely that we'll ever allocate four
  /// billion of these to loop back to the start, but just in case, skip over
  /// the one marked as kInvalidFutureHandle.
  FutureHandleId AllocHandleId() {
    const FutureHandleId id = next_future_handle_++;
    if (next_future_handle_ == kInvalidFutureHandle) next_future_handle_++;
    return id;
  }

  /// Return the backing data for the previously allocated `handle`, if it
  /// is still valid, or nullptr otherwise.
  /// The backing data is an internal object that holds the reference count,
  /// result data, completion callback, etc., for the Future.
  /// The backing data gets deleted when no Futures refer to it, i.e. when its
  /// reference count goes to zero.
  const FutureBackingData* BackingFromHandle(FutureHandleId id) const {
    return const_cast<ReferenceCountedFutureImpl*>(this)->BackingFromHandle(id);
  }
  FutureBackingData* BackingFromHandle(FutureHandleId id);

  /// Allocate backing data for a Future and assign it a unique handle,
  /// which is returned. The most recent Future for `fn_idx` is updated to
  /// be this newly created Future.
  /// The result data for the future is passed in as `data`, and a function to
  /// delete `data` is also passed in (required since `data` can be any type).
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

  /// Return the data for the backing. Requires a function since
  /// FutureBackingData is only defined in the header, but the data is
  /// accessed in template class @ref Complete.
  void* BackingData(FutureBackingData* backing);

  /// Set the error value that will be returned by the Future for `backing`.
  void SetBackingError(FutureBackingData* backing, int error,
                       const char* error_msg);

  /// Complete the proxies of the Future for `backing`.
  void CompleteProxy(FutureBackingData* backing);

  /// Mark the status as complete.
  /// This assumes that mutex_ has been locked via Acquire(), and calls
  /// Release() prior to calling the callback.
  void CompleteHandle(const FutureHandle& handle);

  // See Complete() methods.
  template <typename T, typename F>
  void CompleteInternal(const FutureHandle& handle, int error,
                        const char* error_msg, const F& populate_data_fn) {
    // We don't want to call to the user defined callback with the lock held,
    // so acquire the lock directly, and have it released prior to calling the
    // callback in CompleteHandle.
    mutex_.Acquire();

    // Ensure backing data is still around. It may have been removed after all
    // Futures that refer to it disappeared.
    FutureBackingData* backing = BackingFromHandle(handle.id());
    if (backing == nullptr) {
      mutex_.Release();
      return;
    }

    // Don't allow Complete to be called on a future that is already completed.
    FIREBASE_ASSERT(GetFutureStatus(handle) == kFutureStatusPending);

    // Set the error before populating the data, in case the populate function
    // wants to query the error.
    SetBackingError(backing, error, error_msg);

    // Populate the data. F is a lambda that accepts a data pointer of type T.
    populate_data_fn(static_cast<T*>(BackingData(backing)));

    // Mark the status as complete.
    CompleteHandle(handle);

    // Complete proxied futures.
    CompleteProxy(backing);

    // Call callbacks, if any were registered, releasing the mutex that
    // was previously acquired in any case.
    ReleaseMutexAndRunCallbacks(handle);
  }

  // See CompleteWithResult.
  template <typename T>
  void CompleteWithResultInternal(const FutureHandle& handle, int error,
                                  const char* error_msg, const T& result) {
    CompleteInternal<T>(handle, error, error_msg,
                        [result](T* data) { *data = result; });
  }

  // See Complete.
  template <typename T>
  void CompleteInternal(const FutureHandle& handle, int error,
                        const char* error_msg) {
    // Call templated Complete() with an empty lambda. The syntax is alarming,
    // but the empty lambda just turns `populate_data_fn` into a no-op.
    CompleteInternal<void>(handle, error, error_msg, [](void*) {});
  }

  /// Releases the mutex, calling the Future's completion callbacks, if any.
  /// (The mutex is released before calling the callbacks.)
  void ReleaseMutexAndRunCallbacks(const FutureHandle& handle);

  void RunCallback(FutureBase* future_base,
                   FutureBase::CompletionCallback callback, void* user_data);

  /// Mutex protecting all asynchronous data operations.
  /// Marked as `mutable` so that const functions can still be protected.
  mutable Mutex mutex_;

  /// Hold backing data for all Futures.
  /// Indexed by the FutureHandle, which is a unique integer used by the
  /// Future to access the backing data. The backing data is deleted once no
  /// more Futures reference it.
  // TODO(jsanmiya): Use unordered_map when available (i.e. when not stlport).
  std::map<FutureHandleId, FutureBackingData*> backings_;

  /// A unique int that is incremented by one after every call to @ref Alloc.
  FutureHandleId next_future_handle_;

  /// Optionally keep a future around for the most recent call to a function.
  /// The functions are specified in `fn_idx` of @ref Alloc.
  std::vector<FutureBase> last_results_;

  /// Clean up any stale Future instances.
  TypedCleanupNotifier<FutureBase> cleanup_;

  /// Clean up any stale FutureHandle instances.
  TypedCleanupNotifier<FutureHandle> cleanup_handles_;

  /// True while running the user-supplied callback upon a future's completion.
  /// This flag prevents this instance from being considered safe to delete
  /// before the callback is finished, which would be unsafe because it would
  /// clean up the future that is passed to the callback.
  bool is_running_callback_ = false;
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
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_REFERENCE_COUNTED_FUTURE_IMPL_H_
