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

#import "FirebaseStorage-Swift.h"

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
      memcpy(buffer, data.bytes, data.length);
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
      FIRStorageDownloadTask *download_task = [my_impl dataWithMaxSize:buffer_size
                                                          completion:completion];
      if (listener) listener->impl_->AttachTask(storage, download_task);
      if (controller_out) {
	controller_out->internal_->AssignTask(storage, download_task);
	controller_out->internal_->set_pending_valid(false);
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
