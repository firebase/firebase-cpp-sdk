// Copyright 2016 Google LLC

#include "storage/src/ios/storage_reference_ios.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#import "FirebaseStorage-Swift.h"
#include "app/src/util_ios.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/common/metadata_internal.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/ios/controller_ios.h"
#include "storage/src/ios/listener_ios.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/util_ios.h"

namespace firebase::storage::internal {

FIRStorageMetadata* MetadataToFIRStorageMetadata(const MetadataInternal& meta) {
  FIRStorageMetadata* fir_meta = [[FIRStorageMetadata alloc] init];
  auto set_if_nonempty = [](const char* val, void (^setter)(NSString*)) {
    if (val && val[0] != '\0') setter(@(val));
  };
  set_if_nonempty(meta.cache_control(),
                  ^(NSString* s) { fir_meta.cacheControl = s; });
  set_if_nonempty(meta.content_disposition(),
                  ^(NSString* s) { fir_meta.contentDisposition = s; });
  set_if_nonempty(meta.content_encoding(),
                  ^(NSString* s) { fir_meta.contentEncoding = s; });
  set_if_nonempty(meta.content_language(),
                  ^(NSString* s) { fir_meta.contentLanguage = s; });
  set_if_nonempty(meta.content_type(),
                  ^(NSString* s) { fir_meta.contentType = s; });
  if (meta.custom_metadata() && !meta.custom_metadata()->empty()) {
    NSMutableDictionary<NSString*, NSString*>* dict =
        [[NSMutableDictionary alloc] init];
    for (const auto& [k, v] : *meta.custom_metadata()) {
      dict[@(k.c_str())] = @(v.c_str());
    }
    fir_meta.customMetadata = dict;
  }
  return fir_meta;
}

MetadataInternal* FIRStorageMetadataToMetadata(FIRStorageMetadata* fir_meta,
                                               StorageReference ref) {
  auto* meta = new MetadataInternal(ref);
  if (fir_meta.bucket) meta->set_bucket(fir_meta.bucket.UTF8String);
  if (fir_meta.cacheControl) {
    meta->set_cache_control(fir_meta.cacheControl.UTF8String);
  }
  if (fir_meta.contentDisposition) {
    meta->set_content_disposition(fir_meta.contentDisposition.UTF8String);
  }
  if (fir_meta.contentEncoding) {
    meta->set_content_encoding(fir_meta.contentEncoding.UTF8String);
  }
  if (fir_meta.contentLanguage) {
    meta->set_content_language(fir_meta.contentLanguage.UTF8String);
  }
  if (fir_meta.contentType) {
    meta->set_content_type(fir_meta.contentType.UTF8String);
  }
  if (fir_meta.name) meta->set_name(fir_meta.name.UTF8String);
  if (fir_meta.path) meta->set_path(fir_meta.path.UTF8String);
  meta->set_size_bytes(fir_meta.size);
  if (fir_meta.timeCreated) {
    meta->set_creation_time(
        static_cast<int64_t>([fir_meta.timeCreated timeIntervalSince1970] * 1000));
  }
  if (fir_meta.updated) {
    meta->set_updated_time(
        static_cast<int64_t>([fir_meta.updated timeIntervalSince1970] * 1000));
  }
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

NSURL* _Nullable ParseLocalPathToFileUrl(const char* _Nullable path) {
  @autoreleasepool {
    if (!path || path[0] == '\0') return nil;
    NSString* path_string = [NSString stringWithUTF8String:path];
    if (!path_string || path_string.length == 0) return nil;
    return [path_string hasPrefix:@"file://"]
               ? [NSURL URLWithString:path_string]
               : [NSURL fileURLWithPath:path_string];
  }
}

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
  if (handle) ControllerInternal::LifetimeHandle::AssignTask(handle, storage, task);
}

void SafeResetPendingController(
    const std::shared_ptr<ControllerInternal::LifetimeHandle>& handle) {
  if (!handle) return;
  std::lock_guard<std::recursive_mutex> lock(handle->mutex);
  handle->pending_valid = false;
  handle->pending_cancel = false;
  handle->pending_pause = false;
}

template <typename T>
void CompleteFutureError(ReferenceCountedFutureImpl* api,
                         SafeFutureHandle<T> handle, Error code,
                         const char* msg) {
  if constexpr (std::is_same_v<T, Metadata>) {
    api->CompleteWithResult(handle, code, msg, Metadata(nullptr));
  } else if constexpr (std::is_same_v<T, StorageListResult>) {
    api->CompleteWithResult(handle, code, msg, StorageListResult());
  } else {
    api->Complete(handle, code, msg);
  }
}

template <typename T>
void CompleteFromNSError(ReferenceCountedFutureImpl* api,
                         SafeFutureHandle<T> handle, NSError* _Nullable error,
                         StorageInternal* _Nullable alive_storage) {
  Error code = NSErrorToErrorCode(error);
  std::string msg = GetNSErrorMessage(error, code);
  Error fallback = (code != kErrorNone)
                       ? code
                       : (alive_storage ? kErrorUnknown : kErrorCancelled);
  CompleteFutureError(api, handle, fallback,
                      error ? msg.c_str() : GetErrorMessage(fallback));
}

template <typename T, typename StartBlock>
void DispatchSimpleOp(StorageReferenceInternal* ref,
                      SafeFutureHandle<T> handle,
                      ReferenceCountedFutureImpl* api, StartBlock start_block) {
  FIRStorageReference* my_impl = ref->impl();
  StorageInternal* storage = ref->storage_internal();
  if (!my_impl || !storage) {
    CompleteFutureError(api, handle, kErrorUnknown,
                        GetErrorMessage(kErrorUnknown));
    return;
  }
  auto storage_token = storage->lifetime_token();
  __block BOOL completed = NO;
  auto finish = ^(void (^on_alive)(StorageInternal* _Nonnull),
                  NSError* _Nullable error) {
    if (!storage_token) return;
    MutexLock lock(storage_token->mutex);
    if (completed) return;
    completed = YES;
    if (!storage_token->futures_alive) return;
    StorageInternal* alive = storage_token->storage;
    if (!error && alive && on_alive) {
      on_alive(alive);
    } else {
      CompleteFromNSError(api, handle, error, alive);
    }
  };
  util::DispatchAsyncSafeMainQueue(^() {
    if (!storage_token) return;
    MutexLock lock(storage_token->mutex);
    if (!storage_token->futures_alive) return;
    if (!storage_token->storage) {
      if (!completed) {
        completed = YES;
        CompleteFutureError(api, handle, kErrorCancelled,
                            GetErrorMessage(kErrorCancelled));
      }
      return;
    }
    start_block(my_impl, finish);
  });
}

template <typename T, typename StartBlock>
void DispatchTaskOp(StorageReferenceInternal* ref, Listener* _Nullable listener,
                    Controller* _Nullable controller_out,
                    SafeFutureHandle<T> handle, ReferenceCountedFutureImpl* api,
                    StartBlock start_block) {
  FIRStorageReference* my_impl = ref->impl();
  StorageInternal* storage = ref->storage_internal();
  if (!my_impl || !storage) {
    if (controller_out) *controller_out = Controller();
    CompleteFutureError(api, handle, kErrorUnknown,
                        GetErrorMessage(kErrorUnknown));
    return;
  }
  if (controller_out && !controller_out->internal_) {
    controller_out->internal_ = new ControllerInternal();
  }
  if (controller_out && controller_out->internal_) {
    controller_out->internal_->set_pending_valid(true);
  }
  auto listener_ref =
      (listener && listener->impl_)
          ? listener->impl_->safe_reference()
          : firebase::util::SafeReference<ListenerInternal>();
  auto controller_handle =
      (controller_out && controller_out->internal_)
          ? controller_out->internal_->lifetime_handle()
          : nullptr;
  auto storage_token = storage->lifetime_token();
  __block BOOL completed = NO;
  auto finish = ^(void (^on_alive)(StorageInternal* _Nonnull),
                  NSError* _Nullable error) {
    if (storage_token) {
      MutexLock lock(storage_token->mutex);
      if (!completed) {
        completed = YES;
        if (storage_token->futures_alive) {
          StorageInternal* alive = storage_token->storage;
          if (!error && alive && on_alive) {
            on_alive(alive);
          } else {
            CompleteFromNSError(api, handle, error, alive);
          }
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
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = nil;
    {
      MutexLock lock(storage_token->mutex);
      StorageInternal* alive = storage_token->storage;
      if (!alive) {
        SafeResetPendingController(controller_handle);
        if (storage_token->futures_alive && !completed) {
          completed = YES;
          CompleteFutureError(api, handle, kErrorCancelled,
                              GetErrorMessage(kErrorCancelled));
        }
        return;
      }
      task = start_block(my_impl, alive, finish, &completed, controller_handle);
      if (!task) return;
      SafeAssignController(controller_handle, alive, task);
    }
    SafeAttachListener(listener_ref, storage_token, task);
  });
}

}  // namespace

StorageReferenceInternal::StorageReferenceInternal(
    StorageInternal* storage, std::unique_ptr<FIRStorageReferencePointer> impl)
    : storage_(storage), impl_(std::move(impl)) {}

StorageReferenceInternal::~StorageReferenceInternal() = default;

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& other)
    : storage_(other.storage_),
      impl_(std::make_unique<FIRStorageReferencePointer>(other.impl())) {}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& other) {
  if (this != &other) {
    storage_ = other.storage_;
    impl_ = std::make_unique<FIRStorageReferencePointer>(other.impl());
  }
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& other) noexcept
    : storage_(std::exchange(other.storage_, nullptr)),
      impl_(std::move(other.impl_)) {}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& other) noexcept {
  if (this != &other) {
    storage_ = std::exchange(other.storage_, nullptr);
    impl_ = std::move(other.impl_);
  }
  return *this;
}

