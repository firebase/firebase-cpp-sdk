// Copyright 2016 Google LLC

#include "storage/src/ios/storage_ios.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#import "FirebaseStorage-Swift.h"
#include "app/src/app_ios.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "storage/src/ios/storage_reference_ios.h"
#include "storage/src/ios/util_ios.h"

namespace firebase::storage::internal {

StorageInternal::StorageInternal(App* _Nullable app, const char* _Nullable url)
    : app_(app),
      impl_(std::make_unique<FIRStoragePointer>(nil)),
      url_(url ? url : ""),
      lifetime_token_(std::make_shared<LifetimeToken>(this)) {
  if (!app_) return;
  @autoreleasepool {
    FIRApp* platform_app = app_->GetPlatformApp();
    if (!platform_app) return;
    @try {
      if (url_.empty()) {
        impl_ = std::make_unique<FIRStoragePointer>(
            [FIRStorage storageForApp:platform_app]);
      } else {
        NSString* ns_url = [NSString stringWithUTF8String:url_.c_str()];
        if (!ns_url) {
          LogWarning("Failed to create storage instance: URL is not valid UTF-8.");
          return;
        }
        impl_ = std::make_unique<FIRStoragePointer>(
            [FIRStorage storageForApp:platform_app URL:ns_url]);
      }
    } @catch (NSException* exception) {
      LogWarning("Failed to create storage instance for bucket URL '%s'. %s",
                 url_.c_str(), GetNSExceptionReason(exception).c_str());
      return;
    }
  }
  if (impl()) {
    cleanup_.RegisterObject(this, [](void* obj) {
      auto* self = static_cast<StorageInternal*>(obj);
      if (self->lifetime_token_) {
        MutexLock lock(self->lifetime_token_->mutex);
        self->lifetime_token_->storage = nullptr;
      }
    });
  }
}

StorageInternal::~StorageInternal() {
  if (lifetime_token_) {
    MutexLock lock(lifetime_token_->mutex);
    lifetime_token_->storage = nullptr;
  }
  cleanup_.CleanupAll();
  if (lifetime_token_) {
    MutexLock lock(lifetime_token_->mutex);
    lifetime_token_->futures_alive = false;
  }
}

bool StorageInternal::LifetimeToken::RunWhileAlive(
    const std::function<void(StorageInternal&)>& fn) {
  MutexLock lock(mutex);
  if (!storage) return false;
  fn(*storage);
  return true;
}

void StorageInternal::LifetimeToken::UnregisterCleanupObject(
    void* _Nonnull object) {
  RunWhileAlive([object](StorageInternal& alive_storage) {
    alive_storage.cleanup().UnregisterObject(object);
  });
}

StorageReferenceInternal* _Nullable StorageInternal::GetReference() {
  if (!impl()) return nullptr;
  @autoreleasepool {
    FIRStorageReference* reference = nil;
    @try {
      reference = impl().reference;
    } @catch (NSException* exception) {
      LogWarning("Unable to get root storage reference. %s",
                 GetNSExceptionReason(exception).c_str());
      return nullptr;
    }
    return reference ? new StorageReferenceInternal(
                           this, std::make_unique<FIRStorageReferencePointer>(
                                     reference))
                     : nullptr;
  }
}

StorageReferenceInternal* _Nullable StorageInternal::GetReference(
    const char* _Nullable path) {
  if (!impl()) return nullptr;
  if (!path || path[0] == '\0') return GetReference();
  @autoreleasepool {
    NSString* ns_path = [NSString stringWithUTF8String:path];
    if (!ns_path) {
      LogWarning("Unable to get storage reference: path is not valid UTF-8.");
      return nullptr;
    }
    FIRStorageReference* reference = nil;
    @try {
      reference = [impl() referenceWithPath:ns_path];
    } @catch (NSException* exception) {
      LogWarning("Unable to get storage reference at path '%s'. %s", path,
                 GetNSExceptionReason(exception).c_str());
      return nullptr;
    }
    return reference ? new StorageReferenceInternal(
                           this, std::make_unique<FIRStorageReferencePointer>(
                                     reference))
                     : nullptr;
  }
}

StorageReferenceInternal* _Nullable StorageInternal::GetReferenceFromUrl(
    const char* _Nullable url) {
  if (!impl() || !url || url[0] == '\0') return nullptr;
  @autoreleasepool {
    NSString* ns_url = [NSString stringWithUTF8String:url];
    if (!ns_url) {
      LogWarning("Unable to get storage reference: URL is not valid UTF-8.");
      return nullptr;
    }
    FIRStorageReference* reference = nil;
    @try {
      reference = [impl() referenceForURL:ns_url];
    } @catch (NSException* exception) {
      LogWarning("Unable to get storage reference at URL '%s'. %s", url,
                 GetNSExceptionReason(exception).c_str());
      return nullptr;
    }
    return reference ? new StorageReferenceInternal(
                           this, std::make_unique<FIRStorageReferencePointer>(
                                     reference))
                     : nullptr;
  }
}

double StorageInternal::max_download_retry_time() const {
  return impl() ? impl().maxDownloadRetryTime : 0.0;
}
void StorageInternal::set_max_download_retry_time(double s) {
  if (impl()) impl().maxDownloadRetryTime = s;
}
double StorageInternal::max_upload_retry_time() const {
  return impl() ? impl().maxUploadRetryTime : 0.0;
}
void StorageInternal::set_max_upload_retry_time(double s) {
  if (impl()) impl().maxUploadRetryTime = s;
}
double StorageInternal::max_operation_retry_time() const {
  return impl() ? impl().maxOperationRetryTime : 0.0;
}
void StorageInternal::set_max_operation_retry_time(double s) {
  if (impl()) impl().maxOperationRetryTime = s;
}

void StorageInternal::UseEmulator(const char* _Nullable host, int port) {
  if (!impl()) return;
  if (!host || host[0] == '\0') {
    LogError("Emulator host cannot be null or empty.");
    return;
  }
  if (port <= 0) {
    LogError("Emulator port must be a positive number.");
    return;
  }
  @autoreleasepool {
    NSString* hostString = [NSString stringWithUTF8String:host];
    if (!hostString) {
      LogError("Emulator host is not valid UTF-8.");
      return;
    }
    @try {
      [impl() useEmulatorWithHost:hostString port:port];
    } @catch (NSException* exception) {
      LogError("Failed to configure Storage emulator (%s:%d): %s", host, port,
               GetNSExceptionReason(exception).c_str());
    }
  }
}

bool StorageInternal::initialized() const { return impl() != nil; }

}  // namespace firebase::storage::internal
