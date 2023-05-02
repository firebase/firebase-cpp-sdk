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

namespace firebase {
namespace remote_config {
namespace internal {

class RemoteConfigInternal;

/// @brief Calling Remove stops the listener from receiving
///  config updates and unregisters itself. If remove is called and no other
///  listener registrations remain, the connection to the Remote Config backend
///  is closed. Subsequently calling AddOnConfigUpdate will re-open the
///  connection.
class ConfigUpdateListenerRegistrationInternal {
 public:
  ConfigUpdateListenerRegistrationInternal() = delete;

  /// @brief ConfigUpdateListenerRegistrationInternal constructor that takes in
  /// a function as a parameter. The parameter function connects `Remove` to the
  /// native platform's `Remove` method.
  ConfigUpdateListenerRegistrationInternal(
      RemoteConfigInternal* remote_config,
      std::function<void()> listener_removal_function);

  // Delete the default copy and move constructors to make the ownership more
  // obvious i.e. RemoteConfigInternal owns each instance and forbid anyone else
  // to make copies.
  ConfigUpdateListenerRegistrationInternal(
      const ConfigUpdateListenerRegistrationInternal& another) = delete;
  ConfigUpdateListenerRegistrationInternal(
      ConfigUpdateListenerRegistrationInternal&& another) = delete;

  // So far, there is no use of assignment. So we do not bother to define our
  // own and delete the default one, which does not copy internal data properly.
  ConfigUpdateListenerRegistrationInternal& operator=(
      const ConfigUpdateListenerRegistrationInternal& another) = delete;
  ConfigUpdateListenerRegistrationInternal& operator=(
      ConfigUpdateListenerRegistrationInternal&& another) = delete;

  ~ConfigUpdateListenerRegistrationInternal();

  /// @brief Remove the listener being tracked by this
  /// ConfigUpdateListenerRegistrationInternal. After the initial call,
  /// subsequent calls to Remove have no effect.
  void Remove();

  RemoteConfigInternal* remote_config_internal() { return remote_config_; }

 private:
  /// @brief Reference to the RemoteConfigInternal instance that created this.
  RemoteConfigInternal* remote_config_ = nullptr;  // not owning

  /// @brief Callback to invoke native platform's `Remove`.
  std::function<void()> listener_removal_function_;

  /// @brief Indicates whether or not Remove has been called.
  bool listener_removed_ = false;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_CONFIG_UPDATE_LISTENER_REGISTRATION_INTERNAL_H_
