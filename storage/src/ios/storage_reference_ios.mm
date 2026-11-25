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

#include <cassert>
#include <cstring>
#include <utility>
#include "storage/src/ios/storage_reference_ios.h"

#import "FirebaseStorage-Swift.h"

#include "app/src/util_ios.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/list_result.h"
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
  kStorageReferenceFnList,
  kStorageReferenceFnCount,
};

StorageReferenceInternal::StorageReferenceInternal(
    std::shared_ptr<StorageInternal> storage,
    std::unique_ptr<FIRStorageReferencePointer> impl)
    : strong_storage_(std::move(storage)),
      weak_storage_(strong_storage_),
      impl_(std::move(impl)) {
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    WeakTag, std::weak_ptr<StorageInternal> storage,
    std::unique_ptr<FIRStorageReferencePointer> impl)
    : weak_storage_(std::move(storage)), impl_(std::move(impl)) {
  if (auto locked = weak_storage_.lock()) {
    locked->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
}

StorageReferenceInternal* StorageReferenceInternal::CreateWeak(
    std::weak_ptr<StorageInternal> storage,
    std::unique_ptr<FIRStorageReferencePointer> impl) {
  return new StorageReferenceInternal(WeakTag{}, std::move(storage), std::move(impl));
}

StorageReferenceInternal::~StorageReferenceInternal() {
  if (auto locked = LockStorage()) {
    locked->future_manager().ReleaseFutureApi(this);
  }
}

StorageReferenceInternal::StorageReferenceInternal(const StorageReferenceInternal& other)
    : strong_storage_(other.LockStorage()), weak_storage_(other.weak_storage_) {
  impl_.reset(new FIRStorageReferencePointer(*other.impl_));
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& other) {
  if (this == &other) return *this;
  if (auto old_storage = LockStorage()) {
    old_storage->future_manager().ReleaseFutureApi(this);
  }
  strong_storage_ = other.LockStorage();
  weak_storage_ = other.weak_storage_;
  impl_.reset(new FIRStorageReferencePointer(*other.impl_));
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
  return *this;
}

StorageReferenceInternal* StorageReferenceInternal::CloneWeak() const {
  return CreateWeak(weak_storage_, std::make_unique<FIRStorageReferencePointer>(*impl_));
}

// static
StorageReference StorageReferenceInternal::CloneWeakReference(const StorageReference& ref) {
  return ref.internal_ ? StorageReference(ref.internal_->CloneWeak()) : StorageReference();
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageReferenceInternal::StorageReferenceInternal(StorageReferenceInternal&& other)
    : strong_storage_(std::move(other.strong_storage_)),
      weak_storage_(std::move(other.weak_storage_)),
      impl_(std::move(other.impl_)) {
  other.weak_storage_.reset();
  other.impl_ = std::make_unique<FIRStorageReferencePointer>(nil);
  if (auto locked = LockStorage()) {
    locked->future_manager().MoveFutureApi(&other, this);
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& other) {
  if (this == &other) return *this;
  if (auto old_storage = LockStorage()) {
    old_storage->future_manager().ReleaseFutureApi(this);
  }
  strong_storage_ = std::move(other.strong_storage_);
  weak_storage_ = std::move(other.weak_storage_);
  impl_ = std::move(other.impl_);
  other.weak_storage_.reset();
  other.impl_ = std::make_unique<FIRStorageReferencePointer>(nil);
  if (auto new_storage = LockStorage()) {
    new_storage->future_manager().MoveFutureApi(&other, this);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

bool StorageReferenceInternal::is_valid() const {
  return impl() != nil && LockStorage() != nullptr;
}

Storage StorageReferenceInternal::storage() const {
  return Storage(LockStorage());
}

StorageReferenceInternal* StorageReferenceInternal::Child(const char* path) const {
  auto locked_storage = LockStorage();
  if (!locked_storage || impl() == nil) return nullptr;
  return new StorageReferenceInternal(
      std::move(locked_storage),
      std::make_unique<FIRStorageReferencePointer>([impl() child:@(path)]));
}

Future<void> StorageReferenceInternal::Delete() {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<void>();
  SafeFutureHandle<void> handle = future_impl->SafeAlloc<void>(kStorageReferenceFnDelete);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  FIRStorageVoidError completion = ^(NSError *error) {
    Error error_code = kErrorNone;
    const char* error_string = "";
    {
      auto locked_storage = weak_storage.lock();
      if (!locked_storage) return;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
    }
    ReferenceCountedFutureImpl::CompleteCore(future_core, handle, error_code, error_string);
  };
  [impl() deleteWithCompletion:completion];
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<void>&>(future_impl->LastResult(kStorageReferenceFnDelete))
             : Future<void>();
}

std::string StorageReferenceInternal::bucket() const {
  return impl() ? std::string(impl().bucket.UTF8String) : std::string();
}

std::string StorageReferenceInternal::full_path() const {
  return impl() ? std::string("/") + std::string(impl().fullPath.UTF8String) : std::string();
}

Future<size_t> StorageReferenceInternal::GetFile(const char* path, Listener* listener,
                                                 Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<size_t>();
  SafeFutureHandle<size_t> handle = future_impl->SafeAlloc<size_t>(kStorageReferenceFnGetFile);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  auto controller_init_mutex = std::make_shared<Mutex>();
  ControllerInternal* local_controller = new ControllerInternal();
  FIRStorageVoidURLError completion = ^(NSURL *url, NSError *error) {
    bool should_complete = false;
    Error error_code = kErrorNone;
    const char* error_string = "";
    bool succeeded = false;
    size_t bytes_transferred = 0;
    if (auto locked_storage = weak_storage.lock()) {
      should_complete = true;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (error == nil) {
        succeeded = true;
        bytes_transferred = static_cast<size_t>(local_controller->bytes_transferred());
      }
    }
    {
      MutexLock mutex(*controller_init_mutex);
      delete local_controller;
    }
    if (should_complete) {
      if (succeeded) {
        ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                           error_string, bytes_transferred);
      } else {
        ReferenceCountedFutureImpl::CompleteCore(future_core, handle, error_code, error_string);
      }
    }
  };
  if (controller_out) {
    controller_out->internal_->set_pending_valid(true);
  }
  // Cache a copy of the impl and storage, in case this is destroyed before the thread runs.
  FIRStorageReference* my_impl = impl();
  NSString* path_string = @(path);
  if (path_string.length == 0) {
    future_impl->Complete(handle, kErrorUnknown, "Path cannot be empty.");
    delete local_controller;
    return GetFileLastResult();
  }
  NSURL* local_file_url = nil;
  if ([path_string hasPrefix:@"file://"]) {
    // If it starts with the prefix, load it assuming a URL string.
    local_file_url = [NSURL URLWithString:path_string];
  } else {
    // Otherwise, assume it is a file path.
    local_file_url = [NSURL fileURLWithPath:path_string];
  }
  // If we still failed to convert the path, error out.
  if (local_file_url == nil) {
    future_impl->Complete(handle, kErrorUnknown,
      "Unable to convert provided path to valid URL");
    delete local_controller;
    return GetFileLastResult();
  }
  util::DispatchAsyncSafeMainQueue(^() {
      FIRStorageDownloadTask *download_task;
      {
        MutexLock mutex(*controller_init_mutex);
        download_task = [my_impl writeToFile:local_file_url completion:completion];
        local_controller->AssignTask(weak_storage, download_task);
      }
      if (listener) {
        listener->impl_->AttachTask(weak_storage, download_task);
      }
      if (controller_out) {
        controller_out->internal_->set_pending_valid(false);
        controller_out->internal_->AssignTask(weak_storage, download_task);
      }
    });
  return GetFileLastResult();
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<size_t>&>(future_impl->LastResult(kStorageReferenceFnGetFile))
             : Future<size_t>();
}

Future<size_t> StorageReferenceInternal::GetBytes(void* buffer, size_t buffer_size,
                                                  Listener* listener, Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<size_t>();
  SafeFutureHandle<size_t> handle = future_impl->SafeAlloc<size_t>(kStorageReferenceFnGetBytes);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  __block BOOL completed = NO;
  FIRStorageVoidDataError completion = ^(NSData* _Nullable data, NSError* _Nullable error) {
    if (completed) {
      return;
    }
    completed = YES;
    bool should_complete = false;
    Error error_code = kErrorNone;
    const char* error_string = "";
    bool has_data = false;
    size_t data_length = 0;
    if (auto locked_storage = weak_storage.lock()) {
      should_complete = true;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (data != nil) {
        assert(data.length <= buffer_size);
        memcpy(buffer, data.bytes, data.length);
        has_data = true;
        data_length = static_cast<size_t>(data.length);
      }
    }
    if (listener) listener->impl_->DetachTask();
    if (should_complete) {
      if (has_data) {
        ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                           error_string, data_length);
      } else {
        ReferenceCountedFutureImpl::CompleteCore(future_core, handle, error_code, error_string);
      }
    }
  };

  if (controller_out) {
    controller_out->internal_->set_pending_valid(true);
  }
  // Cache a copy of the impl and storage, in case this is destroyed before the thread runs.
  FIRStorageReference* my_impl = impl();
  util::DispatchAsyncSafeMainQueue(^() {
      FIRStorageDownloadTask *download_task = [my_impl dataWithMaxSize:buffer_size
                                                          completion:completion];
      if (listener) listener->impl_->AttachTask(weak_storage, download_task);
      if (controller_out) {
        controller_out->internal_->AssignTask(weak_storage, download_task);
        controller_out->internal_->set_pending_valid(false);
      }
    });
  return GetBytesLastResult();
}

Future<size_t> StorageReferenceInternal::GetBytesLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<size_t>&>(future_impl->LastResult(kStorageReferenceFnGetBytes))
             : Future<size_t>();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<std::string>();
  SafeFutureHandle<std::string> handle =
      future_impl->SafeAlloc<std::string>(kStorageReferenceFnGetDownloadUrl);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  __block BOOL completed = NO;
  FIRStorageVoidURLError completion = ^(NSURL *url, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = kErrorNone;
    const char* error_string = "";
    bool has_url = false;
    std::string url_str;
    {
      auto locked_storage = weak_storage.lock();
      if (!locked_storage) return;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (url != nil) {
        has_url = true;
        url_str = url.absoluteString.UTF8String;
      }
    }
    if (has_url) {
      ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                         error_string, url_str);
    } else {
      ReferenceCountedFutureImpl::CompleteCore(future_core, handle, error_code, error_string);
    }
  };
  [impl() downloadURLWithCompletion:completion];
  return GetDownloadUrlLastResult();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<std::string>&>(
                           future_impl->LastResult(kStorageReferenceFnGetDownloadUrl))
                     : Future<std::string>();
}

Future<Metadata> StorageReferenceInternal::GetMetadata() {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<Metadata>();
  SafeFutureHandle<Metadata> handle =
      future_impl->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadata);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion = ^(FIRStorageMetadata *metadata, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = kErrorNone;
    const char* error_string = "";
    Metadata result_metadata(nullptr);
    {
      auto locked_storage = weak_storage.lock();
      if (!locked_storage) return;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (metadata != nil) {
        result_metadata = Metadata(new MetadataInternal(
            weak_storage, std::make_unique<FIRStorageMetadataPointer>(metadata)));
      }
    }
    ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                       error_string, result_metadata);
  };
  [impl() metadataWithCompletion:completion];
  return GetMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<Metadata>&>(future_impl->LastResult(kStorageReferenceFnGetMetadata))
             : Future<Metadata>();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadata(const Metadata* metadata) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<Metadata>();
  SafeFutureHandle<Metadata> handle =
      future_impl->SafeAlloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion = ^(FIRStorageMetadata *metadata_impl, NSError *error) {
    if (completed) return;
    completed = YES;
    Error error_code = kErrorNone;
    const char* error_string = "";
    Metadata result_metadata(nullptr);
    {
      auto locked_storage = weak_storage.lock();
      if (!locked_storage) return;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (metadata_impl != nil) {
        result_metadata = Metadata(new MetadataInternal(
            weak_storage, std::make_unique<FIRStorageMetadataPointer>(metadata_impl)));
      }
    }
    ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                       error_string, result_metadata);
  };
  metadata->internal_->CommitCustomMetadata();
  [impl() updateMetadata:metadata->internal_->impl() completion:completion];
  return UpdateMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<Metadata>&>(
                           future_impl->LastResult(kStorageReferenceFnUpdateMetadata))
                     : Future<Metadata>();
}

