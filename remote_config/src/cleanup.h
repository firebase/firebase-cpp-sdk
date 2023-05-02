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

namespace firebase {
namespace remote_config {

// T is a RemoteConfig public type that defines a Cleanup method.
// R is almost always RemoteConfigInternal unless one wants something else to
// manage the cleanup process. We define type R to make this CleanupFn
// implementation platform-independent.
template <typename T, typename R>
struct CleanupFn {
  typedef T (*CreateInvalidObjectFn)();
  static CreateInvalidObjectFn create_invalid_object;

  static void Cleanup(void* obj_void) {
    T* obj = reinterpret_cast<T*>(obj_void);
    // T should define a Cleanup() method.
    obj->Cleanup();
  }

  static void Register(T* obj, R* remote_config) {
    if (remote_config) {
      remote_config->cleanup_notifier().RegisterObject(
          obj, CleanupFn<T, R>::Cleanup);
    }
  }

  static void Unregister(T* obj, R* remote_config) {
    if (remote_config) {
      remote_config->cleanup_notifier().UnregisterObject(obj);
    }
  }
};

}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_CLEANUP_H_
