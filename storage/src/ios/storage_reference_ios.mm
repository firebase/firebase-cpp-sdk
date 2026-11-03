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

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
  kStorageReferenceFnListAll,
  kStorageReferenceFnCount,
};

namespace {

// Public entry points that create Obj-C objects on the caller's thread wrap
// their bodies in @autoreleasepool: callers may be plain C++ threads (e.g. a
// game engine's worker thread) with no autorelease pool, where autoreleased
// temporaries would otherwise leak. Objects that must outlive the pool are
// held by strong locals declared inside it and captured (retained) by the
// blocks dispatched to the main queue, or by ObjCPointer-backed C++ objects.

NSURL* _Nullable ParseLocalPathToFileUrl(const char* _Nullable path) {
  @autoreleasepool {
    if (!path || path[0] == '\0') {
      return nil;
    }
    NSString* path_string = [NSString stringWithUTF8String:path];
    if (!path_string || path_string.length == 0) {
      return nil;
    }
    if ([path_string hasPrefix:@"file://"]) {
      return [NSURL URLWithString:path_string];
    }
    return [NSURL fileURLWithPath:path_string];
  }
}

void SafeAttachListener(
    const firebase::util::SafeReference<ListenerInternal>& listener_ref,
    StorageInternal* _Nonnull storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task) {
  listener_ref.Run(
      [storage, task](ListenerInternal* _Nonnull listener_internal) {
        listener_internal->AttachTask(storage, task);
      });
}

void SafeDetachListener(
    const firebase::util::SafeReference<ListenerInternal>& listener_ref) {
  listener_ref.Run([](ListenerInternal* _Nonnull listener_internal) {
    listener_internal->DetachTask();
  });
}

void SafeAssignController(
    const std::shared_ptr<ControllerInternal::LifetimeHandle>& handle,
    StorageInternal* _Nonnull storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task) {
  if (!handle) {
    return;
  }
  ControllerInternal::LifetimeHandle::AssignTask(handle, storage, task);
}

void SafeResetPendingController(
    const std::shared_ptr<ControllerInternal::LifetimeHandle>& handle) {
  if (!handle) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(handle->mutex);
  handle->pending_valid = false;
  handle->pending_cancel = false;
  handle->pending_pause = false;
}

}  // namespace

StorageReferenceInternal::StorageReferenceInternal(
    StorageInternal* storage, std::unique_ptr<FIRStorageReferencePointer> impl)
    : storage_(storage), impl_(std::move(impl)) {
  if (storage_) {
    storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
}

StorageReferenceInternal::~StorageReferenceInternal() {
  if (storage_) {
    storage_->future_manager().ReleaseFutureApi(this);
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& other)
    : storage_(other.storage_),
      impl_(std::make_unique<FIRStorageReferencePointer>(other.impl())) {
  if (storage_) {
    storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& other) {
  if (this == &other) {
    return *this;
  }
  // Like the copy constructor (and the desktop implementation), the assigned
  // reference starts with a fresh Future API: always release the futures that
  // belonged to the old location, even when both share the same storage_.
  if (storage_) {
    storage_->future_manager().ReleaseFutureApi(this);
  }
  storage_ = other.storage_;
  if (storage_) {
    storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  }
  impl_ = std::make_unique<FIRStorageReferencePointer>(other.impl());
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& other) noexcept
    : storage_(other.storage_), impl_(std::move(other.impl_)) {
  other.storage_ = nullptr;
  if (storage_) {
    storage_->future_manager().MoveFutureApi(&other, this);
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  // Always release this object's own Future API first, even when storage_ ==
  // other.storage_: MoveFutureApi() below re-keys other's Future API to
  // `this`, which would otherwise orphan the futures previously owned here.
  if (storage_) {
    storage_->future_manager().ReleaseFutureApi(this);
  }
  storage_ = other.storage_;
  impl_ = std::move(other.impl_);
  other.storage_ = nullptr;
  if (storage_) {
    storage_->future_manager().MoveFutureApi(&other, this);
  }
  return *this;
}

Storage* StorageReferenceInternal::storage() const {
  if (!storage_) {
    return nullptr;
  }
  return Storage::GetInstance(
      storage_->app(),
      storage_->url().empty() ? nullptr : storage_->url().c_str());
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  @autoreleasepool {
    if (!storage_ || !impl() || !path) {
      return nullptr;
    }
    NSString* child_path = [NSString stringWithUTF8String:path];
    if (!child_path) {
      return nullptr;
    }
    FIRStorageReference* child_ref = nil;
    @try {
      child_ref = [impl() child:child_path];
    } @catch (NSException* exception) {
      return nullptr;
    }
    if (!child_ref) {
      return nullptr;
    }
    return new StorageReferenceInternal(
        storage_, std::make_unique<FIRStorageReferencePointer>(child_ref));
  }
}

Future<void> StorageReferenceInternal::Delete() {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      return Future<void>();
    }
    SafeFutureHandle<void> handle =
        future_impl->SafeAlloc<void>(kStorageReferenceFnDelete);
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      future_impl->Complete(handle, kErrorUnknown,
                            GetErrorMessage(kErrorUnknown));
      return DeleteLastResult();
    }
    Future<void> retained_future = DeleteLastResult();
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();
    __block BOOL completed = NO;
    FIRStorageVoidError completion = ^(NSError* error) {
      (void)retained_future;
      if (completed) return;
      completed = YES;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      Error error_code = NSErrorToErrorCode(error);
      std::string error_string = GetNSErrorMessage(error, error_code);
      if (error == nil && alive_storage != nullptr) {
        future_impl->Complete(handle, kErrorNone, "");
      } else {
        Error fallback_error =
            (error_code != kErrorNone)
                ? error_code
                : (alive_storage == nullptr ? kErrorCancelled : kErrorUnknown);
        const char* fallback_message =
            (error != nil) ? error_string.c_str()
                           : GetErrorMessage(fallback_error);
        future_impl->Complete(handle, fallback_error, fallback_message);
      }
    };
    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        if (!completed) {
          completed = YES;
          future_impl->Complete(handle, kErrorCancelled,
                                GetErrorMessage(kErrorCancelled));
        }
        return;
      }
      [my_impl deleteWithCompletion:completion];
    });
    return retained_future;
  }
}

