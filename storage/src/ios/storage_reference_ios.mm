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

#define IOS_ASYNC_COMPLETION(RESULT_TYPE, CONVERT_RESULT_EXPR, ERROR_FALLBACK, TASK_EXEC_EXPR) \
  @autoreleasepool { \
    ReferenceCountedFutureImpl* future_impl = api; \
    FIRStorageReference* my_impl = impl(); \
    if (!my_impl || !storage_) { \
      if (controller_out) *controller_out = Controller(); \
      future_impl->Complete(handle, kErrorUnknown, GetErrorMessage(kErrorUnknown)); \
      return; \
    } \
    if (controller_out && controller_out->internal_ == nullptr) controller_out->internal_ = new ControllerInternal(); \
    if (controller_out && controller_out->internal_) controller_out->internal_->set_pending_valid(true); \
    std::shared_ptr<ControllerInternal> local_controller = std::make_shared<ControllerInternal>(); \
    firebase::util::SafeReference<ListenerInternal> listener_ref = (listener && listener->impl_) ? listener->impl_->safe_reference() : firebase::util::SafeReference<ListenerInternal>(); \
    std::shared_ptr<ControllerInternal::LifetimeHandle> controller_handle = (controller_out && controller_out->internal_) ? controller_out->internal_->lifetime_handle() : nullptr; \
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token = storage_->lifetime_token(); \
    __block BOOL completed = NO; \
    auto completion = ^(id first_arg, NSError* error) { \
      if (storage_token) { \
        MutexLock lock(storage_token->mutex); \
        if (completed) return; \
        completed = YES; \
        if (storage_token->futures_alive) { \
          StorageInternal* alive_storage = storage_token->storage; \
          Error error_code = NSErrorToErrorCode(error); \
          std::string error_string = GetNSErrorMessage(error, error_code); \
          if (error == nil && alive_storage != nullptr) { \
            RESULT_TYPE result = (CONVERT_RESULT_EXPR); \
            if constexpr (std::is_same_v<RESULT_TYPE, void*>) { \
              future_impl->Complete(handle, kErrorNone, ""); \
            } else { \
              future_impl->CompleteWithResult(handle, kErrorNone, "", result); \
            } \
          } else { \
            Error fallback_error = (error_code != kErrorNone) ? error_code : (alive_storage == nullptr ? kErrorCancelled : kErrorUnknown); \
            const char* fallback_message = (error != nil) ? error_string.c_str() : GetErrorMessage(fallback_error); \
            future_impl->Complete(handle, fallback_error, fallback_message); \
          } \
        } \
      } \
      SafeDetachListener(listener_ref); \
    }; \
    id task_obj = (TASK_EXEC_EXPR); \
    if ([task_obj isKindOfClass:[FIRStorageObservableTask class]]) { \
      FIRStorageObservableTask* task = (FIRStorageObservableTask*)task_obj; \
      local_controller->set_task(task); \
      if (controller_handle) controller_handle->AssignTask(controller_handle, storage_, task); \
      dispatch_async(dispatch_get_main_queue(), ^{ \
        if (storage_token) { \
          MutexLock lock(storage_token->mutex); \
          if (!completed && storage_token->alive && listener_ref.is_valid()) SafeAttachListener(listener_ref, task); \
        } \
      }); \
    } \
  }


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
#include "storage/src/common/metadata_internal.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/util_ios.h"