Storage* StorageReferenceInternal::storage() const {
  return storage_ ? Storage::GetInstance(
                        storage_->app(),
                        storage_->url().empty() ? nullptr : storage_->url().c_str())
                  : nullptr;
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  @autoreleasepool {
    if (!storage_ || !impl() || !path) return nullptr;
    NSString* child_path = [NSString stringWithUTF8String:path];
    if (!child_path) return nullptr;
    FIRStorageReference* child_ref = nil;
    @try {
      child_ref = [impl() child:child_path];
    } @catch (NSException*) {
      return nullptr;
    }
    return child_ref ? new StorageReferenceInternal(
                           storage_, std::make_unique<FIRStorageReferencePointer>(
                                         child_ref))
                     : nullptr;
  }
}

void StorageReferenceInternal::DeleteImpl(SafeFutureHandle<void> handle,
                                          ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    DispatchSimpleOp(this, handle, api, ^(FIRStorageReference* my_impl, auto finish) {
      [my_impl deleteWithCompletion:^(NSError* error) {
        finish(^(StorageInternal*) { api->Complete(handle, kErrorNone, ""); },
               error);
      }];
    });
  }
}

std::string StorageReferenceInternal::bucket() const {
  @autoreleasepool {
    NSString* s = impl() ? impl().bucket : nil;
    const char* c = s ? s.UTF8String : nullptr;
    return c ? std::string(c) : std::string();
  }
}