Future<void> StorageReferenceInternal::DeleteLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<void>&>(
                           future_impl->LastResult(kStorageReferenceFnDelete))
                     : Future<void>();
}

std::string StorageReferenceInternal::bucket() const {
  @autoreleasepool {
    FIRStorageReference* my_impl = impl();
    // Keep the NSString in a strong local while copying its UTF-8 buffer.
    NSString* bucket_str = my_impl ? my_impl.bucket : nil;
    const char* bucket_cstr = bucket_str ? bucket_str.UTF8String : nullptr;
    return bucket_cstr ? std::string(bucket_cstr) : std::string();
  }
}

std::string StorageReferenceInternal::full_path() const {
  @autoreleasepool {
    FIRStorageReference* my_impl = impl();
    // Keep the NSString in a strong local while copying its UTF-8 buffer.
    NSString* full_path_str = my_impl ? my_impl.fullPath : nil;
    const char* fp = full_path_str ? full_path_str.UTF8String : nullptr;
    if (!fp || fp[0] == '\0') {
      return std::string("/");
    }
    if (fp[0] == '/') {
      return std::string(fp);
    }
    return std::string("/") + std::string(fp);
  }
}

Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      if (controller_out) *controller_out = Controller();
      return Future<size_t>();
    }
    SafeFutureHandle<size_t> handle =
        future_impl->SafeAlloc<size_t>(kStorageReferenceFnGetFile);
    if (!path || path[0] == '\0') {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorInvalidArgument,
                            "Path cannot be null or empty.");
      return GetFileLastResult();
    }
    NSURL* local_file_url = ParseLocalPathToFileUrl(path);
    if (local_file_url == nil) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorInvalidArgument,
                            "Unable to convert provided path to valid URL");
      return GetFileLastResult();
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorUnknown,
                            GetErrorMessage(kErrorUnknown));
      return GetFileLastResult();
    }

    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }

    Future<size_t> retained_future = GetFileLastResult();
    std::shared_ptr<ControllerInternal> local_controller =
        std::make_shared<ControllerInternal>();
    firebase::util::SafeReference<ListenerInternal> listener_ref =
        (listener && listener->impl_)
            ? listener->impl_->safe_reference()
            : firebase::util::SafeReference<ListenerInternal>();
    std::shared_ptr<ControllerInternal::LifetimeHandle> controller_handle =
        (controller_out && controller_out->internal_)
            ? controller_out->internal_->lifetime_handle()
            : nullptr;
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();

    __block BOOL completed = NO;
    FIRStorageVoidURLError completion = ^(NSURL* /*url*/, NSError* error) {
      (void)retained_future;
      if (completed) return;
      completed = YES;
      if (storage_token) {
        MutexLock lock(storage_token->mutex);
        if (storage_token->futures_alive) {
          StorageInternal* alive_storage = storage_token->storage;
          Error error_code = NSErrorToErrorCode(error);
          std::string error_string = GetNSErrorMessage(error, error_code);
          if (error == nil && alive_storage != nullptr) {
            size_t bytes_transferred =
                static_cast<size_t>(local_controller->bytes_transferred());
            future_impl->CompleteWithResult(
                handle, kErrorNone, "", bytes_transferred);
          } else {
            Error fallback_error =
                (error_code != kErrorNone)
                    ? error_code
                    : (alive_storage == nullptr ? kErrorCancelled
                                                : kErrorUnknown);
            const char* fallback_message =
                (error != nil) ? error_string.c_str()
                               : GetErrorMessage(fallback_error);
            future_impl->Complete(handle, fallback_error, fallback_message);
          }
        }
      }
      SafeDetachListener(listener_ref);
    };

    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      MutexLock lock(storage_token->mutex);
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        SafeResetPendingController(controller_handle);
        if (storage_token->futures_alive && !completed) {
          completed = YES;
          future_impl->Complete(handle, kErrorCancelled,
                                GetErrorMessage(kErrorCancelled));
        }
        return;
      }
      FIRStorageDownloadTask* download_task =
          [my_impl writeToFile:local_file_url completion:completion];
      local_controller->AssignTask(alive_storage, download_task);
      SafeAttachListener(listener_ref, alive_storage, download_task);
      SafeAssignController(controller_handle, alive_storage, download_task);
    });
    return retained_future;
  }
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<size_t>&>(
                           future_impl->LastResult(kStorageReferenceFnGetFile))
                     : Future<size_t>();
}