namespace firebase {
namespace storage {
namespace internal {

FIRStorageMetadata* MetadataToFIRStorageMetadata(const MetadataInternal& meta) {
  FIRStorageMetadata* fir_meta = [[FIRStorageMetadata alloc] init];
  if (meta.cache_control() && meta.cache_control()[0] != '\0') {
    fir_meta.cacheControl = @(meta.cache_control());
  }
  if (meta.content_disposition() && meta.content_disposition()[0] != '\0') {
    fir_meta.contentDisposition = @(meta.content_disposition());
  }
  if (meta.content_encoding() && meta.content_encoding()[0] != '\0') {
    fir_meta.contentEncoding = @(meta.content_encoding());
  }
  if (meta.content_language() && meta.content_language()[0] != '\0') {
    fir_meta.contentLanguage = @(meta.content_language());
  }
  if (meta.content_type() && meta.content_type()[0] != '\0') {
    fir_meta.contentType = @(meta.content_type());
  }
  if (meta.custom_metadata() && !meta.custom_metadata()->empty()) {
    NSMutableDictionary<NSString*, NSString*>* dict = [[NSMutableDictionary alloc] init];
    for (const auto& kv : *meta.custom_metadata()) {
      dict[@(kv.first.c_str())] = @(kv.second.c_str());
    }
    fir_meta.customMetadata = dict;
  }
  return fir_meta;
}

MetadataInternal* FIRStorageMetadataToMetadata(FIRStorageMetadata* fir_meta, StorageReference ref) {
  MetadataInternal* meta = new MetadataInternal(ref);
  if (fir_meta.bucket) meta->set_bucket(fir_meta.bucket.UTF8String);
  if (fir_meta.cacheControl) meta->set_cache_control(fir_meta.cacheControl.UTF8String);
  if (fir_meta.contentDisposition) meta->set_content_disposition(fir_meta.contentDisposition.UTF8String);
  if (fir_meta.contentEncoding) meta->set_content_encoding(fir_meta.contentEncoding.UTF8String);
  if (fir_meta.contentLanguage) meta->set_content_language(fir_meta.contentLanguage.UTF8String);
  if (fir_meta.contentType) meta->set_content_type(fir_meta.contentType.UTF8String);
  if (fir_meta.name) meta->set_name(fir_meta.name.UTF8String);
  if (fir_meta.path) meta->set_path(fir_meta.path.UTF8String);
  meta->set_size_bytes(fir_meta.size);
  if (fir_meta.timeCreated) meta->set_creation_time((int64_t)([fir_meta.timeCreated timeIntervalSince1970] * 1000));
  if (fir_meta.updated) meta->set_updated_time((int64_t)([fir_meta.updated timeIntervalSince1970] * 1000));
  if (fir_meta.md5Hash) meta->set_md5_hash(fir_meta.md5Hash.UTF8String);
  meta->set_generation(fir_meta.generation);
  meta->set_metadata_generation(fir_meta.metageneration);
  if (fir_meta.customMetadata) {
    for (NSString* key in fir_meta.customMetadata) {
      NSString* value = fir_meta.customMetadata[key];
      if (key && value) {
        (*meta->custom_metadata())[key.UTF8String] = value.UTF8String;
      }
    }
  }
  return meta;
}



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

// Must be called WITHOUT holding storage_token->mutex: the lock order is the
// listener's SafeReference lock, then the token mutex, then the listener's own
// mutex (the completion blocks detach listeners the same way). AttachTask()
// only attaches if the StorageInternal is still alive.
void SafeAttachListener(
    const firebase::util::SafeReference<ListenerInternal>& listener_ref,
    const std::shared_ptr<StorageInternal::LifetimeToken>& storage_token,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task) {
  listener_ref.Run(
      [&storage_token, task](ListenerInternal* _Nonnull listener_internal) {
        listener_internal->AttachTask(storage_token, task);
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
    
  }
}

StorageReferenceInternal::~StorageReferenceInternal() {
  if (storage_) {
    
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& other)
    : storage_(other.storage_),
      impl_(std::make_unique<FIRStorageReferencePointer>(other.impl())) {
  if (storage_) {
    
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
    
  }
  storage_ = other.storage_;
  if (storage_) {
    
  }
  impl_ = std::make_unique<FIRStorageReferencePointer>(other.impl());
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& other) noexcept
    : storage_(other.storage_), impl_(std::move(other.impl_)) {
  other.storage_ = nullptr;
  if (storage_) {
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
    
  }
  storage_ = other.storage_;
  impl_ = std::move(other.impl_);
  other.storage_ = nullptr;
  if (storage_) {
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


template <typename FutureType, typename CompletionBlockT, typename TaskBlockT>
void RunIosTask(
    StorageInternal* storage_,
    SafeFutureHandle<FutureType> handle,
    ReferenceCountedFutureImpl* future_impl,
    internal::StorageReferenceFn fn,
    FIRStorageReference* my_impl,
    Listener* listener,
    Controller* controller_out,
    CompletionBlockT make_completion,
    TaskBlockT execute_task) {
  if (!my_impl || !storage_) {
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorUnknown, GetErrorMessage(kErrorUnknown));
    return;
  }
  if (controller_out && controller_out->internal_ == nullptr) controller_out->internal_ = new ControllerInternal();
  if (controller_out && controller_out->internal_) controller_out->internal_->set_pending_valid(true);
  
  std::shared_ptr<ControllerInternal> local_controller = std::make_shared<ControllerInternal>();
  firebase::util::SafeReference<ListenerInternal> listener_ref =
      (listener && listener->impl_) ? listener->impl_->safe_reference() : firebase::util::SafeReference<ListenerInternal>();
  std::shared_ptr<ControllerInternal::LifetimeHandle> controller_handle =
      (controller_out && controller_out->internal_) ? controller_out->internal_->lifetime_handle() : nullptr;
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token = storage_->lifetime_token();

  __block BOOL completed = NO;
  auto completion = make_completion(storage_token, completed, local_controller, listener_ref);
  
  FIRStorageObservableTask* task = execute_task(my_impl, completion);
  
  if (task != nil) {
    local_controller->set_task(task);
    if (controller_handle) {
      controller_handle->AssignTask(controller_handle, storage_, task);
    }
    // Main queue for listener attachment
    dispatch_async(dispatch_get_main_queue(), ^{
      if (storage_token) {
        MutexLock lock(storage_token->mutex);
        if (!completed && storage_token->alive) {
          if (listener_ref.is_valid()) {
            SafeAttachListener(listener_ref, task);
          }
        }
      }
    });
  } else {
    // If task is nil, immediately complete with error if not already completed
    if (storage_token) {
      MutexLock lock(storage_token->mutex);
      if (!completed && storage_token->futures_alive) {
        completed = YES;
        future_impl->Complete(handle, kErrorUnknown, GetErrorMessage(kErrorUnknown));
      }
    }
    SafeDetachListener(listener_ref);
  }
}

void StorageReferenceInternal::DeleteImpl(SafeFutureHandle<void> handle, ReferenceCountedFutureImpl* api) {
  Listener* listener = nullptr; Controller* controller_out = nullptr;
  IOS_ASYNC_COMPLETION(void*, nullptr, kErrorUnknown,
      [my_impl deleteWithCompletion:^(NSError* err){ ((FIRStorageVoidError)completion)(nil, err); }]);
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

void StorageReferenceInternal::GetFileImpl(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out, SafeFutureHandle<size_t> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
  ReferenceCountedFutureImpl* future_impl = api;
    if (!path || path[0] == '\0') {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorInvalidArgument,
                            "Path cannot be null or empty.");
      return;
    }
    NSURL* local_file_url = ParseLocalPathToFileUrl(path);
    if (local_file_url == nil) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorInvalidArgument,
                            "Unable to convert provided path to valid URL");
      return;
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorUnknown,
                            GetErrorMessage(kErrorUnknown));
      return;
    }

    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }
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

