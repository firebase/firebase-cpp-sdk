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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_STORAGE_REFERENCE_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_STORAGE_REFERENCE_IOS_H_

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "storage/src/ios/storage_ios.h"

#ifdef __OBJC__
#import "FIRStorage.h"
#import "FIRStorageConstants.h"
#import "FIRStorageObservableTask.h"
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
  explicit StorageReferenceInternal(StorageInternal* _Nonnull storage,
                                    UniquePtr<FIRStorageReferencePointer> impl);

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

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  // Move constructor. Moving is an efficient operation for
  // StorageReferenceInternal instances.
  StorageReferenceInternal(StorageReferenceInternal&& other);

  // Move assignment operator. Moving is an efficient operation for
  // StorageReferenceInternal instances.
  StorageReferenceInternal& operator=(StorageReferenceInternal&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  // Gets the storage to which we refer.
  Storage* _Nonnull storage();

  // Gets a reference to a location relative to this one.
  StorageReferenceInternal* _Nonnull Child(const char* _Nonnull path) const;

  // Deletes the object at the current path.
  Future<void> Delete();

  // Returns the result of the most recent call to Delete();
  Future<void> DeleteLastResult();

  // Return the Google Cloud Storage bucket that holds this object.
  std::string bucket();

  // Return the full path to of this object.
  std::string full_path();

  // Asynchronously downloads the object from this StorageReferenceInternal.
  Future<size_t> GetFile(const char* _Nonnull path,
                         Listener* _Nullable listener,
                         Controller* _Nullable controller_out);

  // Returns the result of the most recent call to GetFile();
  Future<size_t> GetFileLastResult();

  // Asynchronously downloads the object from this StorageReferenceInternal.
  Future<size_t> GetBytes(void* _Nonnull buffer, size_t buffer_size,
                          Listener* _Nullable listener,
                          Controller* _Nullable controller_out);

  // Returns the result of the most recent call to GetBytes();
  Future<size_t> GetBytesLastResult();

  // Asynchronously retrieves a long lived download URL with a revokable token.
  Future<std::string> GetDownloadUrl();

  // Returns the result of the most recent call to GetDownloadUrl();
  Future<std::string> GetDownloadUrlLastResult();

  // Retrieves metadata associated with an object at this
  // StorageReferenceInternal.
  Future<Metadata> GetMetadata();

  // Returns the result of the most recent call to GetMetadata();
  Future<Metadata> GetMetadataLastResult();

  // Updates the metadata associated with this StorageReferenceInternal.
  Future<Metadata> UpdateMetadata(const Metadata* _Nonnull metadata);

  // Returns the result of the most recent call to UpdateMetadata();
  Future<Metadata> UpdateMetadataLastResult();

  // Returns the short name of this object.
  std::string name();

  // Returns a new instance of StorageReferenceInternal pointing to the parent
  // location or null if this instance references the root location.
  StorageReferenceInternal* _Nullable GetParent();

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutBytes(const void* _Nonnull buffer, size_t buffer_size,
                            Listener* _Nullable listener,
                            Controller* _Nullable controller_out);

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutBytes(const void* _Nonnull buffer, size_t buffer_size,
                            const Metadata* _Nullable metadata,
                            Listener* _Nullable listener,
                            Controller* _Nullable controller_out);

  // Returns the result of the most recent call to PutBytes();
  Future<Metadata> PutBytesLastResult();

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutFile(const char* _Nonnull path,
                           Listener* _Nullable listener,
                           Controller* _Nullable controller_out);

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutFile(const char* _Nonnull path,
                           const Metadata* _Nullable metadata,
                           Listener* _Nullable listener,
                           Controller* _Nullable controller_out);

  // Returns the result of the most recent call to PutFile();
  Future<Metadata> PutFileLastResult();

  // StorageInternal instance we are associated with.
  StorageInternal* _Nullable storage_internal() const { return storage_; }

 private:
#ifdef __OBJC__
  void AssignListener(
      FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task,
      Listener* _Nullable listener, StorageInternal* _Nullable storage);

  // Create a download task that will stream data into the specified buffer.
  FIRStorageDownloadTask* _Nonnull CreateStreamingDownloadTask(
      FIRStorageReference* _Nonnull impl, StorageInternal* _Nonnull storage,
      FIRStorageVoidDataError _Nonnull completion, void* _Nonnull buffer,
      size_t buffer_size);

  FIRStorageReference* _Nullable impl() const { return impl_->get(); }
#endif  // __OBJC__

  // Get the Future for the StorageReferenceInternal.
  ReferenceCountedFutureImpl* _Nullable future();

  // Keep track of the Storage object for managing Futures.
  StorageInternal* _Nullable storage_;

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRStorageReferencePointer> impl_;

  Mutex controller_init_mutex_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_STORAGE_REFERENCE_IOS_H_