Future<size_t> StorageReferenceInternal::GetBytes(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out) {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      if (controller_out) *controller_out = Controller();
      return Future<size_t>();
    }
    SafeFutureHandle<size_t> handle =
        future_impl->SafeAlloc<size_t>(kStorageReferenceFnGetBytes);
    if (!buffer) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorInvalidArgument,
                            "Buffer cannot be null.");
      return GetBytesLastResult();
    }
    if (buffer_size > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(
          handle, kErrorInvalidArgument,
          "buffer_size exceeds maximum supported download size.");
      return GetBytesLastResult();
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorUnknown,
                            GetErrorMessage(kErrorUnknown));
      return GetBytesLastResult();
    }

    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }

    Future<size_t> retained_future = GetBytesLastResult();
    firebase::util::SafeReference<ListenerInternal> listener_ref =
        (listener && listener->impl_)
            ? listener->impl_->safe_reference()
            : firebase::util::SafeReference<ListenerInternal>();
    std::shared_ptr<ControllerInternal::LifetimeHandle> controller_handle =
        (controller_out && controller_out->internal_)
            ? controller_out->internal_->lifetime_handle()
            : nullptr;
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();

    __block BOOL completed = NO;
    FIRStorageVoidDataError completion =
        ^(NSData* _Nullable data, NSError* _Nullable error) {
          (void)retained_future;
          if (completed) {
            return;
          }
          completed = YES;
          if (storage_token) {
            MutexLock lock(storage_token->mutex);
            if (storage_token->futures_alive) {
              StorageInternal* alive_storage = storage_token->storage;
              Error error_code = NSErrorToErrorCode(error);
              std::string error_string = GetNSErrorMessage(error, error_code);
              if (error == nil && data != nil && alive_storage != nullptr) {
                if (data.length > buffer_size) {
                  future_impl->Complete(
                      handle, kErrorDownloadSizeExceeded,
                      GetErrorMessage(kErrorDownloadSizeExceeded));
                } else {
                  if (data.length > 0 && data.bytes != nullptr) {
                    memcpy(buffer, data.bytes, data.length);
                  }
                  future_impl->CompleteWithResult(
                      handle, kErrorNone, "", static_cast<size_t>(data.length));
                }
              } else {
                Error fallback_error =
                    (error_code != kErrorNone)
                        ? error_code
                        : (alive_storage == nullptr ? kErrorCancelled
                                                    : kErrorUnknown);
                const char* fallback_message =
                    (error != nil) ? error_string.c_str()
                                   : GetErrorMessage(fallback_error);
                future_impl->Complete(handle, fallback_error, fallback_message);
              }
            }
          }
          SafeDetachListener(listener_ref);
        };

    const int64_t max_download_size = static_cast<int64_t>(buffer_size);
    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      MutexLock lock(storage_token->mutex);
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        SafeResetPendingController(controller_handle);
        if (storage_token->futures_alive && !completed) {
          completed = YES;
          future_impl->Complete(handle, kErrorCancelled,
                                GetErrorMessage(kErrorCancelled));
        }
        return;
      }
      FIRStorageDownloadTask* download_task =
          [my_impl dataWithMaxSize:max_download_size completion:completion];
      SafeAttachListener(listener_ref, alive_storage, download_task);
      SafeAssignController(controller_handle, alive_storage, download_task);
    });
    return retained_future;
  }
}