    // Set once the future has been completed. Only read or written with
    // storage_token->mutex held: the completion block and the main-queue
    // block below may run on different threads.
    __block BOOL completed = NO;
    FIRStorageVoidURLError completion = ^(NSURL* /*url*/, NSError* error) {
      if (storage_token) {
        MutexLock lock(storage_token->mutex);
        if (completed) return;
        completed = YES;
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
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      FIRStorageDownloadTask* download_task = nil;
      {
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
        download_task =
            [my_impl writeToFile:local_file_url completion:completion];
        local_controller->AssignTask(alive_storage, download_task);
        SafeAssignController(controller_handle, alive_storage, download_task);
      }
      // Attached after releasing storage_token->mutex; see SafeAttachListener.
      SafeAttachListener(listener_ref, storage_token, download_task);
    });
    return retained_future;
  }
}



void StorageReferenceInternal::GetBytesImpl(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out, SafeFutureHandle<size_t> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
  ReferenceCountedFutureImpl* future_impl = api;
    if (!buffer) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorInvalidArgument,
                            "Buffer cannot be null.");
      return;
    }
    if (buffer_size > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(
          handle, kErrorInvalidArgument,
          "buffer_size exceeds maximum supported download size.");
      return;
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->Complete(handle, kErrorUnknown,
                            GetErrorMessage(kErrorUnknown));
      return;
    }

    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }
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

    // Set once the future has been completed. Only read or written with
    // storage_token->mutex held: the completion block and the main-queue
    // block below may run on different threads.
    __block BOOL completed = NO;
    FIRStorageVoidDataError completion =
        ^(NSData* _Nullable data, NSError* _Nullable error) {
          if (storage_token) {
            MutexLock lock(storage_token->mutex);
            if (completed) return;
            completed = YES;
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
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      FIRStorageDownloadTask* download_task = nil;
      {
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
        download_task =
            [my_impl dataWithMaxSize:max_download_size completion:completion];
        SafeAssignController(controller_handle, alive_storage, download_task);
      }
      // Attached after releasing storage_token->mutex; see SafeAttachListener.
      SafeAttachListener(listener_ref, storage_token, download_task);
    });
    return retained_future;
  }
}



