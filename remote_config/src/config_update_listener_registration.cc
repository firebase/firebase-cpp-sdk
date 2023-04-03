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

#include "remote_config/src/config_update_listener_registration.h"

namespace firebase {
namespace remote_config {

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener,
    RemoteConfigInternal remote_config_internal) : configUpdateListener(config_update_listener), remoteConfigInternal(remote_config_internal) {}

ConfigUpdateListenerRegistration::~ConfigUpdateListenerRegistration() {}

// Removes the listener being tracked by this ConfigUpdateListenerRegistration.
// After the initial call, subsequent calls to Remove have no effect.
void ConfigUpdateListenerRegistration::Remove() {}

}  // namespace remote_config
}  // namespace firebase
