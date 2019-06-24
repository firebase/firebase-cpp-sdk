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

#ifndef FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_ANDROID_INSTANCE_ID_INTERNAL_H_
#define FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_ANDROID_INSTANCE_ID_INTERNAL_H_

#include <assert.h>
#include <jni.h>
#include <vector>

#include "app/memory/shared_ptr.h"
#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "instance_id/src/include/firebase/instance_id.h"
#include "instance_id/src/instance_id_internal_base.h"

namespace firebase {
namespace instance_id {
namespace internal {

// Context for async operations on Android.
class AsyncOperation : public util::JavaThreadContext {
 public:
  AsyncOperation(JNIEnv* env, InstanceIdInternal* instance_id_internal,
                 FutureHandle future_handle)
      : JavaThreadContext(env),
        derived_(nullptr),
        instance_id_internal_(instance_id_internal),
        future_handle_(future_handle) {}
  virtual ~AsyncOperation() {}

  // Get the InstanceId.
  InstanceIdInternal* instance_id_internal() const {
    return instance_id_internal_;
  }

  // Get the future handle from this context.
  template <typename T>
  SafeFutureHandle<T> future_handle() const {
    return SafeFutureHandle<T>(future_handle_);
  }

  // Get the derived class pointer from this class.
  void* derived() const { return derived_; }

 protected:
  void* derived_;

 private:
  InstanceIdInternal* instance_id_internal_;
  FutureHandle future_handle_;
};

// Android specific instance ID data.
class InstanceIdInternal : public InstanceIdInternalBase {
 public:
  // This class must be initialized with Initialize() prior to use.
  InstanceIdInternal();

  // Delete the global reference to the Java InstanceId object.
  ~InstanceIdInternal();

  // Add a global reference to the specified Java InstanceId object and
  // delete the specified local reference.
  // set_instance_id() must be called before this.
  void Initialize(InstanceId* instance_id, jobject java_instance_id);

  // Get the Java InstanceId class.
  jobject java_instance_id() const { return java_instance_id_; }

  // Get the InstanceId object.
  InstanceId* instance_id() const { return instance_id_; }

  // Store a reference to a scheduled operation.
  SharedPtr<AsyncOperation> AddOperation(AsyncOperation* operation);

  // Find the SharedPtr to the operation using raw pointer.
  SharedPtr<AsyncOperation> GetOperationSharedPtr(AsyncOperation* operation);

  // Remove a reference to a schedule operation.
  void RemoveOperation(const SharedPtr<AsyncOperation>& operation);

  // Cancel all scheduled operations.
  void CancelOperations();

  // Complete the future associated with the specified operation and delete the
  // operation.
  template <typename T>
  void CompleteOperationWithResult(const SharedPtr<AsyncOperation>& operation,
                                   const T& result, Error error = kErrorNone,
                                   const char* error_message = nullptr) {
    future_api().CompleteWithResult(operation->future_handle<T>(), error,
                                    error_message ? error_message : "", result);
    RemoveOperation(operation);
  }

  // Complete the future associated with the specified operation and delete the
  // operation.
  void CompleteOperation(const SharedPtr<AsyncOperation>& operation,
                         Error error = kErrorNone,
                         const char* error_message = nullptr);

 private:
  // Cancel the future associated with the specified operation and delete
  // the operation.
  template <typename T>
  void CancelOperationWithResult(const SharedPtr<AsyncOperation>& operation) {
    CompleteOperationWithResult(operation, T(), kErrorUnknown, kCancelledError);
  }

  // Cancel the future associated with the specified operation and delete
  // the operation.
  void CancelOperation(const SharedPtr<AsyncOperation>& operation) {
    CompleteOperation(operation, kErrorUnknown, kCancelledError);
  }

 public:
  // Complete a future with an error when an operation is canceled.
  template <typename T>
  static void CanceledWithResult(void* function_data) {
    AsyncOperation* ptr = static_cast<AsyncOperation*>(function_data);
    // Hold a reference to AsyncOperation so that it will not be deleted during
    // this callback.
    SharedPtr<AsyncOperation> operation =
        ptr->instance_id_internal()->GetOperationSharedPtr(ptr);
    if (!operation) return;
    operation->instance_id_internal()->CancelOperationWithResult<T>(operation);
  }

  static void Canceled(void* function_data);

 private:
  // End user's InstanceId interface.
  InstanceId* instance_id_;
  // Java object instance.
  jobject java_instance_id_;
  std::vector<SharedPtr<AsyncOperation>> operations_;
  // Guards operations_.
  Mutex operations_mutex_;

  static const char* kCancelledError;
};

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase

#endif  // FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_ANDROID_INSTANCE_ID_INTERNAL_H_
