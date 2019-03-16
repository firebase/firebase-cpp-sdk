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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_CLEANUP_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_CLEANUP_H_

namespace firebase {
namespace database {

template <typename T, typename U>
struct CleanupFn {
  typedef T (*CreateInvalidObjectFn)();
  static CreateInvalidObjectFn create_invalid_object;

  static void Cleanup(void* obj_void) {
    T* obj = reinterpret_cast<T*>(obj_void);
    // Overwrite the existing reference with an invalid reference.
    *obj = create_invalid_object();
  }

  static void Register(T* obj, U* internal) {
    if (internal && internal->database_internal()) {
      internal->database_internal()->cleanup().RegisterObject(
          obj, CleanupFn<T, U>::Cleanup);
    }
  }

  static void Unregister(T* obj, U* internal) {
    if (internal && internal->database_internal()) {
      internal->database_internal()->cleanup().UnregisterObject(obj);
    }
  }
};

}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_CLEANUP_H_
