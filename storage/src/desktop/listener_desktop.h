// Copyright 2018 Google LLC
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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LISTENER_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LISTENER_DESKTOP_H_

#include <stdint.h>

#include "app/src/mutex.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase {
namespace storage {
namespace internal {

class ListenerInternal {
 public:
  explicit ListenerInternal(Listener* listener);
  ~ListenerInternal();

  // Notify the listener associated with this class with a progress update.
  // This will debounce updates if the controller doesn't report progress.
  void NotifyProgress(Controller* controller);

  // Attach this listener to the specified rest operation.
  // If the rest operation is destroyed it will remove a reference to this
  // listener.
  void set_rest_operation(RestOperation* operation);

 private:
  Mutex mutex_;
  Listener* listener_;
  // Guards rest_operation_.
  RestOperation* rest_operation_;
  // Used to debounce the listener.
  int64_t bytes_transferred_;
  int64_t total_byte_count_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LISTENER_DESKTOP_H_
