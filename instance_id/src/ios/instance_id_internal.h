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

#ifndef FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_STUB_INSTANCE_ID_INTERNAL_H_
#define FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_STUB_INSTANCE_ID_INTERNAL_H_

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#endif   // defined(__OBJC__)

#include "app/src/mutex.h"
#include "app/src/util_ios.h"
#include "instance_id/src/instance_id_internal_base.h"

#if defined(__OBJC__)
#include "FIRInstanceID.h"
#endif  // defined(__OBJC__)

#if defined(__OBJC__)
typedef firebase::instance_id::internal::InstanceIdInternal
        FIRInstanceIdInternal;

// Async operation being performed on a InstanceIdInternal instance.
@interface FIRInstanceIdInternalOperation : NSObject

- (nonnull instancetype)init NS_UNAVAILABLE;

// Initialize the operation.
- (nonnull instancetype)
    initWithInstanceIdInternal:(nonnull FIRInstanceIdInternal*)iidInternal
                          lock:(nonnull NSRecursiveLock *)lock;
// Get the InstanceIdInternal and acquire the lock.
- (nullable FIRInstanceIdInternal*)start;

// Release the lock and complete the operation.
- (void)end;

// Remove the reference to InstanceIdInternal from this object.
- (void)disconnect;

@end

// Maintains a list of active async operations.
@interface FIRInstanceIdInternalOperationList : NSObject

// Initialize a new instance.
- (nonnull instancetype)init;

// Create and add an operation to this list.
- (nonnull FIRInstanceIdInternalOperation*)add:
    (nonnull FIRInstanceIdInternal*)instanceIdInternal;

// Remove an operation from this list.
- (void)remove:(nonnull FIRInstanceIdInternalOperation*)operation;

// Remove all references to InstanceIdInternal instances from the operations
// in this list.
- (void)disconnectAll;

@end
#endif  // defined(__OBJC__)

namespace firebase {
namespace instance_id {
namespace internal {

OBJ_C_PTR_WRAPPER(FIRInstanceID);
OBJ_C_PTR_WRAPPER(FIRInstanceIdInternalOperationList);

class InstanceIdInternal : public InstanceIdInternalBase {
 public:
  explicit InstanceIdInternal(
      FIRInstanceIDPointer *_Nullable fir_instance_id_pointer);
  ~InstanceIdInternal();

#if defined(__OBJC__)
  // Get the Obj-C instance ID object.
  FIRInstanceID *_Nullable GetFIRInstanceID() const {
    return fir_instance_id_pointer_->get();
  }

  // Create a reference to an instance ID object to the array tracking
  // outstanding operations.
  FIRInstanceIdInternalOperation *_Nonnull AddOperation();
  // Remove a reference to an instance ID object from the array tracking
  // outstanding operations.
  void RemoveOperation(FIRInstanceIdInternalOperation *_Nonnull operation);
#endif  // defined(__OBJC__)

 private:
  FIRInstanceIDPointer *_Nullable fir_instance_id_pointer_;
  // Contains Operation objects which reference this class for each operation.
  // DisconnectOperation() clears all objects to remove all references to this
  // object from async operations.
  FIRInstanceIdInternalOperationListPointer *_Nullable operation_list_;
};

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase

#endif  // FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_STUB_INSTANCE_ID_INTERNAL_H_
