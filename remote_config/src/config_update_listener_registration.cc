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

#include "remote_config/src/include/firebase/remote_config/config_update_listener_registration.h"

namespace firebase {
namespace remote_config {

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    std::function<void()> listener_removal_function)
    : listenerRemovalFunction(listener_removal_function) {}

ConfigUpdateListenerRegistration::~ConfigUpdateListenerRegistration() {}

// TODO: almostmatt, merge this with quan's ios changes. also see about whether to make this no-op after first call
void ConfigUpdateListenerRegistration::Remove() {
  this->listenerRemovalFunction();
}

}  // namespace remote_config
}  // namespace firebase
