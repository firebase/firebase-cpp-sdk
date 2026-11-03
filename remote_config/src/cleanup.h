// Copyright 2023 Google LLC
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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_CLEANUP_H_
#define FIREBASE_REMOTE_CONFIG_SRC_CLEANUP_H_

#include "app/src/cleanup_notifier.h"

namespace firebase {
namespace remote_config {
namespace internal {

template <typename T>
class CleanupFn {
 public:
  static void Cleanup(void* obj_void) {
    T* obj = static_cast<T*>(obj_void);
    obj->Cleanup();
  }

  static void Register(T* instance, RemoteConfigInternal* internal);
  static void Unregister(T* instance, RemoteConfigInternal* internal);
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_CLEANUP_H_