std::string StorageReferenceInternal::full_path() const {
  @autoreleasepool {
    NSString* s = impl() ? impl().fullPath : nil;
    const char* fp = s ? s.UTF8String : nullptr;
    if (!fp || fp[0] == '\0') return "/";
    return fp[0] == '/' ? std::string(fp) : (std::string("/") + fp);
  }
}

std::string StorageReferenceInternal::name() const {
  @autoreleasepool {
    NSString* s = impl() ? impl().name : nil;
    const char* c = s ? s.UTF8String : nullptr;
    return c ? std::string(c) : std::string();
  }
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() const {
  @autoreleasepool {
    if (!storage_ || !impl()) return nullptr;
    FIRStorageReference* parent_ref = impl().parent;
    return parent_ref ? new StorageReferenceInternal(
                            storage_, std::make_unique<FIRStorageReferencePointer>(
                                          parent_ref))
                      : nullptr;
  }
}

void StorageReferenceInternal::GetFileImpl(const char* path, Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<size_t> handle,
                                           ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    if (!path || path[0] == '\0') {
      if (controller_out) *controller_out = Controller();
      api->Complete(handle, kErrorInvalidArgument,
                    "Path cannot be null or empty.");
      return;
    }
    NSURL* local_file_url = ParseLocalPathToFileUrl(path);
    if (!local_file_url) {
      if (controller_out) *controller_out = Controller();
      api->Complete(handle, kErrorInvalidArgument,
                    "Unable to convert provided path to valid URL");
      return;
    }
    auto local_controller = std::make_shared<ControllerInternal>();
    DispatchTaskOp(
        this, listener, controller_out, handle, api,
        ^(FIRStorageReference* my_impl, StorageInternal* alive, auto finish,
          BOOL*, const std::shared_ptr<ControllerInternal::LifetimeHandle>&)
            -> FIRStorageObservableTask<FIRStorageTaskManagement>* {
          FIRStorageDownloadTask* task = [my_impl
              writeToFile:local_file_url
               completion:^(NSURL*, NSError* error) {
                 finish(
                     ^(StorageInternal*) {
                       api->CompleteWithResult(
                           handle, kErrorNone, "",
                           static_cast<size_t>(
                               local_controller->bytes_transferred()));
                     },
                     error);
               }];
          local_controller->AssignTask(alive, task);
          return task;
        });
  }
}