void StorageReferenceInternal::GetDownloadUrlImpl(SafeFutureHandle<std::string> handle, ReferenceCountedFutureImpl* api) {
  Listener* listener = nullptr; Controller* controller_out = nullptr;
  IOS_ASYNC_COMPLETION(std::string, std::string([[((NSURL*)first_arg) absoluteString] UTF8String]), kErrorUnknown,
      [my_impl downloadURLWithCompletion:(FIRStorageVoidURLError)completion]);
}



void StorageReferenceInternal::GetMetadataImpl(SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  Listener* listener = nullptr; Controller* controller_out = nullptr;
  IOS_ASYNC_COMPLETION(Metadata, Metadata(new internal::MetadataInternal((FIRStorageMetadata*)first_arg, storage_)), kErrorUnknown,
      [my_impl metadataWithCompletion:(FIRStorageVoidMetadataError)completion]);
}



void StorageReferenceInternal::UpdateMetadataImpl(const Metadata* metadata, SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  Listener* listener = nullptr; Controller* controller_out = nullptr;
  FIRStorageMetadata* ios_metadata = metadata ? metadata->internal_->impl() : nil;
  IOS_ASYNC_COMPLETION(Metadata, Metadata(new internal::MetadataInternal((FIRStorageMetadata*)first_arg, storage_)), kErrorUnknown,
      [my_impl updateMetadata:ios_metadata completion:(FIRStorageVoidMetadataError)completion]);
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

void StorageReferenceInternal::PutBytesImpl(const void* buffer,
                                                    size_t buffer_size,
                                                    Listener* listener,
                                                    Controller* controller_out, SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  PutBytesImpl(buffer, buffer_size, nullptr, listener, controller_out, handle, api);
  return;
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out, SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
  ReferenceCountedFutureImpl* future_impl = api;
    if (!buffer && buffer_size > 0) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                      "Buffer cannot be null when size > 0.",
                                      Metadata(nullptr));
      return;
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      Metadata(nullptr));
      return;
    }

    FIRStorageMetadata* metadata_impl = nil;
    if (metadata && metadata->is_valid()) {
      metadata_impl = MetadataToFIRStorageMetadata(*metadata->internal_);
    }
    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }
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

    // Set once the future has been completed. Only read or written with
    // storage_token->mutex held: the completion block and the main-queue
    // block below may run on different threads.
    __block BOOL completed = NO;
    FIRStorageVoidMetadataError completion =
        ^(FIRStorageMetadata* resultant_metadata, NSError* error) {
          if (storage_token) {
            MutexLock lock(storage_token->mutex);
            if (completed) return;
            completed = YES;
            if (storage_token->futures_alive) {
              StorageInternal* alive_storage = storage_token->storage;
              Error error_code = NSErrorToErrorCode(error);
              std::string error_string = GetNSErrorMessage(error, error_code);
              if (error == nil && resultant_metadata != nil &&
                  alive_storage != nullptr) {
                FIRStorageReference* meta_ref =
                    resultant_metadata.storageReference ?: my_impl;
                future_impl->CompleteWithResult(
                    handle, kErrorNone, "",
                    Metadata(FIRStorageMetadataToMetadata(
                        resultant_metadata,
                        StorageReference(new StorageReferenceInternal(
                            alive_storage,
                            std::make_unique<FIRStorageReferencePointer>(
                                meta_ref))))));
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
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      FIRStorageUploadTask* upload_task = nil;
      {
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
        upload_task = [my_impl putData:data_payload
                              metadata:metadata_impl
                            completion:completion];
        SafeAssignController(controller_handle, alive_storage, upload_task);
      }
      // Attached after releasing storage_token->mutex; see SafeAttachListener.
      SafeAttachListener(listener_ref, storage_token, upload_task);
    });
    return retained_future;
  }
}



void StorageReferenceInternal::PutFileImpl(const char* path,
                                                   Listener* listener,
                                                   Controller* controller_out, SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  PutFileImpl(path, nullptr, listener, controller_out, handle, api);
  return;
}

