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

#include "storage/src/ios/storage_ios.h"
#include "app/src/app_ios.h"
#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "storage/src/ios/storage_reference_ios.h"

#import "FirebaseStorage-Swift.h"

namespace firebase {
namespace storage {
namespace internal {

StorageInternal::StorageInternal(App* app, const char* url)
    : app_(app),
      impl_(new FIRStoragePointer(nil)) {
  url_ = url ? url : "";
  FIRApp* platform_app = app->GetPlatformApp();
  if (url_.empty()) {
    impl_.reset(new FIRStoragePointer([FIRStorage storageForApp:platform_app]));
  } else {
    @try {
      impl_.reset(new FIRStoragePointer([FIRStorage storageForApp:platform_app
                                                              URL:@(url_.c_str())]));
    } @catch(NSException* exception) {
      LogWarning("Failed to create storage instance for bucket URL '%s'.  %s", url_.c_str(),
                 exception.reason.UTF8String);
      return;
    }
  }
}

StorageInternal::~StorageInternal() {
  // Destructor is necessary for ARC garbage collection.
}

::firebase::App* StorageInternal::app() const {
  return app_;
}

StorageReferenceInternal* StorageInternal::GetReference() const {
  return new StorageReferenceInternal(const_cast<StorageInternal*>(this),
                                      MakeUnique<FIRStorageReferencePointer>(impl().reference));
}

StorageReferenceInternal* StorageInternal::GetReference(const char* path) const {
  return new StorageReferenceInternal(
      const_cast<StorageInternal*>(this),
      MakeUnique<FIRStorageReferencePointer>([impl() referenceWithPath:@(path)]));
}

StorageReferenceInternal* StorageInternal::GetReferenceFromUrl(const char* url) const {
  FIRStorageReference* reference = nil;
  std::string url_string = url ? url : "";
  @try {
    reference = [impl() referenceForURL:@(url)];
  } @catch (NSException* exception) {
    LogWarning("Unable to get storage reference at URL '%s'.  %s", url_string.c_str(),
               exception.reason.UTF8String);
    return nullptr;
  }
  return new StorageReferenceInternal(const_cast<StorageInternal*>(this),
                                      MakeUnique<FIRStorageReferencePointer>(reference));
}

double StorageInternal::max_download_retry_time() const { return impl().maxDownloadRetryTime; }

void StorageInternal::set_max_download_retry_time(double max_transfer_retry_seconds) {
  impl().maxDownloadRetryTime = max_transfer_retry_seconds;
}

double StorageInternal::max_upload_retry_time() const { return impl().maxUploadRetryTime; }

void StorageInternal::set_max_upload_retry_time(double max_transfer_retry_seconds) {
  impl().maxUploadRetryTime = max_transfer_retry_seconds;
}

double StorageInternal::max_operation_retry_time() const { return impl().maxOperationRetryTime; }

void StorageInternal::set_max_operation_retry_time(double max_transfer_retry_seconds) {
  impl().maxOperationRetryTime = max_transfer_retry_seconds;
}

// Whether this object was successfully initialized by the constructor.
bool StorageInternal::initialized() const { return impl() != nil; }

}  // namespace internal
}  // namespace storage
}  // namespace firebase

