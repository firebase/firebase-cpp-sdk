// Copyright 2016 Google LLC

#ifndef FIREBASE_STORAGE_SRC_IOS_STORAGE_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_STORAGE_IOS_H_

#include <functional>
#include <memory>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"

typedef NSString* FIRStorageHandle;
typedef void (^FIRStorageVoidDataError)(NSData* _Nullable, NSError* _Nullable);
typedef void (^FIRStorageVoidError)(NSError* _Nullable);
typedef void (^FIRStorageVoidMetadata)(FIRStorageMetadata* _Nullable);
typedef void (^FIRStorageVoidMetadataError)(FIRStorageMetadata* _Nullable,
                                            NSError* _Nullable);
typedef void (^FIRStorageVoidSnapshot)(FIRStorageTaskSnapshot* _Nonnull);
typedef void (^FIRStorageVoidURLError)(NSURL* _Nullable, NSError* _Nullable);
FOUNDATION_EXPORT NSString* _Nonnull const FIRStorageErrorDomain NS_SWIFT_NAME(
    StorageErrorDomain);
#endif  // __OBJC__

namespace firebase::storage::internal {

OBJ_C_PTR_WRAPPER(FIRStorage);

class StorageInternal {
 public:
  struct LifetimeToken {
    Mutex mutex;
    StorageInternal* _Nullable storage = nullptr;
    bool futures_alive = true;
    explicit LifetimeToken(StorageInternal* _Nullable s)
        : storage(s), futures_alive(s != nullptr) {}

    bool RunWhileAlive(const std::function<void(StorageInternal&)>& fn);
    void UnregisterCleanupObject(void* _Nonnull object);
  };

  StorageInternal(App* _Nullable app, const char* _Nullable url);
  ~StorageInternal();

  StorageInternal(const StorageInternal&) = delete;
  StorageInternal& operator=(const StorageInternal&) = delete;
  StorageInternal(StorageInternal&&) = delete;
  StorageInternal& operator=(StorageInternal&&) = delete;

  App* _Nullable app() const { return app_; }
  const std::string& url() const { return url_; }

  StorageReferenceInternal* _Nullable GetReference();
  StorageReferenceInternal* _Nullable GetReference(const char* _Nullable path);
  StorageReferenceInternal* _Nullable GetReferenceFromUrl(
      const char* _Nullable url);

  double max_download_retry_time() const;
  void set_max_download_retry_time(double max_transfer_retry_seconds);
  double max_upload_retry_time() const;
  void set_max_upload_retry_time(double max_transfer_retry_seconds);
  double max_operation_retry_time() const;
  void set_max_operation_retry_time(double max_transfer_retry_seconds);
  void UseEmulator(const char* _Nullable host, int port);
  bool initialized() const;

  CleanupNotifier& cleanup() { return cleanup_; }
  std::shared_ptr<LifetimeToken> lifetime_token() const {
    return lifetime_token_;
  }

 private:
#ifdef __OBJC__
  FIRStorage* _Nullable impl() const { return impl_ ? impl_->get() : nil; }
#endif
  App* _Nullable app_;
  std::unique_ptr<FIRStoragePointer> impl_;
  std::string url_;
  std::shared_ptr<LifetimeToken> lifetime_token_;
  CleanupNotifier cleanup_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_IOS_STORAGE_IOS_H_