void StorageReferenceInternal::PutFileImpl(const char* path,
                                                   const Metadata* metadata,
                                                   Listener* listener,
                                                   Controller* controller_out, SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
  ReferenceCountedFutureImpl* future_impl = api;
    if (!path || path[0] == '\0') {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                      "Path cannot be null or empty.",
                                      Metadata(nullptr));
      return;
    }
    NSURL* local_file_url = ParseLocalPathToFileUrl(path);
    if (local_file_url == nil) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(
          handle, kErrorInvalidArgument,
          "Unable to convert provided path to valid URL", Metadata(nullptr));
      return;
    }
    FIRStorageReference* my_impl = impl();
    if (!my_impl || !storage_) {
      if (controller_out) *controller_out = Controller();
      future_impl->CompleteWithResult(handle, kErrorUnknown,
                                      GetErrorMessage(kErrorUnknown),
                                      Metadata(nullptr));
      return;
    }

    FIRStorageMetadata* metadata_impl = nil;
    if (metadata && metadata->is_valid()) {
      metadata_impl = MetadataToFIRStorageMetadata(*metadata->internal_);
    }
    if (controller_out && controller_out->internal_ == nullptr) {
      // A moved-from Controller has no internal state; allocate one so the
      // caller's Controller still binds to this task.
      controller_out->internal_ = new ControllerInternal();
    }
    if (controller_out && controller_out->internal_) {
      controller_out->internal_->set_pending_valid(true);
    }
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

    // Set once the future has been completed. Only read or written with
    // storage_token->mutex held: the completion block and the main-queue
    // block below may run on different threads.
    __block BOOL completed = NO;
    FIRStorageVoidMetadataError completion =
        ^(FIRStorageMetadata* resultant_metadata, NSError* error) {
          if (storage_token) {
            MutexLock lock(storage_token->mutex);
            if (completed) return;
            completed = YES;
            if (storage_token->futures_alive) {
              StorageInternal* alive_storage = storage_token->storage;
              Error error_code = NSErrorToErrorCode(error);
              std::string error_string = GetNSErrorMessage(error, error_code);
              if (error == nil && resultant_metadata != nil &&
                  alive_storage != nullptr) {
                FIRStorageReference* meta_ref =
                    resultant_metadata.storageReference ?: my_impl;
                future_impl->CompleteWithResult(
                    handle, kErrorNone, "",
                    Metadata(FIRStorageMetadataToMetadata(
                        resultant_metadata,
                        StorageReference(new StorageReferenceInternal(
                            alive_storage,
                            std::make_unique<FIRStorageReferencePointer>(
                                meta_ref))))));
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
      if (!storage_token) {
        SafeResetPendingController(controller_handle);
        return;
      }
      FIRStorageUploadTask* upload_task = nil;
      {
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
        // source file doesn't exist so check for file existence here and abort
        // if it's missing.
        NSError* file_url_exists_error = nil;
        if (![local_file_url checkResourceIsReachableAndReturnError:
                                 &file_url_exists_error]) {
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

        upload_task = [my_impl putFile:local_file_url
                              metadata:metadata_impl
                            completion:completion];
        SafeAssignController(controller_handle, alive_storage, upload_task);
      }
      // Attached after releasing storage_token->mutex; see SafeAttachListener.
      SafeAttachListener(listener_ref, storage_token, upload_task);
    });
    return retained_future;
  }
}



void StorageReferenceInternal::ListImpl(int max_results_per_page, const char* page_token, SafeFutureHandle<StorageListResult> handle, ReferenceCountedFutureImpl* api) {
  Listener* listener = nullptr; Controller* controller_out = nullptr;
  NSString* ios_page_token = page_token ? @(page_token) : nil;
  IOS_ASYNC_COMPLETION(StorageListResult, StorageListResult(new internal::ListResultInternal((FIRStorageListResult*)first_arg, storage_)), kErrorUnknown,
      (ios_page_token ? [my_impl listWithMaxResults:max_results_per_page pageToken:ios_page_token completion:(FIRStorageVoidListError)completion] : [my_impl listWithMaxResults:max_results_per_page completion:(FIRStorageVoidListError)completion]));
}



void StorageReferenceInternal::ListAllImpl(SafeFutureHandle<StorageListResult> handle, ReferenceCountedFutureImpl* api) {
  Listener* listener = nullptr; Controller* controller_out = nullptr;
  IOS_ASYNC_COMPLETION(StorageListResult, StorageListResult(new internal::ListResultInternal((FIRStorageListResult*)first_arg, storage_)), kErrorUnknown,
      [my_impl listAllWithCompletion:(FIRStorageVoidListError)completion]);
}



ReferenceCountedFutureImpl* StorageReferenceInternal::future() const {
  return const_cast<ReferenceCountedFutureImpl*>(&future_api_);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
