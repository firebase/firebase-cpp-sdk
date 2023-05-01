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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_CONFIG_UPDATE_LISTENER_REGISTRATION_H_
#define FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_CONFIG_UPDATE_LISTENER_REGISTRATION_H_

#include <functional>

namespace firebase {
namespace remote_config {

namespace internal {
class RemoteConfigInternal;
class ConfigUpdateListenerRegistrationInternal;
}  // namespace internal

/// @brief Calling Remove stops the listener from receiving
///  config updates and unregisters itself. If remove is called and no other
///  listener registrations remain, the connection to the Remote Config backend
///  is closed. Subsequently calling AddOnConfigUpdate will re-open the
///  connection.
class ConfigUpdateListenerRegistration {
 public:
  /**
   * @brief Creates an invalid ConfigUpdateListenerRegistration that has to be
   * reassigned before it can be used.
   *
   * Calling Remove() on an invalid ConfigUpdateListenerRegistration is a no-op.
   */
  ConfigUpdateListenerRegistration();

  /**
   * @brief Copy constructor.
   *
   * `ConfigUpdateListenerRegistration` can be efficiently copied because it
   * simply refers to the same underlying listener. If there is more than one
   * copy of a `ConfigUpdateListenerRegistration`, after calling `Remove` on one
   * of them, the listener is removed, and calling `Remove` on any other copies
   * will be a no-op.
   *
   * @param[in] other `ConfigUpdateListenerRegistration` to copy from.
   */
  ConfigUpdateListenerRegistration(
      const ConfigUpdateListenerRegistration& other);

  /**
   * @brief Move constructor.
   *
   * Moving is more efficient than copying for a
   * `ConfigUpdateListenerRegistration`. After being moved from, a
   * `ConfigUpdateListenerRegistration` is equivalent to its default-constructed
   * state.
   *
   * @param[in] other `ConfigUpdateListenerRegistration` to move data from.
   */
  ConfigUpdateListenerRegistration(ConfigUpdateListenerRegistration&& other);

  virtual ~ConfigUpdateListenerRegistration();

  /**
   * @brief Copy assignment operator.
   *
   * `ConfigUpdateListenerRegistration` can be efficiently copied because it
   * simply refers to the same underlying listener. If there is more than one
   * copy of a `ConfigUpdateListenerRegistration`, after calling `Remove` on one
   * of them, the listener is removed, and calling `Remove` on any other copies
   * will be a no-op.
   *
   * @param[in] other `ConfigUpdateListenerRegistration` to copy from.
   *
   * @return Reference to the destination `ConfigUpdateListenerRegistration`.
   */
  ConfigUpdateListenerRegistration& operator=(
      const ConfigUpdateListenerRegistration& other);

  /**
   * @brief Move assignment operator.
   *
   * Moving is more efficient than copying for a
   * `ConfigUpdateListenerRegistration`. After being moved from, a
   * `ConfigUpdateListenerRegistration` is equivalent to its default-constructed
   * state.
   *
   * @param[in] other `ConfigUpdateListenerRegistration` to move data from.
   *
   * @return Reference to the destination `ConfigUpdateListenerRegistration`.
   */
  ConfigUpdateListenerRegistration& operator=(
      ConfigUpdateListenerRegistration&& other);

  /// @brief Remove the listener being tracked by this
  /// ConfigUpdateListenerRegistration. After the initial call, subsequent calls
  /// to Remove have no effect.
  void Remove();

 private:
  friend class internal::RemoteConfigInternal;
  template <typename T, typename R>
  friend struct CleanupFn;

  explicit ConfigUpdateListenerRegistration(
      internal::ConfigUpdateListenerRegistrationInternal* internal);

  /// @brief Method to cleanup any references to internal data.
  void Cleanup();

  internal::RemoteConfigInternal* remote_config_ = nullptr;
  mutable internal::ConfigUpdateListenerRegistrationInternal* internal_ =
      nullptr;
};

}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_CONFIG_UPDATE_LISTENER_REGISTRATION_H_
