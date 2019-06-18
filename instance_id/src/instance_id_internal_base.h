// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_INSTANCE_ID_INTERNAL_BASE_H_
#define FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_INSTANCE_ID_INTERNAL_BASE_H_

#include <map>

#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "instance_id/src/include/firebase/instance_id.h"

namespace firebase {
namespace instance_id {
namespace internal {

// Common functionality for platform implementations of InstanceIdInternal.
class InstanceIdInternalBase {
 public:
  // Enumeration for API functions that return a Future.
  // This allows us to hold a Future for the most recent call to that API.
  enum ApiFunction {
    kApiFunctionGetId = 0,
    kApiFunctionDeleteId,
    kApiFunctionGetToken,
    kApiFunctionDeleteToken,
    kApiFunctionMax,
  };

 public:
  InstanceIdInternalBase();
  ~InstanceIdInternalBase();

  // Allocate a future handle for the specified function.
  template <typename T>
  SafeFutureHandle<T> FutureAlloc(ApiFunction function_index) {
    return future_api_.SafeAlloc<T>(function_index);
  }

  // Get the future API implementation.
  ReferenceCountedFutureImpl &future_api() { return future_api_; }

  // Associate an InstanceId instance with an app.
  static void RegisterInstanceIdForApp(App *app, InstanceId *instance_id);
  // Remove association of InstanceId instance with an App.
  static void UnregisterInstanceIdForApp(App *app, InstanceId *instance_id);
  // Find an InstanceId instance associated with an app.
  static InstanceId *FindInstanceIdByApp(App *app);

  // Return the mutex to make sure both find and register are guarded.
  static Mutex& mutex() { return instance_id_by_app_mutex_; }

 private:
  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_api_;
  /// Identifier used to track futures associated with future_impl.
  std::string future_api_id_;

  static std::map<App *, InstanceId *> instance_id_by_app_;
  static Mutex instance_id_by_app_mutex_;
};

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase

#endif  // FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_INSTANCE_ID_INTERNAL_BASE_H_