void StorageReferenceInternal::GetBytesImpl(void* buffer, size_t buffer_size,
                                            Listener* listener,
                                            Controller* controller_out,
                                            SafeFutureHandle<size_t> handle,
                                            ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    if (!buffer) {
      if (controller_out) *controller_out = Controller();
      api->Complete(handle, kErrorInvalidArgument, "Buffer cannot be null.");
      return;
    }
    if (buffer_size > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
      if (controller_out) *controller_out = Controller();
      api->Complete(handle, kErrorInvalidArgument,
                    "buffer_size exceeds maximum supported download size.");
      return;
    }
    const int64_t max_size = static_cast<int64_t>(buffer_size);
    DispatchTaskOp(
        this, listener, controller_out, handle, api,
        ^(FIRStorageReference* my_impl, StorageInternal*, auto finish, BOOL*,
          const std::shared_ptr<ControllerInternal::LifetimeHandle>&)
            -> FIRStorageObservableTask<FIRStorageTaskManagement>* {
          return [my_impl
              dataWithMaxSize:max_size
                   completion:^(NSData* _Nullable data, NSError* _Nullable error) {
                     finish(
                         data ? ^(StorageInternal*) {
                           if (data.length > buffer_size) {
                             api->Complete(
                                 handle, kErrorDownloadSizeExceeded,
                                 GetErrorMessage(kErrorDownloadSizeExceeded));
                           } else {
                             if (data.length > 0 && data.bytes) {
                               std::memcpy(buffer, data.bytes, data.length);
                             }
                             api->CompleteWithResult(
                                 handle, kErrorNone, "",
                                 static_cast<size_t>(data.length));
                           }
                         }
                              : (void (^)(StorageInternal*))nil,
                         error);
                   }];
        });
  }
}

void StorageReferenceInternal::GetDownloadUrlImpl(
    SafeFutureHandle<std::string> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    DispatchSimpleOp(
        this, handle, api, ^(FIRStorageReference* my_impl, auto finish) {
          [my_impl downloadURLWithCompletion:^(NSURL* url, NSError* error) {
            NSString* url_str = url ? url.absoluteString : nil;
            const char* url_cstr = url_str ? url_str.UTF8String : nullptr;
            finish(
                url_cstr ? ^(StorageInternal*) {
                  api->CompleteWithResult(handle, kErrorNone, "",
                                          std::string(url_cstr));
                }
                         : (void (^)(StorageInternal*))nil,
                error);
          }];
        });
  }
}

void StorageReferenceInternal::GetMetadataImpl(
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    DispatchSimpleOp(
        this, handle, api, ^(FIRStorageReference* my_impl, auto finish) {
          [my_impl metadataWithCompletion:^(FIRStorageMetadata* meta,
                                            NSError* error) {
            finish(
                meta ? ^(StorageInternal* alive) {
                  FIRStorageReference* r = meta.storageReference ?: my_impl;
                  api->CompleteWithResult(
                      handle, kErrorNone, "",
                      Metadata(FIRStorageMetadataToMetadata(
                          meta, StorageReference(new StorageReferenceInternal(
                                    alive,
                                    std::make_unique<FIRStorageReferencePointer>(
                                        r))))));
                }
                     : (void (^)(StorageInternal*))nil,
                error);
          }];
        });
  }
}