std::string StorageReferenceInternal::name() {
  return impl() ? std::string(impl().name.UTF8String) : std::string();
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() {
  auto locked_storage = LockStorage();
  if (!locked_storage || impl() == nil) return nullptr;
  return new StorageReferenceInternal(std::move(locked_storage),
                                      std::make_unique<FIRStorageReferencePointer>(impl().parent));
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, Listener* listener, Controller* controller_out) {
  return PutBytes(buffer, buffer_size, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata, Listener* listener,
    Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<Metadata>();
  SafeFutureHandle<Metadata> handle = future_impl->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion =
      ^(FIRStorageMetadata *resultant_metadata, NSError *error) {
    if (completed) return;
    completed = YES;
    bool should_complete = false;
    Error error_code = kErrorNone;
    const char* error_string = "";
    Metadata result_metadata(nullptr);
    if (auto locked_storage = weak_storage.lock()) {
      should_complete = true;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (error == nil) {
        result_metadata = Metadata(new MetadataInternal(
            weak_storage, std::make_unique<FIRStorageMetadataPointer>(resultant_metadata)));
      }
    }
    if (listener) listener->impl_->DetachTask();
    if (should_complete) {
      ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                         error_string, result_metadata);
    }
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
        listener->impl_->AttachTask(weak_storage, upload_task);
      }
      if (controller_out) {
        controller_out->internal_->set_pending_valid(false);
        controller_out->internal_->AssignTask(weak_storage, upload_task);
      }
    });
  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<Metadata>&>(future_impl->LastResult(kStorageReferenceFnPutBytes))
             : Future<Metadata>();
}

