// Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_SRC_JNI_GLOBAL_H_
#define FIREBASE_APP_SRC_JNI_GLOBAL_H_

#include "app/src/jni/ownership.h"

#include <type_traits>

namespace firebase {
namespace jni {

template <typename T, typename std::enable_if_t<std::is_pointer_v<T>, int> = 0>
inline GlobalRef<T> MakeGlobalRef(JNIEnv* env, T object,
                                  bool adopt_existing_global = false) noexcept {
  return GlobalRef<T>(env, object, adopt_existing_global);
}

inline void FlushDeletedGlobalRefs(JNIEnv* env) noexcept {
  internal::GlobalRefReaper::Flush(env);
}

}  // namespace jni
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_JNI_GLOBAL_H_
