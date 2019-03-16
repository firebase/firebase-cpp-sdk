// Copyright 2017 Google LLC
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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_CONTROLLER_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_CONTROLLER_DESKTOP_H_

#include <stdint.h>

#include "app/memory/unique_ptr.h"
#include "app/rest/controller_curl.h"
#include "app/rest/request_binary.h"
#include "app/rest/response_binary.h"
#include "app/rest/transport_builder.h"
#include "app/src/mutex.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

class RestOperation;

class ControllerInternal {
 public:
  ControllerInternal();

  ~ControllerInternal();

  ControllerInternal(const ControllerInternal& other);

  ControllerInternal& operator=(const ControllerInternal& other);

  // Pauses the operation currently in progress.
  bool Pause();

  // Resumes the operation that is paused.
  bool Resume();

  // Cancels the operation currently in progress.
  bool Cancel();

  // Returns true if the operation is paused.
  bool is_paused() const;

  // Returns the number of bytes transferred so far.
  int64_t bytes_transferred();

  // Returns the total bytes to be transferred.
  int64_t total_byte_count();

  // Returns the StorageReference associated with this Controller.
  StorageReferenceInternal* GetReference() const;

  // Returns if a controllerInternal is valid and ready to be used.
  bool is_valid();

  // Initialization.  Done outside the constructor, because the object is owned
  // (and constructed) by the caller, but needs to be initialized by the
  // storage library.
  // reference is copied by this class and operation is referenced.
  // It's safe for operation to be deleted before this class as this object
  // registers for cleanup on RestOperation::cleanup().
  void Initialize(const StorageReference& reference, RestOperation* operation);

 private:
  // Update the internal state from the operation optionally returning
  // the current transfer state.
  void UpdateFromOperation(int64_t* transferred, int64_t* total);

  // Remove rest operation reference from this controller.
  static void RemoveRestOperationReference(void* object);

 private:
  // Guards reference_ and operation_.
  mutable Mutex mutex_;
  StorageReference reference_;
  RestOperation* operation_;
  int64_t bytes_transferred_;
  int64_t total_byte_count_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_CONTROLLER_DESKTOP_H_