Future<Metadata> StorageReferenceInternal::PutFile(
    const char* path, Listener* listener, Controller* controller_out) {
  return PutFile(path, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutFile(
    const char* path, const Metadata* metadata, Listener* listener, Controller* controller_out) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<Metadata>();
  SafeFutureHandle<Metadata> handle = future_impl->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  __block BOOL completed = NO;
  FIRStorageVoidMetadataError completion =
      ^(FIRStorageMetadata *resultant_metadata, NSError *error) {
    if (completed) return;
    completed = YES;
    bool should_complete = false;
    Error error_code = kErrorNone;
    const char* error_string = "";
    Metadata result_metadata(nullptr);
    if (auto locked_storage = weak_storage.lock()) {
      should_complete = true;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (error == nil) {
        result_metadata = Metadata(new MetadataInternal(
            weak_storage, std::make_unique<FIRStorageMetadataPointer>(resultant_metadata)));
      }
    }
    if (listener) listener->impl_->DetachTask();
    if (should_complete) {
      ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                         error_string, result_metadata);
    }
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
        bool should_complete = false;
        {
          if (auto locked_storage = weak_storage.lock()) {
            should_complete = true;
          }
        }
        if (should_complete) {
          Error error_code = kErrorObjectNotFound;
          const char* error_string = GetErrorMessage(error_code);
          ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                             error_string, Metadata(nullptr));
        }
        return;
      }

      FIRStorageUploadTask* upload_task = [my_impl putFile:local_file_url
                                                  metadata:metadata_impl
                                                completion:completion];
      if (listener) {
        listener->impl_->AttachTask(weak_storage, upload_task);
      }
      if (controller_out) {
        controller_out->internal_->set_pending_valid(false);
        controller_out->internal_->AssignTask(weak_storage, upload_task);
      }
  });
  return PutFileLastResult();
}

