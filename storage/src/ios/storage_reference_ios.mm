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

#include "storage/src/ios/storage_reference_ios.h"

#import "FIRStorage.h"
#import "FIRStorageConstants.h"
#import "FIRStorageDownloadTask.h"
#import "FIRStorageReference.h"
#import "FIRStorageTaskSnapshot.h"
#import "FIRStorageUploadTask.h"

#include "app/src/util_ios.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/ios/controller_ios.h"
#include "storage/src/ios/listener_ios.h"
#include "storage/src/ios/metadata_ios.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/util_ios.h"

@interface FIRStorageDownloadTask ()
// WARNING: This exposes the private fetcher property in FIRStorageDownloadTask so that it's
// possible to stop fetching prematurely without signalling an error.
@property(strong, atomic) GTMSessionFetcher* fetcher;

// WARNING: Expose private init method so that it's possible to customize the fetcher's behavior.
- (instancetype)initWithReference:(FIRStorageReference*)reference
                   fetcherService:(GTMSessionFetcherService*)service
                    dispatchQueue:(dispatch_queue_t)queue
                             file:(nullable NSURL*)fileURL;
@end

// Streaming download task.
@interface FIRCPPStorageDownloadTask : FIRStorageDownloadTask
// Buffer to used to aggregate downloaded data.
@property(nonatomic, nullable) void* buffer;
// Size of the block of memory referenced by "buffer".
@property(nonatomic) size_t bufferSize;
// Write offset into "buffer".
@property(nonatomic) size_t bufferDownloadOffset;
// Progress block copied from the fetcher.
// fetcher.receivedProgressBlock can be invalidated before it's required by the streaming block
// so it is cached here.
@property(strong, atomic) GTMSessionFetcherReceivedProgressBlock receivedProgressBlock;
@end

@implementation FIRCPPStorageDownloadTask
// TODO(b/120283849) Find a more stable fix for this issue.
// When we set up our Download Task, we set a state on the session fetcher service, enqueue the
// task, then clear the state. Enqueuing the task now by default happens asynchronously, which
// means the fetcher service state has already been cleared, so to work around that, we change
// dispatchAsync to just execute the block immediately, as it used to do prior to the queue logic.
// The underlying change: https://github.com/firebase/firebase-ios-sdk/pull/1981
- (void)dispatchAsync:(void (^)(void))block {
  block();
}
@end