Future<size_t> StorageReferenceInternal::GetBytesLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<size_t>&>(
                           future_impl->LastResult(kStorageReferenceFnGetBytes))
                     : Future<size_t>();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      return Future<std::string>();
    }
    SafeFutureHandle<std::string> handle =
        future_impl->SafeAlloc<std::string>(kStorageReferenceFnGetDownloadUrl);
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      future_impl->Complete(handle, kErrorUnknown,
                            GetErrorMessage(kErrorUnknown));
      return GetDownloadUrlLastResult();
    }
    Future<std::string> retained_future = GetDownloadUrlLastResult();
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();
    __block BOOL completed = NO;
    FIRStorageVoidURLError completion = ^(NSURL* url, NSError* error) {
      (void)retained_future;
      if (completed) return;
      completed = YES;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      Error error_code = NSErrorToErrorCode(error);
      std::string error_string = GetNSErrorMessage(error, error_code);
      // Keep the NSString in a strong local while its UTF-8 buffer is in use.
      NSString* url_string = url ? url.absoluteString : nil;
      const char* url_utf8 = url_string ? url_string.UTF8String : nullptr;
      if (error == nil && url_utf8 != nullptr && alive_storage != nullptr) {
        future_impl->CompleteWithResult(handle, kErrorNone, "",
                                        std::string(url_utf8));
      } else {
        Error fallback_error =
            (error_code != kErrorNone)
                ? error_code
                : (alive_storage == nullptr ? kErrorCancelled : kErrorUnknown);
        const char* fallback_message =
            (error != nil) ? error_string.c_str()
                           : GetErrorMessage(fallback_error);
        future_impl->Complete(handle, fallback_error, fallback_message);
      }
    };
    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        if (!completed) {
          completed = YES;
          future_impl->Complete(handle, kErrorCancelled,
                                GetErrorMessage(kErrorCancelled));
        }
        return;
      }
      [my_impl downloadURLWithCompletion:completion];
    });
    return retained_future;
  }
}

Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<std::string>&>(
                   future_impl->LastResult(kStorageReferenceFnGetDownloadUrl))
             : Future<std::string>();
}

Future<Metadata> StorageReferenceInternal::GetMetadata() {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      return Future<Metadata>();
    }
    SafeFutureHandle<Metadata> handle =
        future_impl->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadata);
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      Metadata(nullptr));
      return GetMetadataLastResult();
    }
    Future<Metadata> retained_future = GetMetadataLastResult();
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();
    __block BOOL completed = NO;
    FIRStorageVoidMetadataError completion =
        ^(FIRStorageMetadata* metadata, NSError* error) {
          (void)retained_future;
          if (completed) return;
          completed = YES;
          if (!storage_token) {
            return;
          }
          MutexLock lock(storage_token->mutex);
          if (!storage_token->futures_alive) {
            return;
          }
          StorageInternal* alive_storage = storage_token->storage;
          Error error_code = NSErrorToErrorCode(error);
          std::string error_string = GetNSErrorMessage(error, error_code);
          if (error == nil && metadata != nil && alive_storage != nullptr) {
            future_impl->CompleteWithResult(
                handle, kErrorNone, "",
                Metadata(new MetadataInternal(
                    alive_storage,
                    std::make_unique<FIRStorageMetadataPointer>(metadata))));
          } else {
            Error fallback_error =
                (error_code != kErrorNone)
                    ? error_code
                    : (alive_storage == nullptr ? kErrorCancelled
                                                : kErrorUnknown);
            const char* fallback_message =
                (error != nil) ? error_string.c_str()
                               : GetErrorMessage(fallback_error);
            future_impl->CompleteWithResult(handle, fallback_error,
                                            fallback_message, Metadata(nullptr));
          }
        };
    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        if (!completed) {
          completed = YES;
          future_impl->CompleteWithResult(handle, kErrorCancelled,
                                          GetErrorMessage(kErrorCancelled),
                                          Metadata(nullptr));
        }
        return;
      }
      [my_impl metadataWithCompletion:completion];
    });
    return retained_future;
  }
}

Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<Metadata>&>(
                   future_impl->LastResult(kStorageReferenceFnGetMetadata))
             : Future<Metadata>();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      return Future<Metadata>();
    }
    SafeFutureHandle<Metadata> handle =
        future_impl->SafeAlloc<Metadata>(kStorageReferenceFnUpdateMetadata);
    if (!metadata || !metadata->is_valid() || !metadata->internal_) {
      future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                      GetErrorMessage(kErrorInvalidArgument),
                                      Metadata(nullptr));
      return UpdateMetadataLastResult();
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      Metadata(nullptr));
      return UpdateMetadataLastResult();
    }
    Future<Metadata> retained_future = UpdateMetadataLastResult();
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();
    __block BOOL completed = NO;
    FIRStorageVoidMetadataError completion =
        ^(FIRStorageMetadata* metadata_impl, NSError* error) {
          (void)retained_future;
          if (completed) return;
          completed = YES;
          if (!storage_token) {
            return;
          }
          MutexLock lock(storage_token->mutex);
          if (!storage_token->futures_alive) {
            return;
          }
          StorageInternal* alive_storage = storage_token->storage;
          Error error_code = NSErrorToErrorCode(error);
          std::string error_string = GetNSErrorMessage(error, error_code);
          if (error == nil && metadata_impl != nil && alive_storage != nullptr) {
            future_impl->CompleteWithResult(
                handle, kErrorNone, "",
                Metadata(new MetadataInternal(
                    alive_storage,
                    std::make_unique<FIRStorageMetadataPointer>(metadata_impl))));
          } else {
            Error fallback_error =
                (error_code != kErrorNone)
                    ? error_code
                    : (alive_storage == nullptr ? kErrorCancelled
                                                : kErrorUnknown);
            const char* fallback_message =
                (error != nil) ? error_string.c_str()
                               : GetErrorMessage(fallback_error);
            future_impl->CompleteWithResult(handle, fallback_error,
                                            fallback_message, Metadata(nullptr));
          }
        };
    MetadataInternal local_metadata(*metadata->internal_);
    local_metadata.CommitCustomMetadata();
    FIRStorageMetadata* metadata_copy = [local_metadata.impl() copy];
    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        if (!completed) {
          completed = YES;
          future_impl->CompleteWithResult(handle, kErrorCancelled,
                                          GetErrorMessage(kErrorCancelled),
                                          Metadata(nullptr));
        }
        return;
      }
      [my_impl updateMetadata:metadata_copy completion:completion];
    });
    return retained_future;
  }
}

Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl
             ? static_cast<const Future<Metadata>&>(
                   future_impl->LastResult(kStorageReferenceFnUpdateMetadata))
             : Future<Metadata>();
}

