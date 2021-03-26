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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_STORAGE_REFERENCE_ANDROID_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_STORAGE_REFERENCE_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

class StorageReferenceInternal {
 public:
  // StorageReferenceInternal will create its own global reference to ref_obj,
  // so you should delete the object you passed in after creating the
  // StorageReferenceInternal instance.
  StorageReferenceInternal(StorageInternal* database, jobject ref_obj);

  StorageReferenceInternal(const StorageReferenceInternal& ref);

  StorageReferenceInternal& operator=(const StorageReferenceInternal& ref);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  StorageReferenceInternal(StorageReferenceInternal&& ref);

  StorageReferenceInternal& operator=(StorageReferenceInternal&& ref);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ~StorageReferenceInternal();

  // Gets the storage to which we refer.
  Storage* storage();

  // Gets a reference to a location relative to this one.
  StorageReferenceInternal* Child(const char* path) const;

  // Deletes the object at the current path.
  Future<void> Delete();

  Future<void> DeleteLastResult();

  // Return the Google Cloud Storage bucket that holds this object.
  std::string bucket();

  // Return the full path of this object.
  std::string full_path();

  // Asynchronously downloads the object from this StorageReferenceInternal.
  Future<size_t> GetFile(const char* path, Listener* listener,
                         Controller* controller_out);

  // Returns the result of the most recent call to GetFile();
  Future<size_t> GetFileLastResult();

  // Asynchronously downloads the object from this StorageReferenceInternal.
  Future<size_t> GetBytes(void* buffer, size_t buffer_size, Listener* listener,
                          Controller* controller_out);

  // Returns the result of the most recent call to GetBytes();
  Future<size_t> GetBytesLastResult();

  // Asynchronously retrieves a long lived download URL with a revokable token.
  Future<std::string> GetDownloadUrl();

  Future<std::string> GetDownloadUrlLastResult();

  // Retrieves metadata associated with an object at this
  // StorageReferenceInternal.
  Future<Metadata> GetMetadata();

  Future<Metadata> GetMetadataLastResult();

  // Updates the metadata associated with this StorageReferenceInternal.
  Future<Metadata> UpdateMetadata(const Metadata* metadata);

  Future<Metadata> UpdateMetadataLastResult();

  // Returns the short name of this object.
  std::string name();

  // Returns a new instance of StorageReferenceInternal pointing to the parent
  // location or null if this instance references the root location.
  StorageReferenceInternal* GetParent();

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            Listener* listener, Controller* controller_out);

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            const Metadata* metadata, Listener* listener,
                            Controller* controller_out);

  // Returns the result of the most recent call to PutBytes();
  Future<Metadata> PutBytesLastResult();

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutFile(const char* path, Listener* listener,
                           Controller* controller_out);

  // Asynchronously uploads data to the currently specified
  // StorageReferenceInternal, without additional metadata.
  Future<Metadata> PutFile(const char* path, const Metadata* metadata,
                           Listener* listener, Controller* controller_out);

  // Returns the result of the most recent call to PutFile();
  Future<Metadata> PutFileLastResult();

  // Initialize JNI bindings for this class.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Called from the Java CppByteDownloader class, this simply writes some bytes
  // into a buffer at the specified offset.
  static void CppByteDownloaderWriteBytes(JNIEnv* env, jclass clazz,
                                          jlong buffer_ptr, jlong buffer_size,
                                          jlong buffer_offset,
                                          jbyteArray byte_array,
                                          jlong num_bytes_to_copy);

  // Called from the Java CppByteUploader class, this simply reads some bytes
  // from a C++ buffer into a Java buffer at the specified offset.
  static jint CppByteUploaderReadBytes(JNIEnv* env, jclass clazz,
                                       jlong cpp_buffer_pointer,
                                       jlong cpp_buffer_size,
                                       jlong cpp_buffer_offset, jobject bytes,
                                       jint bytes_offset,
                                       jint num_bytes_to_read);

  // StorageInternal instance we are associated with.
  StorageInternal* storage_internal() const { return storage_; }

 private:
  static void FutureCallback(JNIEnv* env, jobject result,
                             util::FutureResult result_code,
                             const char* status_message, void* callback_data);

  // If `listener` is not nullptr, create a Java listener class for it and
  // assign it to the running task, returning the new Java listener.
  jobject AssignListenerToTask(Listener* listener, jobject task);

  ReferenceCountedFutureImpl* future();

  StorageInternal* storage_;
  jobject obj_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_STORAGE_REFERENCE_ANDROID_H_
