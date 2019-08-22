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

#include "instance_id/src/ios/instance_id_internal.h"

@interface FIRInstanceIdInternalOperation () {
  FIRInstanceIdInternal *_instanceIdInternal;
  NSRecursiveLock *_lock;
}
@end

@implementation FIRInstanceIdInternalOperation
- (nonnull instancetype)
    initWithInstanceIdInternal:(nonnull FIRInstanceIdInternal*)iidInternal
                          lock:(nonnull NSRecursiveLock*)lock {
  self = [super init];
  if (self) {
    _instanceIdInternal = iidInternal;
    _lock = lock;
  }
  return self;
}

- (nullable FIRInstanceIdInternal*)start {
  [_lock lock];
  return _instanceIdInternal;
}

- (void)end {
  if (_instanceIdInternal) _instanceIdInternal->RemoveOperation(self);
  [_lock unlock];
}

- (void)disconnect {
  [_lock lock];
  _instanceIdInternal = nullptr;
  [_lock unlock];
}
@end

// Maintains a list of active async operations.
@interface FIRInstanceIdInternalOperationList () {
  NSMutableArray* _list;
  NSRecursiveLock* _lock;
}
@end

@implementation FIRInstanceIdInternalOperationList
- (nonnull instancetype)init {
  self = [super init];
  if (self) {
    _list = [[NSMutableArray alloc] init];
    _lock = [[NSRecursiveLock alloc] init];
  }
  return self;
}

- (nonnull FIRInstanceIdInternalOperation*)add:
    (nonnull FIRInstanceIdInternal*)instanceIdInternal {
  [_lock lock];
  FIRInstanceIdInternalOperation* operation =
    [[FIRInstanceIdInternalOperation alloc]
        initWithInstanceIdInternal:instanceIdInternal
                              lock:_lock];
  [_list addObject:operation];
  [_lock unlock];
  return operation;
}

- (void)remove:(nonnull FIRInstanceIdInternalOperation*)operation {
  [_lock lock];
  [_list removeObject:operation];
  [_lock unlock];
}

- (void)disconnectAll {
  [_lock lock];
  for (id object in _list) {
    [((FIRInstanceIdInternalOperation*)object) disconnect];
  }
  [_lock unlock];
}
@end

namespace firebase {
namespace instance_id {
namespace internal {

InstanceIdInternal::InstanceIdInternal(
    FIRInstanceIDPointer *_Nullable fir_instance_id_pointer)
    : fir_instance_id_pointer_(fir_instance_id_pointer) {
  operation_list_ = new FIRInstanceIdInternalOperationListPointer(
      [[FIRInstanceIdInternalOperationList alloc] init]);
}

InstanceIdInternal::~InstanceIdInternal() {
  [operation_list_->get() disconnectAll];
  delete fir_instance_id_pointer_;
  delete operation_list_;
}

FIRInstanceIdInternalOperation *_Nonnull InstanceIdInternal::AddOperation() {
  return [operation_list_->get() add:this];
}

void InstanceIdInternal::RemoveOperation(
    FIRInstanceIdInternalOperation *_Nonnull operation) {
  [operation_list_->get() remove:operation];
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