void StorageReferenceInternal::UpdateMetadataImpl(
    const Metadata* metadata, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    FIRStorageMetadata* ios_meta =
        (metadata && metadata->is_valid())
            ? MetadataToFIRStorageMetadata(*metadata->internal_)
            : nil;
    DispatchSimpleOp(
        this, handle, api, ^(FIRStorageReference* my_impl, auto finish) {
          [my_impl updateMetadata:ios_meta
                       completion:^(FIRStorageMetadata* meta, NSError* error) {
                         finish(
                             meta ? ^(StorageInternal* alive) {
                               FIRStorageReference* r =
                                   meta.storageReference ?: my_impl;
                               api->CompleteWithResult(
                                   handle, kErrorNone, "",
                                   Metadata(FIRStorageMetadataToMetadata(
                                       meta,
                                       StorageReference(
                                           new StorageReferenceInternal(
                                               alive,
                                               std::make_unique<
                                                   FIRStorageReferencePointer>(
                                                   r))))));
                             }
                                  : (void (^)(StorageInternal*))nil,
                             error);
                       }];
        });
  }
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl* api) {
  PutBytesImpl(buffer, buffer_size, nullptr, listener, controller_out, handle,
               api);
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out,
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    if (!buffer && buffer_size > 0) {
      if (controller_out) *controller_out = Controller();
      api->CompleteWithResult(handle, kErrorInvalidArgument,
                              "Buffer cannot be null when size > 0.",
                              Metadata(nullptr));
      return;
    }
    FIRStorageMetadata* meta_impl =
        (metadata && metadata->is_valid())
            ? MetadataToFIRStorageMetadata(*metadata->internal_)
            : nil;
    NSData* data_payload =
        (buffer && buffer_size > 0)
            ? [NSData dataWithBytes:buffer length:buffer_size]
            : [NSData data];
    DispatchTaskOp(
        this, listener, controller_out, handle, api,
        ^(FIRStorageReference* my_impl, StorageInternal*, auto finish, BOOL*,
          const std::shared_ptr<ControllerInternal::LifetimeHandle>&)
            -> FIRStorageObservableTask<FIRStorageTaskManagement>* {
          return [my_impl
                 putData:data_payload
                metadata:meta_impl
              completion:^(FIRStorageMetadata* res_meta, NSError* error) {
                finish(
                    res_meta ? ^(StorageInternal* alive) {
                      FIRStorageReference* r =
                          res_meta.storageReference ?: my_impl;
                      api->CompleteWithResult(
                          handle, kErrorNone, "",
                          Metadata(FIRStorageMetadataToMetadata(
                              res_meta,
                              StorageReference(new StorageReferenceInternal(
                                  alive,
                                  std::make_unique<FIRStorageReferencePointer>(
                                      r))))));
                    }
                             : (void (^)(StorageInternal*))nil,
                    error);
              }];
        });
  }
}

void StorageReferenceInternal::PutFileImpl(const char* path, Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<Metadata> handle,
                                           ReferenceCountedFutureImpl* api) {
  PutFileImpl(path, nullptr, listener, controller_out, handle, api);
}

void StorageReferenceInternal::PutFileImpl(
    const char* path, const Metadata* metadata, Listener* listener,
    Controller* controller_out, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    if (!path || path[0] == '\0') {
      if (controller_out) *controller_out = Controller();
      api->CompleteWithResult(handle, kErrorInvalidArgument,
                              "Path cannot be null or empty.",
                              Metadata(nullptr));
      return;
    }
    NSURL* local_file_url = ParseLocalPathToFileUrl(path);
    if (!local_file_url) {
      if (controller_out) *controller_out = Controller();
      api->CompleteWithResult(
          handle, kErrorInvalidArgument,
          "Unable to convert provided path to valid URL", Metadata(nullptr));
      return;
    }
    FIRStorageMetadata* meta_impl =
        (metadata && metadata->is_valid())
            ? MetadataToFIRStorageMetadata(*metadata->internal_)
            : nil;
    DispatchTaskOp(
        this, listener, controller_out, handle, api,
        ^(FIRStorageReference* my_impl, StorageInternal* alive, auto finish,
          BOOL* completed,
          const std::shared_ptr<ControllerInternal::LifetimeHandle>& ctrl_handle)
            -> FIRStorageObservableTask<FIRStorageTaskManagement>* {
          NSError* exists_err = nil;
          if (![local_file_url
                  checkResourceIsReachableAndReturnError:&exists_err]) {
            SafeResetPendingController(ctrl_handle);
            if (alive->lifetime_token()->futures_alive && !*completed) {
              *completed = YES;
              api->CompleteWithResult(handle, kErrorObjectNotFound,
                                      GetErrorMessage(kErrorObjectNotFound),
                                      Metadata(nullptr));
            }
            return nil;
          }
          return [my_impl
                 putFile:local_file_url
                metadata:meta_impl
              completion:^(FIRStorageMetadata* res_meta, NSError* error) {
                finish(
                    res_meta ? ^(StorageInternal* alive_s) {
                      FIRStorageReference* r =
                          res_meta.storageReference ?: my_impl;
                      api->CompleteWithResult(
                          handle, kErrorNone, "",
                          Metadata(FIRStorageMetadataToMetadata(
                              res_meta,
                              StorageReference(new StorageReferenceInternal(
                                  alive_s,
                                  std::make_unique<FIRStorageReferencePointer>(
                                      r))))));
                    }
                             : (void (^)(StorageInternal*))nil,
                    error);
              }];
        });
  }
}

