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

#ifndef FIREBASE_STORAGE_SRC_IOS_STORAGE_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_STORAGE_IOS_H_

#include <functional>
#include <memory>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

#ifdef __OBJC__

#import "FirebaseStorage-Swift.h"

// Some missing typedefs not included in Swift header.
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

namespace firebase {
namespace storage {
namespace internal {

// This defines the class FIRStoragePointer, which is a C++-compatible wrapper
// around the FIRStorage Obj-C class.
OBJ_C_PTR_WRAPPER(FIRStorage);

class StorageInternal {
 public:
  struct LifetimeToken {
    Mutex mutex;
    StorageInternal* _Nullable storage = nullptr;
    bool futures_alive = true;
    explicit LifetimeToken(StorageInternal* _Nullable s)
        : storage(s), futures_alive(s != nullptr) {}

    // Runs `fn(*storage)` with `mutex` held if the StorageInternal is still
    // alive, and returns whether it ran.
    //
    // ~StorageInternal() clears `storage` under `mutex` before it runs any
    // cleanup callback (and cleanup callbacks run nowhere else), so inside
    // `fn` no cleanup callback is in flight and the StorageInternal cannot be
    // freed: storage.cleanup().UnregisterObject() never blocks and never
    // touches a dead CleanupNotifier. Owners must therefore unregister through
    // this method (or UnregisterCleanupObject()) rather than through a cached
    // raw StorageInternal pointer.
    //
    // Lock order: `mutex` is acquired before owner locks (e.g.
    // ControllerInternal::LifetimeHandle::mutex, ListenerInternal::mutex_), so
    // `fn` may acquire such a lock, but this must never be called while one is
    // held. Two different tokens' mutexes are only ever nested on the main
    // queue (a main-queue block holding the new StorageInternal's token rebinds
    // an owner away from an old one), so that nesting cannot deadlock.
    bool RunWhileAlive(const std::function<void(StorageInternal&)>& fn);

    // Unregisters `object` from storage->cleanup() if the StorageInternal is
    // still alive; otherwise does nothing. See RunWhileAlive().
    void UnregisterCleanupObject(void* _Nonnull object);
  };

  // `app` may be null, in which case initialized() returns false.
  StorageInternal(App* _Nullable app, const char* _Nullable url);

  ~StorageInternal();

  StorageInternal(const StorageInternal&) = delete;
  StorageInternal& operator=(const StorageInternal&) = delete;
  StorageInternal(StorageInternal&&) = delete;
  StorageInternal& operator=(StorageInternal&&) = delete;

  // Get the firebase::App that this Storage was created with.
  App* _Nullable app() const;

  // Return the URL we were created with.
  const std::string& url() const { return url_; }

  // Get a StorageReference to the root of the database.
  StorageReferenceInternal* _Nullable GetReference();

  // Get a StorageReference for the specified path.
  StorageReferenceInternal* _Nullable GetReference(const char* _Nullable path);

  // Get a StorageReference for the provided URL.
  StorageReferenceInternal* _Nullable GetReferenceFromUrl(
      const char* _Nullable url);

  // Returns the maximum time in seconds to retry a download if a failure
  // occurs.
  double max_download_retry_time() const;
  // Sets the maximum time to retry a download if a failure occurs.
  void set_max_download_retry_time(double max_transfer_retry_seconds);

  // Returns the maximum time to retry an upload if a failure occurs.
  double max_upload_retry_time() const;
  // Sets the maximum time to retry an upload if a failure occurs.
  void set_max_upload_retry_time(double max_transfer_retry_seconds);

  // Returns the maximum time to retry operations other than upload and
  // download if a failure occurs.
  double max_operation_retry_time() const;
  // Sets the maximum time to retry operations other than upload and download
  // if a failure occurs.
  void set_max_operation_retry_time(double max_transfer_retry_seconds);

  // Configures the Storage SDK to use an emulated backend instead of call the
  // default remote backend
  void UseEmulator(const char* _Nullable host, int port);

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const;

  // When this is deleted, it will clean up all StorageReferences and other
  // objects.
  CleanupNotifier& cleanup() { return cleanup_; }

  std::shared_ptr<LifetimeToken> lifetime_token() const {
    return lifetime_token_;
  }

 private:
#ifdef __OBJC__
  FIRStorage* _Nullable impl() const { return impl_ ? impl_->get() : nil; }
#endif
  // The firebase::App that this Storage was created with.
  App* _Nullable app_;

  // Object lifetime managed by Objective C ARC.
  std::unique_ptr<FIRStoragePointer> impl_;

  FutureManager future_manager_;

  std::string url_;

  CleanupNotifier cleanup_;

  std::shared_ptr<LifetimeToken> lifetime_token_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_STORAGE_IOS_H_
