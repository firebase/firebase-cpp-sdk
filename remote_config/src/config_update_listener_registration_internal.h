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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_CONFIG_UPDATE_LISTENER_REGISTRATION_INTERNAL_H_
#define FIREBASE_REMOTE_CONFIG_SRC_CONFIG_UPDATE_LISTENER_REGISTRATION_INTERNAL_H_

#include <functional>
#include <mutex>

namespace firebase {
namespace remote_config {
namespace internal {

class RemoteConfigInternal;

class ConfigUpdateListenerRegistrationInternal {
 public:
  ConfigUpdateListenerRegistrationInternal(
      RemoteConfigInternal* remote_config,
      std::function<void()> listener_removal_function);

  ConfigUpdateListenerRegistrationInternal(
      const ConfigUpdateListenerRegistrationInternal&) = delete;
  ConfigUpdateListenerRegistrationInternal& operator=(
      const ConfigUpdateListenerRegistrationInternal&) = delete;

  ~ConfigUpdateListenerRegistrationInternal();

  void Remove();

  RemoteConfigInternal* remote_config() const { return remote_config_; }
  RemoteConfigInternal* remote_config_internal() const {
    return remote_config_;
  }

 private:
  RemoteConfigInternal* remote_config_ = nullptr;
  std::mutex mutex_;
  std::function<void()> listener_removal_function_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_CONFIG_UPDATE_LISTENER_REGISTRATION_INTERNAL_H_
