// Copyright 2016 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LISTENER_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LISTENER_H_

#include "firebase/storage/controller.h"

namespace firebase {
namespace storage {

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class ControllerInternal;
class ListenerInternal;
class StorageReferenceInternal;
class RestOperation;
}  // namespace internal
/// @endcond

/// @brief Base class used to receive pause and progress events on a running
/// read or write operation.
///
/// Subclasses of this listener class can be used to receive events about data
/// transfer progress. Attach the listener to a transfer operation using
/// StorageReference::GetBytes(), StorageReference::GetFile(),
/// StorageReference::PutBytes(), and StorageReference::PutFile(); then
/// OnPaused() will be called whenever the read or write operation is paused,
/// and OnProgress() will be called periodically as the transfer makes progress.
///
/// @note Callbacks may be invoked on background worker threads or the platform
/// main thread depending on the platform. Destroying a Listener automatically
/// detaches it from any ongoing transfer operations. Subclasses whose
/// OnPaused() or OnProgress() implementations access derived member state
/// should call Detach() at the beginning of the subclass destructor before
/// derived members are destroyed.
class Listener {
 public:
  /// @brief Constructor.
  Listener();

  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  Listener(Listener&&) = delete;
  Listener& operator=(Listener&&) = delete;

  /// @brief Virtual destructor.
  virtual ~Listener();

  /// @brief Detaches this listener from any ongoing transfer operations.
  ///
  /// Derived classes whose callbacks access subclass member state should call
  /// Detach() at the beginning of their destructor before derived members are
  /// destroyed. ~Listener() also calls Detach() automatically.
  void Detach();

  /// @brief The operation was paused.
  ///
  /// @param[in] controller A controller that can be used to check the status
  /// and make changes to the ongoing operation.
  virtual void OnPaused(Controller* /*controller*/) {}

  /// @brief There has been a progress event.
  ///
  /// @param[in] controller A controller that can be used to check the status
  /// and make changes to the ongoing operation.
  virtual void OnProgress(Controller* controller) = 0;

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class internal::ControllerInternal;
  friend class internal::ListenerInternal;
  friend class internal::StorageReferenceInternal;
  friend class internal::RestOperation;

  // Platform specific data.
  internal::ListenerInternal* impl_ = nullptr;
  /// @endcond
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LISTENER_H_