std::string StorageReferenceInternal::name() const {
  @autoreleasepool {
    FIRStorageReference* my_impl = impl();
    // Keep the NSString in a strong local while copying its UTF-8 buffer.
    NSString* name_str = my_impl ? my_impl.name : nil;
    const char* name_cstr = name_str ? name_str.UTF8String : nullptr;
    return name_cstr ? std::string(name_cstr) : std::string();
  }
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() const {
  @autoreleasepool {
    if (!storage_ || !impl()) {
      return nullptr;
    }
    FIRStorageReference* parent_ref = impl().parent;
    if (!parent_ref) {
      return nullptr;
    }
    return new StorageReferenceInternal(
        storage_, std::make_unique<FIRStorageReferencePointer>(parent_ref));
  }
}

Future<Metadata> StorageReferenceInternal::PutBytes(const void* buffer,
                                                    size_t buffer_size,
                                                    Listener* listener,
                                                    Controller* controller_out) {
  return PutBytes(buffer, buffer_size, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out) {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      if (controller_out) *controller_out = Controller();
      return Future<Metadata>();
    }
    SafeFutureHandle<Metadata> handle =
        future_impl->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
    if (!buffer && buffer_size > 0) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                      "Buffer cannot be null when size > 0.",
                                      Metadata(nullptr));
      return PutBytesLastResult();
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      Metadata(nullptr));
      return PutBytesLastResult();
    }

    FIRStorageMetadata* metadata_impl = nil;
    Metadata local_metadata;
    if (metadata) local_metadata = *metadata;
    internal::MetadataSetDefaults(&local_metadata);
    if (local_metadata.is_valid()) {
      local_metadata.internal_->CommitCustomMetadata();
      metadata_impl = [local_metadata.internal_->impl() copy];
    }
    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }

    Future<Metadata> retained_future = PutBytesLastResult();
    firebase::util::SafeReference<ListenerInternal> listener_ref =
        (listener && listener->impl_)
            ? listener->impl_->safe_reference()
            : firebase::util::SafeReference<ListenerInternal>();
    std::shared_ptr<ControllerInternal::LifetimeHandle> controller_handle =
        (controller_out && controller_out->internal_)
            ? controller_out->internal_->lifetime_handle()
            : nullptr;
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();

    __block BOOL completed = NO;
    FIRStorageVoidMetadataError completion =
        ^(FIRStorageMetadata* resultant_metadata, NSError* error) {
          (void)retained_future;
          if (completed) return;
          completed = YES;
          if (storage_token) {
            MutexLock lock(storage_token->mutex);
            if (storage_token->futures_alive) {
              StorageInternal* alive_storage = storage_token->storage;
              Error error_code = NSErrorToErrorCode(error);
              std::string error_string = GetNSErrorMessage(error, error_code);
              if (error == nil && resultant_metadata != nil &&
                  alive_storage != nullptr) {
                future_impl->CompleteWithResult(
                    handle, kErrorNone, "",
                    Metadata(new MetadataInternal(
                        alive_storage,
                        std::make_unique<FIRStorageMetadataPointer>(
                            resultant_metadata))));
              } else {
                Error fallback_error =
                    (error_code != kErrorNone)
                        ? error_code
                        : (alive_storage == nullptr ? kErrorCancelled
                                                    : kErrorUnknown);
                const char* fallback_message =
                    (error != nil) ? error_string.c_str()
                                   : GetErrorMessage(fallback_error);
                future_impl->CompleteWithResult(
                    handle, fallback_error, fallback_message, Metadata(nullptr));
              }
            }
          }
          SafeDetachListener(listener_ref);
        };

    // Copy input buffer into NSData so caller buffer lifetime doesn't cause UAF.
    NSData* data_payload =
        (buffer && buffer_size > 0)
            ? [NSData dataWithBytes:buffer length:buffer_size]
            : [NSData data];

    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      MutexLock lock(storage_token->mutex);
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        SafeResetPendingController(controller_handle);
        if (storage_token->futures_alive && !completed) {
          completed = YES;
          future_impl->CompleteWithResult(handle, kErrorCancelled,
                                          GetErrorMessage(kErrorCancelled),
                                          Metadata(nullptr));
        }
        return;
      }
      FIRStorageUploadTask* upload_task = [my_impl putData:data_payload
                                                  metadata:metadata_impl
                                                completion:completion];
      SafeAttachListener(listener_ref, alive_storage, upload_task);
      SafeAssignController(controller_handle, alive_storage, upload_task);
    });
    return retained_future;
  }
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<Metadata>&>(
                           future_impl->LastResult(kStorageReferenceFnPutBytes))
                     : Future<Metadata>();
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  return PutFile(path, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   const Metadata* metadata,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      if (controller_out) *controller_out = Controller();
      return Future<Metadata>();
    }
    SafeFutureHandle<Metadata> handle =
        future_impl->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
    if (!path || path[0] == '\0') {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                      "Path cannot be null or empty.",
                                      Metadata(nullptr));
      return PutFileLastResult();
    }
    NSURL* local_file_url = ParseLocalPathToFileUrl(path);
    if (local_file_url == nil) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(
          handle, kErrorInvalidArgument,
          "Unable to convert provided path to valid URL", Metadata(nullptr));
      return PutFileLastResult();
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      Metadata(nullptr));
      return PutFileLastResult();
    }

    FIRStorageMetadata* metadata_impl = nil;
    Metadata local_metadata;
    if (metadata) local_metadata = *metadata;
    internal::MetadataSetDefaults(&local_metadata);
    if (local_metadata.is_valid()) {
      local_metadata.internal_->CommitCustomMetadata();
      metadata_impl = [local_metadata.internal_->impl() copy];
    }
    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }

    Future<Metadata> retained_future = PutFileLastResult();
    firebase::util::SafeReference<ListenerInternal> listener_ref =
        (listener && listener->impl_)
            ? listener->impl_->safe_reference()
            : firebase::util::SafeReference<ListenerInternal>();
    std::shared_ptr<ControllerInternal::LifetimeHandle> controller_handle =
        (controller_out && controller_out->internal_)
            ? controller_out->internal_->lifetime_handle()
            : nullptr;
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();

    __block BOOL completed = NO;
    FIRStorageVoidMetadataError completion =
        ^(FIRStorageMetadata* resultant_metadata, NSError* error) {
          (void)retained_future;
          if (completed) return;
          completed = YES;
          if (storage_token) {
            MutexLock lock(storage_token->mutex);
            if (storage_token->futures_alive) {
              StorageInternal* alive_storage = storage_token->storage;
              Error error_code = NSErrorToErrorCode(error);
              std::string error_string = GetNSErrorMessage(error, error_code);
              if (error == nil && resultant_metadata != nil &&
                  alive_storage != nullptr) {
                future_impl->CompleteWithResult(
                    handle, kErrorNone, "",
                    Metadata(new MetadataInternal(
                        alive_storage,
                        std::make_unique<FIRStorageMetadataPointer>(
                            resultant_metadata))));
              } else {
                Error fallback_error =
                    (error_code != kErrorNone)
                        ? error_code
                        : (alive_storage == nullptr ? kErrorCancelled
                                                    : kErrorUnknown);
                const char* fallback_message =
                    (error != nil) ? error_string.c_str()
                                   : GetErrorMessage(fallback_error);
                future_impl->CompleteWithResult(
                    handle, fallback_error, fallback_message, Metadata(nullptr));
              }
            }
          }
          SafeDetachListener(listener_ref);
        };

    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      MutexLock lock(storage_token->mutex);
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        SafeResetPendingController(controller_handle);
        if (storage_token->futures_alive && !completed) {
          completed = YES;
          future_impl->CompleteWithResult(handle, kErrorCancelled,
                                          GetErrorMessage(kErrorCancelled),
                                          Metadata(nullptr));
        }
        return;
      }
      // Due to b/73017865 the iOS SDK can end up in an infinite loop if the
      // source file doesn't exist so check for file existence here and abort if
      // it's missing.
      NSError* file_url_exists_error = nil;
      if (![local_file_url
              checkResourceIsReachableAndReturnError:&file_url_exists_error]) {
        SafeResetPendingController(controller_handle);
        if (storage_token->futures_alive && !completed) {
          completed = YES;
          Error error_code = kErrorObjectNotFound;
          const char* error_string = GetErrorMessage(error_code);
          future_impl->CompleteWithResult(handle, error_code, error_string,
                                          Metadata(nullptr));
        }
        return;
      }

      FIRStorageUploadTask* upload_task = [my_impl putFile:local_file_url
                                                  metadata:metadata_impl
                                                completion:completion];
      SafeAttachListener(listener_ref, alive_storage, upload_task);
      SafeAssignController(controller_handle, alive_storage, upload_task);
    });
    return retained_future;
  }
}

