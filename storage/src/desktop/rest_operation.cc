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

#include "storage/src/desktop/rest_operation.h"

#include <memory>

#include "app/rest/transport_curl.h"
#include "app/src/mutex.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/curl_requests.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase {
namespace storage {
namespace internal {

RestOperation::RestOperation(StorageInternal* storage_internal,
                             const StorageReference& storage_reference,
                             rest::Request* request, Notifier* request_notifier,
                             BlockingResponse* response, Listener* listener,
                             FutureHandle handle, Controller* controller_out)
    : storage_internal_(storage_internal),
      request_(request),
      request_notifier_(request_notifier),
      response_(response),
      listener_(nullptr),
      handle_(handle),
      is_complete_(false) {
  // Notify this operation when the response reports progress and clean up if
  // the response completes.
  response_->set_update_callback(
      [](Notifier::UpdateCallbackType update_type, void* data) {
        RestOperation* operation = reinterpret_cast<RestOperation*>(data);
        switch (update_type) {
          case Notifier::kUpdateCallbackTypeProgress:
            operation->NotifyListenerOfProgress();
            break;
          case Notifier::kUpdateCallbackTypeComplete:
          case Notifier::kUpdateCallbackTypeFailed:
            // The caller will set the response to completed which will allow
            // StorageInternal::CleanupOperations() to delete this operation.
            operation->is_complete_ = true;
            break;
        }
      },
      this);
  request_notifier_->set_update_callback(
      [](Notifier::UpdateCallbackType update_type, void* data) {
        RestOperation* operation = reinterpret_cast<RestOperation*>(data);
        if (update_type == Notifier::kUpdateCallbackTypeProgress) {
          operation->NotifyListenerOfProgress();
        }
      },
      this);

  set_listener(listener);

  transport_.set_is_async(true);
  // Acquire the mutex to prevent operation from being completed before
  // construction is finished.
  MutexLock lock(mutex_);
  transport_.Perform(*request_, response_.get(), &rest_controller_);

  // rest::TransportCurl owns the rest::Controller pointer so as long as this
  // object is alive and rest::Controller is valid.
  controller_.internal_->Initialize(storage_reference, this);
  storage_internal_->cleanup().RegisterObject(this, [](void* operation) {
    delete reinterpret_cast<RestOperation*>(operation);
  });
  storage_internal->AddOperation(this);
  if (controller_out) *controller_out = controller_;
}

RestOperation::~RestOperation() {
  MutexLock lock(mutex_);
  // Clear the update callback to avoid deleting the operation while it's being
  // deleted.
  response_->set_update_callback(nullptr, nullptr);
  request_notifier_->set_update_callback(nullptr, nullptr);
  rest_controller_->Cancel();
  cleanup().CleanupAll();
  storage_internal_->cleanup().UnregisterObject(this);
  storage_internal_->RemoveOperation(this);
  set_listener(nullptr);
}

bool RestOperation::Pause() {
  MutexLock lock(mutex_);
  bool paused = rest_controller_->Pause();
  if (paused && listener_) {
    listener_->OnPaused(&controller_);
  }
  return paused;
}

bool RestOperation::Resume() {
  MutexLock lock(mutex_);
  return rest_controller_->Resume();
}

bool RestOperation::Cancel() {
  MutexLock lock(mutex_);
  return rest_controller_->Cancel();
}

bool RestOperation::is_paused() const {
  MutexLock lock(mutex_);
  return rest_controller_->IsPaused();
}

int64_t RestOperation::bytes_transferred() const {
  MutexLock lock(mutex_);
  return rest_controller_->BytesTransferred();
}

int64_t RestOperation::total_byte_count() const {
  MutexLock lock(mutex_);
  return rest_controller_->TransferSize();
}

// Whether this operation is complete and can be deleted.
bool RestOperation::is_complete() const {
  MutexLock lock(mutex_);
  return is_complete_;
}

// Notify the listener of progress.
void RestOperation::NotifyListenerOfProgress() {
  MutexLock lock(mutex_);
  if (listener_) listener_->impl_->NotifyProgress(&controller_);
}

void RestOperation::set_listener(Listener* listener) {
  if (listener) listener->impl_->set_rest_operation(this);
  MutexLock lock(mutex_);
  if (listener_) listener_->impl_->set_rest_operation(nullptr);
  listener_ = listener;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