namespace firebase {
namespace storage {
namespace internal {

// Should reads and writes share thier futures?
enum StorageReferenceFn {
  kStorageReferenceFnDelete = 0,
  kStorageReferenceFnGetBytes,
  kStorageReferenceFnGetFile,
  kStorageReferenceFnGetDownloadUrl,
  kStorageReferenceFnGetMetadata,
  kStorageReferenceFnUpdateMetadata,
  kStorageReferenceFnPutBytes,
  kStorageReferenceFnPutFile,
  kStorageReferenceFnCount,
};

StorageReferenceInternal::StorageReferenceInternal(StorageInternal* storage,
                                                   UniquePtr<FIRStorageReferencePointer> impl)
    : storage_(storage), impl_(std::move(impl)) {
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
}

StorageReferenceInternal::~StorageReferenceInternal() {
  storage_->future_manager().ReleaseFutureApi(this);
}

StorageReferenceInternal::StorageReferenceInternal(const StorageReferenceInternal& other)
    : storage_(other.storage_) {
  impl_.reset(new FIRStorageReferencePointer(*other.impl_));
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& other) {
  impl_.reset(new FIRStorageReferencePointer(*other.impl_));
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageReferenceInternal::StorageReferenceInternal(StorageReferenceInternal&& other)
    : storage_(other.storage_), impl_(std::move(other.impl_)) {
  other.storage_ = nullptr;
  other.impl_ = MakeUnique<FIRStorageReferencePointer>(nil);
  storage_->future_manager().MoveFutureApi(&other, this);
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& other) {
  storage_ = other.storage_;
  impl_ = std::move(other.impl_);
  other.storage_ = nullptr;
  other.impl_ = MakeUnique<FIRStorageReferencePointer>(nil);
  storage_->future_manager().MoveFutureApi(&other, this);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Storage* StorageReferenceInternal::storage() {
  return Storage::GetInstance(storage_->app());
}

StorageReferenceInternal* StorageReferenceInternal::Child(const char* path) const {
  return new StorageReferenceInternal(
      storage_, MakeUnique<FIRStorageReferencePointer>([impl() child:@(path)]));
}

Future<void> StorageReferenceInternal::Delete() {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<void> handle = future_impl->SafeAlloc<void>(kStorageReferenceFnDelete);
  FIRStorageVoidError completion = ^(NSError *error) {
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    future_impl->Complete(handle, error_code, error_string);
  };
  [impl() deleteWithCompletion:completion];
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() {
  return static_cast<const Future<void>&>(future()->LastResult(kStorageReferenceFnDelete));
}

std::string StorageReferenceInternal::bucket() { return std::string(impl().bucket.UTF8String); }

std::string StorageReferenceInternal::full_path() {
  return std::string("/") + std::string(impl().fullPath.UTF8String);
}

Future<size_t> StorageReferenceInternal::GetFile(const char* path, Listener* listener,
                                                 Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<size_t> handle = future_impl->SafeAlloc<size_t>(kStorageReferenceFnGetFile);
  ControllerInternal* local_controller = new ControllerInternal();
  FIRStorageVoidURLError completion = ^(NSURL *url, NSError *error) {
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (error == nil) {
      size_t bytes_transferred = static_cast<size_t>(local_controller->bytes_transferred());
      future_impl->CompleteWithResult(handle, error_code, error_string, bytes_transferred);
    } else {
      future_impl->Complete(handle, error_code, error_string);
    }
    {
      MutexLock mutex(controller_init_mutex_);
      delete local_controller;
    }
  };
  if (controller_out) {
    controller_out->internal_->set_pending_valid(true);
  }
  // Cache a copy of the impl and storage, in case this is destroyed before the thread runs.
  FIRStorageReference* my_impl = impl();
  StorageInternal* storage = storage_;
  util::DispatchAsyncSafeMainQueue(^() {
      FIRStorageDownloadTask *download_task;
      {
        MutexLock mutex(controller_init_mutex_);
        download_task = [my_impl writeToFile:[NSURL URLWithString:@(path)] completion:completion];
        local_controller->AssignTask(storage, download_task);
      }
      if (listener) {
        listener->impl_->AttachTask(storage, download_task);
      }
      if (controller_out) {
        controller_out->internal_->set_pending_valid(false);
        controller_out->internal_->AssignTask(storage, download_task);
      }
    });
  return GetFileLastResult();
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() {
  return static_cast<const Future<size_t>&>(future()->LastResult(kStorageReferenceFnGetFile));
}

FIRStorageDownloadTask* StorageReferenceInternal::CreateStreamingDownloadTask(
    FIRStorageReference* impl, StorageInternal* storage, FIRStorageVoidDataError completion,
    void* buffer, size_t buffer_size) {
  FIRCPPGTMSessionFetcherService* session_fetcher_service = storage->session_fetcher_service();
  FIRCPPStorageDownloadTask* task =
      [[FIRCPPStorageDownloadTask alloc] initWithReference:impl
                                            fetcherService:session_fetcher_service
                                             dispatchQueue:storage->dispatch_queue()
                                                      file:nil];
  task.buffer = buffer;
  task.bufferSize = buffer_size;
  dispatch_queue_t callbackQueue = session_fetcher_service.callbackQueue;
  if (!callbackQueue) callbackQueue = dispatch_get_main_queue();

  // Connect events for download task success / failure to the completion callback.
  [task observeStatus:FIRStorageTaskStatusSuccess
              handler:^(FIRStorageTaskSnapshot* _Nonnull snapshot) {
                dispatch_async(callbackQueue, ^{
                  completion([NSData dataWithBytesNoCopy:buffer
                                                  length:task.bufferDownloadOffset
                                            freeWhenDone:NO],
                             nil);
                });
              }];
  [task observeStatus:FIRStorageTaskStatusFailure
              handler:^(FIRStorageTaskSnapshot* _Nonnull snapshot) {
                dispatch_async(callbackQueue, ^{
                  completion(nil, snapshot.error);
                });
              }];

  GTMSessionFetcherAccumulateDataBlock accumulate_data_block = ^(
      NSData* GTM_NULLABLE_TYPE received_buffer) {
    if (received_buffer) {
      if (task.bufferDownloadOffset == task.bufferSize) {
        return;
      }

      // NSData can reference non-contiguous memory so copy each data range from the object.
      size_t previousDownloadOffset = task.bufferDownloadOffset;
      [received_buffer enumerateByteRangesUsingBlock:^(const void* data_bytes,
                                                       NSRange data_byte_range, BOOL* stop) {
        size_t space_remaining = task.bufferSize - task.bufferDownloadOffset;
        size_t data_byte_range_size = data_byte_range.length;
        size_t copy_size;
        if (data_byte_range_size < space_remaining) {
          copy_size = data_byte_range_size;
        } else {
          copy_size = space_remaining;
          *stop = YES;
        }
        if (copy_size) {
          void* target_buffer = static_cast<char*>(task.buffer) + task.bufferDownloadOffset;
          memcpy(target_buffer, data_bytes, copy_size);
          task.bufferDownloadOffset += copy_size;
        }
      }];

      // When an accumulation block is configured, progress updates are not provided from
      // GTMSessionFetcher so manually notify FIRStorageDownloadTask of download progress.
      GTMSessionFetcherReceivedProgressBlock progressBlock = task.receivedProgressBlock;
      if (progressBlock != nil) {
        dispatch_async(callbackQueue, ^{
          progressBlock(task.bufferDownloadOffset - previousDownloadOffset,
                        task.bufferDownloadOffset);
        });
      }

      // If the buffer is now full, stop downloading.
      if (task.bufferSize == task.bufferDownloadOffset) {
        [task.fetcher stopFetching];
        dispatch_async(callbackQueue, ^{
          // NOTE: We can allocate NSData without copying the input buffer as we know that
          // the receiver of this callback (future completion) does not mutate the NSData object.
          completion(
              [NSData dataWithBytesNoCopy:buffer length:task.bufferDownloadOffset freeWhenDone:NO],
              nil);
          // Remove the reference to this block.
          task.fetcher.accumulateDataBlock = nil;
        });
      }
    } else {
      completion(nil, [NSError errorWithDomain:FIRStorageErrorDomain
                                          code:FIRStorageErrorCodeUnknown
                                      userInfo:nil]);
    }
  };

  util::DispatchAsyncSafeMainQueue(^() {
    @synchronized(session_fetcher_service) {
      session_fetcher_service.accumulateDataBlock = accumulate_data_block;
      [task enqueue];
      session_fetcher_service.accumulateDataBlock = nil;
      task.receivedProgressBlock = task.fetcher.receivedProgressBlock;
    }
  });
  return task;
}

Future<size_t> StorageReferenceInternal::GetBytes(void* buffer, size_t buffer_size,
                                                  Listener* listener, Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<size_t> handle = future_impl->SafeAlloc<size_t>(kStorageReferenceFnGetBytes);
  __block BOOL completed = NO;
  FIRStorageVoidDataError completion = ^(NSData* _Nullable data, NSError* _Nullable error) {
    if (completed) {
      return;
    }
    completed = YES;
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (data != nil) {
      assert(data.length <= buffer_size);
      future_impl->CompleteWithResult(handle, error_code, error_string,
                                      static_cast<size_t>(data.length));
    } else {
      future_impl->Complete(handle, error_code, error_string);
    }
    if (listener) listener->impl_->DetachTask();
  };

  if (controller_out) {
    controller_out->internal_->set_pending_valid(true);
  }
  // Cache a copy of the impl and storage, in case this is destroyed before the thread runs.
  FIRStorageReference* my_impl = impl();
  StorageInternal* storage = storage_;
  util::DispatchAsyncSafeMainQueue(^() {
    // TODO(smiles): Add streaming callback so that the user can actually stream data rather
    // than providing the entire buffer to upload.
    FIRStorageDownloadTask* download_task =
        CreateStreamingDownloadTask(my_impl, storage, completion, buffer, buffer_size);
    if (listener) listener->impl_->AttachTask(storage, download_task);
    if (controller_out) {
      controller_out->internal_->set_pending_valid(false);
      controller_out->internal_->AssignTask(storage, download_task);
    }
  });
  return GetBytesLastResult();
}

Future<size_t> StorageReferenceInternal::GetBytesLastResult() {
  return static_cast<const Future<size_t>&>(future()->LastResult(kStorageReferenceFnGetBytes));
}

Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<std::string> handle =
      future_impl->SafeAlloc<std::string>(kStorageReferenceFnGetDownloadUrl);
  __block BOOL completed = NO;
  FIRStorageVoidURLError completion = ^(NSURL *url, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (url != nil) {
      future_impl->CompleteWithResult(handle, error_code, error_string,
                                      std::string(url.absoluteString.UTF8String));
    } else {
      future_impl->Complete(handle, error_code, error_string);
    }
  };
  [impl() downloadURLWithCompletion:completion];
  return GetDownloadUrlLastResult();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() {
  return static_cast<const Future<std::string>&>(
      future()->LastResult(kStorageReferenceFnGetDownloadUrl));
}

Future<Metadata> StorageReferenceInternal::GetMetadata() {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<Metadata> handle =
      future_impl->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadata);
  StorageInternal* storage = storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion = ^(FIRStorageMetadata *metadata, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (metadata != nil) {
      future_impl->CompleteWithResult(
          handle, error_code, error_string,
          Metadata(new MetadataInternal(storage, MakeUnique<FIRStorageMetadataPointer>(metadata))));
    } else {
      future_impl->CompleteWithResult(handle, error_code, error_string, Metadata(nullptr));
    }
  };
  [impl() metadataWithCompletion:completion];
  return GetMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() {
  return static_cast<const Future<Metadata>&>(future()->LastResult(kStorageReferenceFnGetMetadata));
}

Future<Metadata> StorageReferenceInternal::UpdateMetadata(const Metadata* metadata) {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<Metadata> handle =
      future_impl->SafeAlloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  StorageInternal* storage = storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion = ^(FIRStorageMetadata *metadata_impl, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (metadata_impl != nil) {
      future_impl->CompleteWithResult(
          handle, error_code, error_string,
          Metadata(
              new MetadataInternal(storage, MakeUnique<FIRStorageMetadataPointer>(metadata_impl))));
    } else {
      future_impl->CompleteWithResult(handle, error_code, error_string,
                                      Metadata(nullptr));
    }
  };
  metadata->internal_->CommitCustomMetadata();
  [impl() updateMetadata:metadata->internal_->impl() completion:completion];
  return UpdateMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnUpdateMetadata));
}

std::string StorageReferenceInternal::name() { return std::string(impl().name.UTF8String); }

StorageReferenceInternal* StorageReferenceInternal::GetParent() {
  return new StorageReferenceInternal(storage_,
                                      MakeUnique<FIRStorageReferencePointer>(impl().parent));
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, Listener* listener, Controller* controller_out) {
  return PutBytes(buffer, buffer_size, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata, Listener* listener,
    Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<Metadata> handle = future_impl->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  StorageInternal* storage = storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion =
      ^(FIRStorageMetadata *resultant_metadata, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (error == nil) {
      future_impl->CompleteWithResult(
          handle, error_code, error_string,
          Metadata(new MetadataInternal(
              storage, MakeUnique<FIRStorageMetadataPointer>(resultant_metadata))));
    } else {
      future_impl->CompleteWithResult(handle, error_code, error_string, Metadata(nullptr));
    }
    if (listener) listener->impl_->DetachTask();
  };
  FIRStorageMetadata *metadata_impl = nil;
  Metadata local_metadata;
  if (metadata) local_metadata = *metadata;
  metadata = &local_metadata;
  internal::MetadataSetDefaults(&local_metadata);
  if (metadata->is_valid()) {
    metadata->internal_->CommitCustomMetadata();
    metadata_impl = metadata->internal_->impl();
  }
  if (controller_out) {
    controller_out->internal_->set_pending_valid(true);
  }
  // Cache a copy of the impl, in case this is destroyed before the thread runs.
  FIRStorageReference* my_impl = impl();
  util::DispatchAsyncSafeMainQueue(^() {
      // NOTE: We can allocate NSData without copying the input buffer as we know that
      // FIRStorageUploadTask does not mutate the NSData object.
      FIRStorageUploadTask* upload_task =
          [my_impl putData:[NSData dataWithBytesNoCopy:const_cast<void*>(buffer)
                                                length:buffer_size
                                          freeWhenDone:NO]
                  metadata:metadata_impl
                completion:completion];
      if (listener) {
        listener->impl_->AttachTask(storage_, upload_task);
      }
      if (controller_out) {
        controller_out->internal_->set_pending_valid(false);
        controller_out->internal_->AssignTask(storage_, upload_task);
      }
    });
  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() {
  return static_cast<const Future<Metadata>&>(future()->LastResult(kStorageReferenceFnPutBytes));
}

Future<Metadata> StorageReferenceInternal::PutFile(
    const char* path, Listener* listener, Controller* controller_out) {
  return PutFile(path, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutFile(
    const char* path, const Metadata* metadata, Listener* listener, Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  SafeFutureHandle<Metadata> handle = future_impl->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  StorageInternal* storage = storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion =
      ^(FIRStorageMetadata *resultant_metadata, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    if (error == nil) {
      future_impl->CompleteWithResult(
          handle, error_code, error_string,
          Metadata(new MetadataInternal(
              storage, MakeUnique<FIRStorageMetadataPointer>(resultant_metadata))));
    } else {
      future_impl->CompleteWithResult(handle, error_code, error_string, Metadata(nullptr));
    }
    if (listener) listener->impl_->DetachTask();
  };
  FIRStorageMetadata *metadata_impl = nil;
  Metadata local_metadata;
  if (metadata) local_metadata = *metadata;
  metadata = &local_metadata;
  internal::MetadataSetDefaults(&local_metadata);
  if (metadata->is_valid()) {
    metadata->internal_->CommitCustomMetadata();
    metadata_impl = metadata->internal_->impl();
  }
  NSURL* local_file_url = [NSURL URLWithString:@(path)];
  if (controller_out) {
    controller_out->internal_->set_pending_valid(true);
  }
  // Cache a copy of the impl, in case this is destroyed before the thread runs.
  FIRStorageReference* my_impl = impl();
  util::DispatchAsyncSafeMainQueue(^() {
      // Due to b/73017865 the iOS SDK can end up in an infinite loop if the source file doesn't
      // exist so check for file existence here and abort if it's missing.
      NSError* file_url_exists_error;
      if (![local_file_url checkResourceIsReachableAndReturnError:&file_url_exists_error]) {
        Error error_code = kErrorObjectNotFound;
        const char* error_string = GetErrorMessage(error_code);
        future_impl->CompleteWithResult(handle, error_code, error_string, Metadata(nullptr));
        return;
      }

      FIRStorageUploadTask* upload_task = [my_impl putFile:local_file_url
                                                  metadata:metadata_impl
                                                completion:completion];
      if (listener) {
        listener->impl_->AttachTask(storage, upload_task);
      }
      if (controller_out) {
        controller_out->internal_->set_pending_valid(false);
        controller_out->internal_->AssignTask(storage, upload_task);
      }
  });
  return PutFileLastResult();
}

Future<Metadata> StorageReferenceInternal::PutFileLastResult() {
  return static_cast<const Future<Metadata>&>(future()->LastResult(kStorageReferenceFnPutFile));
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() {
  return storage_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