Future<Metadata> StorageReferenceInternal::PutFileLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<Metadata>&>(
                           future_impl->LastResult(kStorageReferenceFnPutFile))
                     : Future<Metadata>();
}

Future<StorageListResult> StorageReferenceInternal::List(
    int max_results_per_page, const char* page_token) {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      return Future<StorageListResult>();
    }
    SafeFutureHandle<StorageListResult> handle =
        future_impl->SafeAlloc<StorageListResult>(kStorageReferenceFnList);
    if (max_results_per_page <= 0 || max_results_per_page > 1000) {
      future_impl->CompleteWithResult(
          handle, kErrorInvalidArgument,
          "max_results_per_page must be between 1 and 1000 inclusive.",
          StorageListResult());
      return ListLastResult();
    }
    NSString* page_token_ns = nil;
    if (page_token && page_token[0] != '\0') {
      page_token_ns = [NSString stringWithUTF8String:page_token];
      if (!page_token_ns) {
        future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                        "page_token is not valid UTF-8.",
                                        StorageListResult());
        return ListLastResult();
      }
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      StorageListResult());
      return ListLastResult();
    }
    Future<StorageListResult> retained_future = ListLastResult();
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();

    __block BOOL completed = NO;
    auto completion =
        ^(FIRStorageListResult* _Nullable result, NSError* _Nullable error) {
          (void)retained_future;
          if (completed) return;
          completed = YES;
          if (!storage_token) {
            return;
          }
          MutexLock lock(storage_token->mutex);
          if (!storage_token->futures_alive) {
            return;
          }
          StorageInternal* alive_storage = storage_token->storage;
          Error error_code = NSErrorToErrorCode(error);
          std::string error_string = GetNSErrorMessage(error, error_code);
          if (error == nil && result != nil && alive_storage != nullptr) {
            std::vector<StorageReference> prefixes;
            std::vector<StorageReference> items;
            NSString* next_page_token = result.pageToken;

            auto process_refs = [&](NSArray<FIRStorageReference*>* refs,
                                    std::vector<StorageReference>& out_vec) {
              for (FIRStorageReference* ref in refs) {
                out_vec.push_back(StorageReference(new StorageReferenceInternal(
                    alive_storage,
                    std::make_unique<FIRStorageReferencePointer>(ref))));
              }
            };
            process_refs(result.prefixes, prefixes);
            process_refs(result.items, items);

            // next_page_token is a strong local, so its UTF-8 buffer
            // stays valid until StorageListResultInternal copies it.
            const char* token_cstr =
                next_page_token ? next_page_token.UTF8String : nullptr;
            if (!token_cstr) {
              token_cstr = "";
            }
            internal::StorageListResultInternal* list_result_internal =
                new internal::StorageListResultInternal(
                    std::move(prefixes), std::move(items), token_cstr);
            future_impl->CompleteWithResult(
                handle, kErrorNone, "",
                internal::StorageListResultInternal::AsStorageListResult(
                    list_result_internal));
          } else {
            Error fallback_error =
                (error_code != kErrorNone)
                    ? error_code
                    : (alive_storage == nullptr ? kErrorCancelled
                                                : kErrorUnknown);
            const char* fallback_message =
                (error != nil) ? error_string.c_str()
                               : GetErrorMessage(fallback_error);
            future_impl->CompleteWithResult(
                handle, fallback_error, fallback_message, StorageListResult());
          }
        };

    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        if (!completed) {
          completed = YES;
          future_impl->CompleteWithResult(handle, kErrorCancelled,
                                          GetErrorMessage(kErrorCancelled),
                                          StorageListResult());
        }
        return;
      }
      if (page_token_ns) {
        [my_impl listWithMaxResults:max_results_per_page
                          pageToken:page_token_ns
                         completion:completion];
      } else {
        [my_impl listWithMaxResults:max_results_per_page completion:completion];
      }
    });

    return retained_future;
  }
}

