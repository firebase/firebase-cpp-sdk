/*
 * Copyright 2018 Google LLC
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

#ifndef FIREBASE_ANALYTICS_SRC_ANALYTICS_COMMON_H_
#define FIREBASE_ANALYTICS_SRC_ANALYTICS_COMMON_H_

#include <memory>
#include <mutex>

#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace analytics {
namespace internal {

enum AnalyticsFn {
  kAnalyticsFnGetAnalyticsInstanceId,
  kAnalyticsFnGetSessionId,
  kAnalyticsFnLogAppleTransaction,
  kAnalyticsFnCount,
};

// Data structure which holds the Future API for this module.
class FutureData {
 public:
  FutureData() : api_(kAnalyticsFnCount) {}
  ~FutureData() = default;

  FutureData(const FutureData&) = delete;
  FutureData& operator=(const FutureData&) = delete;
  FutureData(FutureData&&) = delete;
  FutureData& operator=(FutureData&&) = delete;

  ReferenceCountedFutureImpl* api() { return &api_; }

  // Mutex protecting FutureData singleton creation, lookup, and destruction.
  static std::recursive_mutex& mutex();

  // Create FutureData singleton. Callers using the returned raw pointer must
  // hold FutureData::mutex() across its use, or call GetShared().
  static FutureData* Create();
  // Destroy the FutureData singleton.
  static void Destroy();
  // Get the Future data singleton. Callers using the returned raw pointer must
  // hold FutureData::mutex() across its use, or call GetShared().
  static FutureData* Get();
  // Get a shared_ptr to the active FutureData instance for async callbacks.
  static std::shared_ptr<FutureData> GetShared();

 private:
  static std::shared_ptr<FutureData>& FutureDataPtr();

  ReferenceCountedFutureImpl api_;
};

// Shared helper to retrieve the last result of a future by function index.
template <typename T>
Future<T> GetLastResult(AnalyticsFn fn) {
  std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
  if (!IsInitialized()) return Future<T>();
  std::shared_ptr<FutureData> future_data = FutureData::GetShared();
  if (!future_data) return Future<T>();
  return static_cast<const Future<T>&>(future_data->api()->LastResult(fn));
}

template <typename T>
Future<T> CompleteStubFuture(internal::AnalyticsFn fn, T result_val) {
  std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
  if (!IsInitialized()) return Future<T>();
  std::shared_ptr<FutureData> future_data = FutureData::GetShared();
  if (!future_data) return Future<T>();
  auto* api = future_data->api();
  const auto future_handle = api->SafeAlloc<T>(fn);
  api->CompleteWithResult(future_handle, 0, "", result_val);
  return Future<T>(api, future_handle.get());
}

template <>
inline Future<void> CompleteStubFuture<void>(internal::AnalyticsFn fn, void* /*unused*/) {
  std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
  if (!IsInitialized()) return Future<void>();
  std::shared_ptr<FutureData> future_data = FutureData::GetShared();
  if (!future_data) return Future<void>();
  auto* api = future_data->api();
  const auto future_handle = api->SafeAlloc<void>(fn);
  api->Complete(future_handle, 0, "");
  return Future<void>(api, future_handle.get());
}

extern const char* const kAnalyticsModuleName;

// Registers a cleanup task for this module on the specified App instance.
void RegisterTerminateOnAppDestroy(const App* app);

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Unregisters the cleanup task for this module.
void UnregisterTerminateOnAppDestroy();

// Unregisters the cleanup task for this module if auto-initialization is
// disabled.
void UnregisterTerminateOnDefaultAppDestroy();

// Determine whether the analytics module is initialized.
// This is implemented per platform.
bool IsInitialized();

}  // namespace internal
}  // namespace analytics
}  // namespace firebase

#endif  // FIREBASE_ANALYTICS_SRC_ANALYTICS_COMMON_H_
