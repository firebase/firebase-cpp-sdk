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

#include "remote_config/src/config_update_listener_registration_internal.h"

#include <functional>
#include <mutex>
#include <utility>

namespace firebase::remote_config::internal {

ConfigUpdateListenerRegistrationInternal::
    ConfigUpdateListenerRegistrationInternal(
        RemoteConfigInternal* remote_config,
        std::function<void()> listener_removal_function)
    : remote_config_(remote_config),
      listener_removal_function_(std::move(listener_removal_function)) {}

ConfigUpdateListenerRegistrationInternal::
    ~ConfigUpdateListenerRegistrationInternal() {
  Remove();
}

void ConfigUpdateListenerRegistrationInternal::Remove() {
  std::function<void()> fn;
  {
    std::scoped_lock lock(mutex_);
    fn = std::move(listener_removal_function_);
    listener_removal_function_ = nullptr;
  }
  if (fn) {
    fn();
  }
}

}  // namespace firebase::remote_config::internal