Future<StorageListResult> StorageReferenceInternal::ListLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<StorageListResult>&>(
                           future_impl->LastResult(kStorageReferenceFnList))
                     : Future<StorageListResult>();
}

Future<StorageListResult> StorageReferenceInternal::ListAll() {
  @autoreleasepool {
    ReferenceCountedFutureImpl* future_impl = future();
    if (!future_impl) {
      return Future<StorageListResult>();
    }
    SafeFutureHandle<StorageListResult> handle =
        future_impl->SafeAlloc<StorageListResult>(kStorageReferenceFnListAll);
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      StorageListResult());
      return ListAllLastResult();
    }
    Future<StorageListResult> retained_future = ListAllLastResult();
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token =
        storage_->lifetime_token();

    __block BOOL completed = NO;
    auto completion =
        ^(FIRStorageListResult* _Nullable result, NSError* _Nullable error) {
          (void)retained_future;
          if (completed) return;
          completed = YES;
          if (!storage_token) {
            return;
          }
          MutexLock lock(storage_token->mutex);
          if (!storage_token->futures_alive) {
            return;
          }
          StorageInternal* alive_storage = storage_token->storage;
          Error error_code = NSErrorToErrorCode(error);
          std::string error_string = GetNSErrorMessage(error, error_code);
          if (error == nil && result != nil && alive_storage != nullptr) {
            std::vector<StorageReference> prefixes;
            std::vector<StorageReference> items;

            auto process_refs = [&](NSArray<FIRStorageReference*>* refs,
                                    std::vector<StorageReference>& out_vec) {
              for (FIRStorageReference* ref in refs) {
                out_vec.push_back(StorageReference(new StorageReferenceInternal(
                    alive_storage,
                    std::make_unique<FIRStorageReferencePointer>(ref))));
              }
            };
            process_refs(result.prefixes, prefixes);
            process_refs(result.items, items);

            internal::StorageListResultInternal* list_result_internal =
                new internal::StorageListResultInternal(
                    std::move(prefixes), std::move(items), std::string());
            future_impl->CompleteWithResult(
                handle, kErrorNone, "",
                internal::StorageListResultInternal::AsStorageListResult(
                    list_result_internal));
          } else {
            Error fallback_error =
                (error_code != kErrorNone)
                    ? error_code
                    : (alive_storage == nullptr ? kErrorCancelled
                                                : kErrorUnknown);
            const char* fallback_message =
                (error != nil) ? error_string.c_str()
                               : GetErrorMessage(fallback_error);
            future_impl->CompleteWithResult(
                handle, fallback_error, fallback_message, StorageListResult());
          }
        };

    util::DispatchAsyncSafeMainQueue(^() {
      (void)retained_future;
      if (!storage_token) {
        return;
      }
      MutexLock lock(storage_token->mutex);
      if (!storage_token->futures_alive) {
        return;
      }
      StorageInternal* alive_storage = storage_token->storage;
      if (!alive_storage) {
        if (!completed) {
          completed = YES;
          future_impl->CompleteWithResult(handle, kErrorCancelled,
                                          GetErrorMessage(kErrorCancelled),
                                          StorageListResult());
        }
        return;
      }
      [my_impl listAllWithCompletion:completion];
    });

    return retained_future;
  }
}

Future<StorageListResult> StorageReferenceInternal::ListAllLastResult() const {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<StorageListResult>&>(
                           future_impl->LastResult(kStorageReferenceFnListAll))
                     : Future<StorageListResult>();
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() const {
  return storage_ ? storage_->future_manager().GetFutureApi(
                        const_cast<StorageReferenceInternal*>(this))
                  : nullptr;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
