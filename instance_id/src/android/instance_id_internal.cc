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

#include "instance_id/src/android/instance_id_internal.h"

#include <assert.h>
#include <jni.h>

#include "app/memory/shared_ptr.h"
#include "app/src/mutex.h"

namespace firebase {
namespace instance_id {
namespace internal {

const char* InstanceIdInternal::kCancelledError = "Cancelled";

InstanceIdInternal::InstanceIdInternal()
    : instance_id_(nullptr), java_instance_id_(nullptr) {}

InstanceIdInternal::~InstanceIdInternal() {
  CancelOperations();
  Initialize(instance_id_, nullptr);
}

void InstanceIdInternal::Initialize(InstanceId* instance_id,
                                    jobject java_instance_id) {
  instance_id_ = instance_id;
  JNIEnv* env = instance_id_->app().GetJNIEnv();
  if (java_instance_id_) env->DeleteGlobalRef(java_instance_id_);
  java_instance_id_ = env->NewGlobalRef(java_instance_id);
  env->DeleteLocalRef(java_instance_id);
}

// Store a reference to a scheduled operation.
SharedPtr<AsyncOperation>* InstanceIdInternal::AddOperation(
    AsyncOperation* operation) {
  MutexLock lock(operations_mutex_);
  operations_.push_back(SharedPtr<AsyncOperation>(operation));
  return &operations_[operations_.size() - 1];
}

// Remove a reference to a schedule operation.
void InstanceIdInternal::RemoveOperation(
    const SharedPtr<AsyncOperation>& operation) {
  MutexLock lock(operations_mutex_);
  for (auto it = operations_.begin(); it != operations_.end(); ++it) {
    if (&(**it) == &(*operation)) {
      operations_.erase(it);
      break;
    }
  }
}

// Cancel all scheduled operations.
void InstanceIdInternal::CancelOperations() {
  MutexLock lock(operations_mutex_);
  while (operations_.size()) {
    operations_[0]->Cancel();
  }
}

void InstanceIdInternal::CompleteOperation(
    const SharedPtr<AsyncOperation>& operation, Error error,
    const char* error_message) {
  future_api().Complete(operation->future_handle<void>(), error,
                        error_message ? error_message : "");
  RemoveOperation(operation);
}

void InstanceIdInternal::Canceled(void* function_data) {
    SharedPtr<AsyncOperation>& operation =
        *(static_cast<SharedPtr<AsyncOperation>*>(function_data));
    operation->instance_id_internal()->CancelOperation(operation);
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
