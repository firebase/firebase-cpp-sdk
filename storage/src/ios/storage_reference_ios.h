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

#ifndef FIREBASE_STORAGE_SRC_IOS_STORAGE_REFERENCE_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_STORAGE_REFERENCE_IOS_H_

#include <memory>
#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/storage_reference.h"
#include "storage/src/ios/storage_ios.h"

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"

// typedef not included in Swift header.
typedef void (^FIRStorageVoidDataError)(NSData* _Nullable, NSError* _Nullable);
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

// This defines the class FIRStorageReferencePointer, which is a C++-compatible
// wrapper around the FIRStorageReference Obj-C class.
OBJ_C_PTR_WRAPPER(FIRStorageReference);
// This defines the class FIRStorageObservableTaskPointer, which is a
// C++-compatible wrapper around the FIRStorageObservableTask Obj-C class.
OBJ_C_PTR_WRAPPER(FIRStorageObservableTask);

class StorageReferenceInternal {
 public:
  explicit StorageReferenceInternal(
      StorageInternal* _Nonnull storage,
      std::unique_ptr<FIRStorageReferencePointer> impl);

  ~StorageReferenceInternal();

  // Copy constructor. It's totally okay (and efficient) to copy
  // StorageReferenceInternal instances, as they simply point to the same
  // location.
  StorageReferenceInternal(const StorageReferenceInternal& reference);

  // Copy assignment operator. It's totally okay (and efficient) to copy
  // StorageReferenceInternal instances, as they simply point to the same
  // location.
  StorageReferenceInternal& operator=(
      const StorageReferenceInternal& reference);

  // Move constructor. Moving is an efficient operation for
  // StorageReferenceInternal instances.
  StorageReferenceInternal(StorageReferenceInternal&& other) noexcept;

  // Move assignment operator. Moving is an efficient operation for
  // StorageReferenceInternal instances.
  StorageReferenceInternal& operator=(
      StorageReferenceInternal&& other) noexcept;

  // Gets the storage to which we refer.
  Storage* _Nullable storage() const;

  // Gets a reference to a location relative to this one.
  StorageReferenceInternal* _Nullable Child(const char* _Nullable path) const;

  // Deletes the object at the current path.
  Future<void> Delete();

  // Returns the result of the most recent call to Delete();
  Future<void> DeleteLastResult() const;

  // Return the Google Cloud Storage bucket that holds this object.
  std::string bucket() const;

  // Return the full path to of this object.
  std::string full_path() const;

  // Asynchronously downloads the object from this StorageReferenceInternal.
  Future<size_t> GetFile(const char* _Nullable path,
                         Listener* _Nullable listener,
                         Controller* _Nullable controller_out);

  // Returns the result of the most recent call to GetFile();
  Future<size_t> GetFileLastResult() const;

  // Asynchronously downloads the object from this StorageReferenceInternal.
  Future<size_t> GetBytes(void* _Nullable buffer, size_t buffer_size,
                          Listener* _Nullable listener,
                          Controller* _Nullable controller_out);

  // Returns the result of the most recent call to GetBytes();
  Future<size_t> GetBytesLastResult() const;

  // Asynchronously retrieves a long lived download URL with a revokable token.
  Future<std::string> GetDownloadUrl();

  // Returns the result of the most recent call to GetDownloadUrl();
  Future<std::string> GetDownloadUrlLastResult() const;

  // Retrieves metadata associated with an object at this
  // StorageReferenceInternal.
  Future<Metadata> GetMetadata();

  // Returns the result of the most recent call to GetMetadata();
  Future<Metadata> GetMetadataLastResult() const;

  // Updates the metadata associated with this StorageReferenceInternal.
  Future<Metadata> UpdateMetadata(const Metadata* _Nullable metadata);

  // Returns the result of the most recent call to UpdateMetadata();
  Future<Metadata> UpdateMetadataLastResult() const;

  // Returns the short name of this object.
  std::string name() const;

  // Returns a new instance of StorageReferenceInternal pointing to the parent
  // location or null if this instance references the root location.
  StorageReferenceInternal* _Nullable GetParent() const;

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutBytes(const void* _Nullable buffer, size_t buffer_size,
                            Listener* _Nullable listener,
                            Controller* _Nullable controller_out);

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutBytes(const void* _Nullable buffer, size_t buffer_size,
                            const Metadata* _Nullable metadata,
                            Listener* _Nullable listener,
                            Controller* _Nullable controller_out);

  // Returns the result of the most recent call to PutBytes();
  Future<Metadata> PutBytesLastResult() const;

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutFile(const char* _Nullable path,
                           Listener* _Nullable listener,
                           Controller* _Nullable controller_out);

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutFile(const char* _Nullable path,
                           const Metadata* _Nullable metadata,
                           Listener* _Nullable listener,
                           Controller* _Nullable controller_out);

  // Returns the result of the most recent call to PutFile();
  Future<Metadata> PutFileLastResult() const;

  // List items (files) and prefixes (folders) under this StorageReference.
  Future<StorageListResult> List(int max_results_per_page,
                                 const char* _Nullable page_token);

  // Returns the result of the most recent call to List();
  Future<StorageListResult> ListLastResult() const;

  // List all items (files) and prefixes (folders) under this StorageReference.
  Future<StorageListResult> ListAll();

  // Returns the result of the most recent call to ListAll();
  Future<StorageListResult> ListAllLastResult() const;

  // StorageInternal instance we are associated with.
  StorageInternal* _Nullable storage_internal() const { return storage_; }

 private:
#ifdef __OBJC__
  FIRStorageReference* _Nullable impl() const {
    return impl_ ? impl_->get() : nil;
  }
#endif  // __OBJC__

  // Get the Future for the StorageReferenceInternal.
  ReferenceCountedFutureImpl* _Nullable future() const;

  // Keep track of the Storage object for managing Futures.
  StorageInternal* _Nullable storage_ = nullptr;

  // Object lifetime managed by Objective C ARC.
  std::unique_ptr<FIRStorageReferencePointer> impl_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_STORAGE_REFERENCE_IOS_H_