namespace {

void CompleteListResult(ReferenceCountedFutureImpl* api,
                        SafeFutureHandle<StorageListResult> handle,
                        FIRStorageListResult* result,
                        StorageInternal* alive_storage) {
  std::vector<StorageReference> prefixes, items;
  auto process_refs = [&](NSArray<FIRStorageReference*>* refs,
                          std::vector<StorageReference>& out) {
    for (FIRStorageReference* ref in refs) {
      out.push_back(StorageReference(new StorageReferenceInternal(
          alive_storage, std::make_unique<FIRStorageReferencePointer>(ref))));
    }
  };
  process_refs(result.prefixes, prefixes);
  process_refs(result.items, items);
  NSString* tok = result.pageToken;
  const char* tok_cstr = (tok && tok.UTF8String) ? tok.UTF8String : "";
  api->CompleteWithResult(
      handle, kErrorNone, "",
      StorageListResultInternal::AsStorageListResult(
          new StorageListResultInternal(std::move(prefixes), std::move(items),
                                        tok_cstr)));
}

}  // namespace

void StorageReferenceInternal::ListImpl(
    int max_results_per_page, const char* page_token,
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    if (max_results_per_page <= 0 || max_results_per_page > 1000) {
      api->CompleteWithResult(
          handle, kErrorInvalidArgument,
          "max_results_per_page must be between 1 and 1000 inclusive.",
          StorageListResult());
      return;
    }
    NSString* page_token_ns = nil;
    if (page_token && page_token[0] != '\0') {
      page_token_ns = [NSString stringWithUTF8String:page_token];
      if (!page_token_ns) {
        api->CompleteWithResult(handle, kErrorInvalidArgument,
                                "page_token is not valid UTF-8.",
                                StorageListResult());
        return;
      }
    }
    const int64_t max_results = static_cast<int64_t>(max_results_per_page);
    DispatchSimpleOp(
        this, handle, api, ^(FIRStorageReference* my_impl, auto finish) {
          auto cb = ^(FIRStorageListResult* _Nullable res,
                      NSError* _Nullable error) {
            finish(
                res ? ^(StorageInternal* alive) {
                  CompleteListResult(api, handle, res, alive);
                }
                    : (void (^)(StorageInternal*))nil,
                error);
          };
          if (page_token_ns) {
            [my_impl listWithMaxResults:max_results
                              pageToken:page_token_ns
                             completion:cb];
          } else {
            [my_impl listWithMaxResults:max_results completion:cb];
          }
        });
  }
}

void StorageReferenceInternal::ListAllImpl(
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* api) {
  @autoreleasepool {
    DispatchSimpleOp(
        this, handle, api, ^(FIRStorageReference* my_impl, auto finish) {
          [my_impl listAllWithCompletion:^(FIRStorageListResult* _Nullable res,
                                           NSError* _Nullable error) {
            finish(
                res ? ^(StorageInternal* alive) {
                  CompleteListResult(api, handle, res, alive);
                }
                    : (void (^)(StorageInternal*))nil,
                error);
          }];
        });
  }
}

}  // namespace firebase::storage::internal
