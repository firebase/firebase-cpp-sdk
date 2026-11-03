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

#include "firebase/internal/common.h"

namespace firebase {
namespace remote_config {

template <typename T, typename R>
struct CleanupFn;

namespace internal {

class ConfigUpdateListenerRegistrationInternal;
class RemoteConfigInternal;

}  // namespace internal

/// @brief Represents a real-time config update listener registration returned
/// by `RemoteConfig::AddOnConfigUpdateListener()`.
///
/// Call `Remove()` to unregister the listener associated with this
/// registration. Note that the destructor does not automatically remove the
/// listener unless `RemoteConfig` itself is destroyed.
class ConfigUpdateListenerRegistration {
 public:
  /// @brief Creates an invalid `ConfigUpdateListenerRegistration` that is not
  /// associated with any listener.
  ConfigUpdateListenerRegistration();

  /// @brief Copy constructor. Copies of a `ConfigUpdateListenerRegistration`
  /// refer to the same underlying listener registration.
  ///
  /// @param[in] other `ConfigUpdateListenerRegistration` to copy from.
  ConfigUpdateListenerRegistration(
      const ConfigUpdateListenerRegistration& other);

  /// @brief Move constructor. Moves the underlying listener registration.
  ///
  /// @param[in] other `ConfigUpdateListenerRegistration` to move from.
  ConfigUpdateListenerRegistration(
      ConfigUpdateListenerRegistration&& other) noexcept;

  ~ConfigUpdateListenerRegistration();

  /// @brief Copy assignment operator.
  ///
  /// @param[in] other `ConfigUpdateListenerRegistration` to copy from.
  ///
  /// @return Reference to the destination `ConfigUpdateListenerRegistration`.
  ConfigUpdateListenerRegistration& operator=(
      const ConfigUpdateListenerRegistration& other);

  /// @brief Move assignment operator.
  ///
  /// @param[in] other `ConfigUpdateListenerRegistration` to move from.
  ///
  /// @return Reference to the destination `ConfigUpdateListenerRegistration`.
  ConfigUpdateListenerRegistration& operator=(
      ConfigUpdateListenerRegistration&& other) noexcept;

  /// @brief Removes the listener being tracked by this
  /// `ConfigUpdateListenerRegistration`. After the initial call, subsequent
  /// calls to `Remove()` have no effect.
  void Remove();

  /// @brief Returns `true` if this registration refers to an active listener
  /// registration object.
  [[nodiscard]] bool is_valid() const { return internal_ != nullptr; }

  bool operator==(const ConfigUpdateListenerRegistration& other) const {
    return internal_ == other.internal_;
  }

  bool operator!=(const ConfigUpdateListenerRegistration& other) const {
    return !(*this == other);
  }

 private:
  friend class internal::RemoteConfigInternal;
  friend class internal::ConfigUpdateListenerRegistrationInternal;
  template <typename T, typename R>
  friend struct ::firebase::remote_config::CleanupFn;

  explicit ConfigUpdateListenerRegistration(
      internal::ConfigUpdateListenerRegistrationInternal* internal);

  void Cleanup();
  static void CleanupConfigUpdateListenerRegistration(void* registration);

  internal::RemoteConfigInternal* remote_config_ = nullptr;
  internal::ConfigUpdateListenerRegistrationInternal* internal_ = nullptr;
};

}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_CONFIG_UPDATE_LISTENER_REGISTRATION_H_
