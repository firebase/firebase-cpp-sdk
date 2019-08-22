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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_STORAGE_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_STORAGE_IOS_H_

#include <map>
#include <set>

#include <dispatch/dispatch.h>

#include "app/memory/unique_ptr.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

#ifdef __OBJC__
#import "FIRStorage.h"
#import "GTMSessionFetcher/GTMSessionFetcher.h"
#import "GTMSessionFetcher/GTMSessionFetcherService.h"

// GTMSessionFetcherService implementation that yields a
// FIRCPPGTMSessionFetcher class rather than the default implementation.
// This makes it possible to customize the behavior of
// GTMSessionFetcher before a fetch operation is started.
@interface FIRCPPGTMSessionFetcherService : GTMSessionFetcherService

// If set, assigned to FIRCPPGTMSessionFetcher.accumulateDataBlock on
// FIRCPPGTMSessionFetcher:beginFetchWithCompletionHandler.
// This allows the handler of this event to stream received into a buffer or
// to the application.
@property(atomic, copy, GTM_NULLABLE) GTMSessionFetcherAccumulateDataBlock accumulateDataBlock;

// Returns FIRCPPGTMSessionFetch as the fetcher class so that it's possible to
// override properties on instances created from the class before the fetch
// operation is started.
- (id _Nonnull)fetcherWithRequest:(NSURLRequest* _Nonnull)request
                     fetcherClass:(Class _Nonnull)fetcherClass;
@end

// GTMSessionFetcher implementation that streams via a C/C++ callback.
// See FIRCPPGTMSessionFetcherService.accumulateDataBlock.
@interface FIRCPPGTMSessionFetcher : GTMSessionFetcher

// Override the fetch method so that it's possible to customize fetch options.
// Specifically, if service.accumulateDataBlock is set it overrides the
// fetcher's accumulateDataBlock property.
- (void)beginFetchWithCompletionHandler:(GTM_NULLABLE GTMSessionFetcherCompletionHandler)handler;
@end
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

// This defines the class FIRStoragePointer, which is a C++-compatible wrapper
// around the FIRStorage Obj-C class.
OBJ_C_PTR_WRAPPER(FIRStorage);
// This defines the class FIRCPPGTMSessionFetcherPointer, which is a
// C++-compatible wrapper around the FIRCPPGTMSessionFetcherPointer Obj-C class.
OBJ_C_PTR_WRAPPER(FIRCPPGTMSessionFetcherService);

class StorageInternal {
 public:
  StorageInternal(App* _Nonnull app, const char* _Nullable url);

  ~StorageInternal();

  // Get the firebase::App that this Storage was created with.
  App* _Nonnull app() const;

  // Return the URL we were created with.
  const std::string& url() const { return url_; }

  // Get a StorageReference to the root of the database.
  StorageReferenceInternal* _Nullable GetReference() const;

  // Get a StorageReference for the specified path.
  StorageReferenceInternal* _Nullable GetReference(const char* _Nullable path) const;

  // Get a StorageReference for the provided URL.
  StorageReferenceInternal* _Nullable GetReferenceFromUrl(const char* _Nullable url) const;

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

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const;

  // When this is deleted, it will clean up all StorageReferences and other
  // objects.
  CleanupNotifier& cleanup() { return cleanup_; }

#ifdef __OBJC__
  // Get the session fetcher for streaming.
  FIRCPPGTMSessionFetcherService* _Nonnull session_fetcher_service() const {
    return session_fetcher_service_->get();
  }
#endif  // __OBJC__

  // Get the dispatch queue for streaming.
  dispatch_queue_t _Nullable dispatch_queue() const;

 private:
#ifdef __OBJC__
  FIRStorage* _Nullable impl() const { return impl_->get(); }
#endif
  // The firease::App that this Storage was created with.
  App* _Nonnull app_;

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRStoragePointer> impl_;
  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRCPPGTMSessionFetcherServicePointer> session_fetcher_service_;

  FutureManager future_manager_;

  std::string url_;

  CleanupNotifier cleanup_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_STORAGE_IOS_H_
