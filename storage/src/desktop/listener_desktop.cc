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

#include "storage/src/desktop/listener_desktop.h"

#include <stdint.h>

#include "app/src/mutex.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase {
namespace storage {
namespace internal {

ListenerInternal::ListenerInternal(Listener *listener) :
    listener_(listener), bytes_transferred_(-1), total_byte_count_(-1) {}

ListenerInternal::~ListenerInternal() {
  MutexLock lock(mutex_);
  if (rest_operation_) {
    rest_operation_->cleanup().UnregisterObject(this);
    rest_operation_->set_listener(nullptr);
  }
}

void ListenerInternal::NotifyProgress(Controller* controller) {
  int64_t new_bytes_transferred = controller->bytes_transferred();
  int64_t new_total_byte_count = controller->total_byte_count();
  if (new_bytes_transferred != bytes_transferred_ ||
      new_total_byte_count != total_byte_count_) {
    bytes_transferred_ = new_bytes_transferred;
    total_byte_count_ = new_total_byte_count;
    if (listener_) listener_->OnProgress(controller);
  }
}

void ListenerInternal::set_rest_operation(RestOperation *operation) {
  MutexLock lock(mutex_);
  rest_operation_ = operation;
  if (rest_operation_) {
    // Remove the reference to the rest operation from this listener when the
    // operation is destroyed.  Since this is called when rest_operation_ is
    // destroyed we know the operation will no longer reference this listener.
    rest_operation_->cleanup().RegisterObject(this, [](void *listener) {
      reinterpret_cast<ListenerInternal *>(listener)->set_rest_operation(
          nullptr);
      // Do not call rest_operation->set_listener() here as it can lead to
      // deadlock between the mutex owned by the listener and the operation's
      // mutex.
    });
  }
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