Future<Metadata> StorageReferenceInternal::PutFileLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<Metadata>&>(future_impl->LastResult(kStorageReferenceFnPutFile))
             : Future<Metadata>();
}

Future<StorageListResult> StorageReferenceInternal::List(int max_results_per_page,
                                                         const char* page_token) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl) return Future<StorageListResult>();
  SafeFutureHandle<StorageListResult> handle =
      future_impl->SafeAlloc<StorageListResult>(kStorageReferenceFnList);
  auto future_core = future_impl->core();
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;

  auto completion = ^(FIRStorageListResult* _Nullable result, NSError* _Nullable error) {
    Error error_code = kErrorNone;
    const char* error_string = "";
    StorageListResult list_result;
    {
      auto locked_storage = weak_storage.lock();
      if (!locked_storage) return;
      error_code = NSErrorToErrorCode(error);
      error_string = GetErrorMessage(error_code);
      if (error == nil) {
        std::vector<StorageReference> prefixes;
        std::vector<StorageReference> items;
        NSString* next_page_token = result.pageToken;

        auto process_refs = [&](NSArray<FIRStorageReference*>* refs,
                                std::vector<StorageReference>& out_vec) {
          for (FIRStorageReference* ref in refs) {
            out_vec.push_back(StorageReference(StorageReferenceInternal::CreateWeak(
                weak_storage, std::make_unique<FIRStorageReferencePointer>(ref))));
          }
        };
        process_refs(result.prefixes, prefixes);
        process_refs(result.items, items);

        internal::StorageListResultInternal* list_result_internal =
            new internal::StorageListResultInternal(
                std::move(prefixes), std::move(items),
                next_page_token ? [next_page_token UTF8String] : "");
        list_result =
            internal::StorageListResultInternal::AsStorageListResult(list_result_internal);
      }
    }
    ReferenceCountedFutureImpl::CompleteCoreWithResult(future_core, handle, error_code,
                                                       error_string, list_result);
  };

  FIRStorageReference* my_impl = impl();
  NSString* page_token_ns = page_token ? @(page_token) : nil;

  util::DispatchAsyncSafeMainQueue(^() {
    if (page_token_ns) {
      [my_impl listWithMaxResults:max_results_per_page
                        pageToken:page_token_ns
                       completion:completion];
    } else {
      [my_impl listWithMaxResults:max_results_per_page completion:completion];
    }
  });

  return ListLastResult();
}

Future<StorageListResult> StorageReferenceInternal::ListLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<StorageListResult>&>(
                           future_impl->LastResult(kStorageReferenceFnList))
                     : Future<StorageListResult>();
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() {
  auto locked_storage = LockStorage();
  return locked_storage ? locked_storage->future_manager().GetFutureApi(this) : nullptr;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
