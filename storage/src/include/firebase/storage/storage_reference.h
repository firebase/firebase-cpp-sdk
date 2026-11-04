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

#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_STORAGE_REFERENCE_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_STORAGE_REFERENCE_H_

#include <cstddef>
#include <string>
#include <vector>

#include "firebase/future.h"
#include "firebase/internal/common.h"
#include "firebase/storage/list_result.h"
#include "firebase/storage/metadata.h"

namespace firebase {
namespace storage {

class Controller;
class Listener;
class Storage;

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class ControllerInternal;
class MetadataInternal;
class StorageInternal;
class StorageReferenceInternalCommon;
class StorageReferenceInternal;
}  // namespace internal
/// @endcond FIREBASE_APP_INTERNAL

#ifndef SWIG
/// Represents a reference to a Cloud Storage object.
/// Developers can upload and download objects, get/set object metadata, and
/// delete an object at a specified path.
#endif  // SWIG
class StorageReference {
 public:
  /// @brief Default constructor. This creates an invalid StorageReference.
  /// Attempting to perform any operations on this reference will fail unless a
  /// valid StorageReference has been assigned to it.
  StorageReference() : internal_(nullptr) {}

  ~StorageReference();

  /// @brief Copy constructor. It's totally okay (and efficient) to copy
  /// StorageReference instances, as they simply point to the same location.
  ///
  /// @param[in] reference StorageReference to copy from.
  StorageReference(const StorageReference& reference);

  /// @brief Copy assignment operator. It's totally okay (and efficient) to copy
  /// StorageReference instances, as they simply point to the same location.
  ///
  /// @param[in] reference StorageReference to copy from.
  ///
  /// @returns Reference to the destination StorageReference.
  StorageReference& operator=(const StorageReference& reference);

  /// @brief Move constructor. Moving is an efficient operation for
  /// StorageReference instances.
  ///
  /// @param[in] other StorageReference to move data from.
  StorageReference(StorageReference&& other) noexcept;

  /// @brief Move assignment operator. Moving is an efficient operation for
  /// StorageReference instances.
  ///
  /// @param[in] other StorageReference to move data from.
  ///
  /// @returns Reference to the destination StorageReference.
  StorageReference& operator=(StorageReference&& other) noexcept;

  /// @brief Gets the firebase::storage::Storage instance to which we refer.
  ///
  /// The pointer will remain valid as long as the owning Storage instance and
  /// App remain alive.
  ///
  /// @returns The firebase::storage::Storage instance that this
  /// StorageReference refers to, or nullptr if this StorageReference is
  /// invalid.
  Storage* storage() const;

  /// @brief Gets a reference to a location relative to this one.
  ///
  /// @param[in] path Path relative to this reference's location.
  /// The pointer only needs to be valid during this call.
  ///
  /// @returns Child relative to this location.
  StorageReference Child(const char* path) const;

  /// @brief Gets a reference to a location relative to this one.
  ///
  /// @param[in] path Path relative to this reference's location.
  ///
  /// @returns Child relative to this location.
  StorageReference Child(const std::string& path) const {
    return Child(path.c_str());
  }

  /// @brief Deletes the object at the current path.
  ///
  /// @returns A Future result, which will complete when the operation either
  /// succeeds or fails. When the Future is completed, if its Error is
  /// kErrorNone, the operation succeeded.
  Future<void> Delete();

  /// @brief Returns the result of the most recent call to Delete().
  ///
  /// @returns The result of the most recent call to Delete().
  Future<void> DeleteLastResult() const;

  /// @brief Return the Google Cloud Storage bucket that holds this object.
  ///
  /// @returns The bucket.
  std::string bucket() const;

  /// @brief Return the full path of the storage reference, not including
  /// the Google Cloud Storage bucket.
  ///
  /// @returns Full path to the storage reference, not including GCS bucket.
  /// For example, for the reference "gs://bucket/path/to/object.txt", the full
  /// path would be "path/to/object.txt".
  std::string full_path() const;

  /// @brief Asynchronously downloads the object from this StorageReference to a
  /// local file on disk.
  ///
  /// @param[in] path Path to local file on device to download into.
  /// @param[in] listener Optional listener that will respond to events on this
  /// download operation. The caller is responsible for allocating and
  /// deallocating the listener.
  /// @param[out] controller_out Optional Controller output that provides the
  /// ability to pause (on desktop), resume (on desktop), or cancel an ongoing
  /// download operation.
  ///
  /// @returns A future that returns the number of bytes downloaded.
  Future<size_t> GetFile(const char* path, Listener* listener = nullptr,
                         Controller* controller_out = nullptr);

#ifndef SWIG
  /// @brief Asynchronously downloads the object from this StorageReference to a
  /// local file on disk.
  ///
  /// @param[in] path Path to local file on device to download into.
  /// @param[in] listener Optional listener that will respond to events on this
  /// download operation.
  /// @param[out] controller_out Optional Controller output to control the
  /// download operation.
  ///
  /// @returns A future that returns the number of bytes downloaded.
  Future<size_t> GetFile(const std::string& path, Listener* listener = nullptr,
                         Controller* controller_out = nullptr) {
    return GetFile(path.c_str(), listener, controller_out);
  }
#endif  // SWIG

  /// @brief Returns the result of the most recent call to GetFile().
  ///
  /// @returns The result of the most recent call to GetFile().
  Future<size_t> GetFileLastResult() const;

  /// @brief Asynchronously downloads the object from this StorageReference into
  /// a caller-provided memory buffer.
  ///
  /// @param[in] buffer A byte buffer to read the data into. This buffer must
  /// remain valid for the duration of the transfer.
  /// @param[in] buffer_size The size of the byte buffer.
  /// @param[in] listener Optional listener that will respond to events on this
  /// download operation. The caller is responsible for allocating and
  /// deallocating the listener.
  /// @param[out] controller_out Optional Controller output that provides the
  /// ability to pause (on desktop), resume (on desktop), or cancel an ongoing
  /// download operation.
  ///
  /// @returns A future that returns the number of bytes read.
  Future<size_t> GetBytes(void* buffer, size_t buffer_size,
                          Listener* listener = nullptr,
                          Controller* controller_out = nullptr);

  /// @brief Returns the result of the most recent call to GetBytes().
  ///
  /// @returns The result of the most recent call to GetBytes().
  Future<size_t> GetBytesLastResult() const;

  /// @brief Asynchronously retrieves a long lived download URL with a revokable
  /// token.
  ///
  /// This can be used to share the file with others, but can be revoked by a
  /// developer in the Firebase Console if desired.
  ///
  /// @returns A Future result, which will complete when the operation either
  /// succeeds or fails. When the Future is completed, if its Error is
  /// kErrorNone, the operation succeeded and the URL is returned.
  Future<std::string> GetDownloadUrl();

  /// @brief Returns the result of the most recent call to GetDownloadUrl().
  ///
  /// @returns The result of the most recent call to GetDownloadUrl().
  Future<std::string> GetDownloadUrlLastResult() const;

  /// @brief Retrieves metadata associated with an object at this
  /// StorageReference.
  ///
  /// @returns A Future result, which will complete when the operation either
  /// succeeds or fails. When the Future is completed, if its Error is
  /// kErrorNone, the operation succeeded and the Metadata is returned.
  Future<Metadata> GetMetadata();

  /// @brief Returns the result of the most recent call to GetMetadata().
  ///
  /// @returns The result of the most recent call to GetMetadata().
  Future<Metadata> GetMetadataLastResult() const;

  /// @brief Updates the metadata associated with this StorageReference.
  ///
  /// @param[in] metadata The valid Metadata attributes to apply to this object.
  ///
  /// @returns A Future result, which will complete when the operation either
  /// succeeds or fails. When the Future is completed, if its Error is
  /// kErrorNone, the operation succeeded and the Metadata is returned.
  Future<Metadata> UpdateMetadata(const Metadata& metadata);

  /// @brief Returns the result of the most recent call to UpdateMetadata().
  ///
  /// @returns The result of the most recent call to UpdateMetadata().
  Future<Metadata> UpdateMetadataLastResult() const;

  /// @brief List items (files) and prefixes (folders) under this
  /// StorageReference.
  ///
  /// "/" is treated as a path delimiter. Storage does not support object paths
  /// that end with "/" or contain two consecutive "/"s. All invalid objects in
  /// GCS will be filtered. Requires Firebase Storage Security Rules Version 2.
  ///
  /// @param[in] max_results_per_page The maximum number of results to return
  /// in a single page. Must be greater than 0 and at most 1000.
  /// @param[in] page_token A page token from a previous call to List, or
  /// nullptr if this is the first call.
  ///
  /// @returns A Future result, which will complete when the operation either
  /// succeeds or fails. When the Future is completed, if its Error is
  /// kErrorNone, the operation succeeded and the StorageListResult is returned.
  Future<StorageListResult> List(int max_results_per_page = 1000,
                                 const char* page_token = nullptr);

#ifndef SWIG
  /// @brief List items (files) and prefixes (folders) under this
  /// StorageReference.
  ///
  /// @param[in] max_results_per_page The maximum number of results to return
  /// in a single page. Must be greater than 0 and at most 1000.
  /// @param[in] page_token A page token from a previous call to List, or an
  /// empty string if this is the first call.
  ///
  /// @returns A Future result containing the StorageListResult page.
  Future<StorageListResult> List(int max_results_per_page,
                                 const std::string& page_token) {
    return List(max_results_per_page,
                page_token.empty() ? nullptr : page_token.c_str());
  }
#endif  // SWIG

  /// @brief Returns the result of the most recent call to List().
  ///
  /// @returns The result of the most recent call to List().
  Future<StorageListResult> ListLastResult() const;

  /// @brief List all items (files) and prefixes (folders) under this
  /// StorageReference.
  ///
  /// @returns A Future result, which will complete when the operation either
  /// succeeds or fails. When the Future is completed, if its Error is
  /// kErrorNone, the operation succeeded and the combined StorageListResult is
  /// returned.
  Future<StorageListResult> ListAll();

  /// @brief Returns the result of the most recent call to ListAll().
  ///
  /// @returns The result of the most recent call to ListAll().
  Future<StorageListResult> ListAllLastResult() const;

  /// @brief Returns the short name of this object.
  ///
  /// @returns the short name of this object.
  std::string name() const;

  /// @brief Returns a new instance of StorageReference pointing to the parent
  /// location or an invalid StorageReference (`is_valid() == false`) if this
  /// instance references the root location.
  ///
  /// @returns The parent StorageReference, or an invalid StorageReference if at
  /// the root.
  StorageReference GetParent() const;

  /// @brief Returns a new instance of StorageReference pointing to the root of
  /// the bucket.
  ///
  /// @returns The root StorageReference, or an invalid StorageReference if this
  /// reference is invalid.
  StorageReference GetRoot() const;

  /// @brief Returns true if this StorageReference points to the root of the
  /// bucket.
  bool is_root() const;

  /// @brief Asynchronously uploads data to the currently specified
  /// StorageReference, without additional metadata.
  ///
  /// @param[in] buffer A byte buffer to write data from. This buffer must
  /// remain valid for the duration of the transfer.
  /// @param[in] buffer_size The size of the byte buffer.
  /// @param[in] listener Optional listener that will respond to events on this
  /// upload operation. The caller is responsible for allocating and
  /// deallocating the listener.
  /// @param[out] controller_out Optional Controller output that provides the
  /// ability to pause, resume, or cancel an ongoing upload operation.
  ///
  /// @returns A future that returns the Metadata of the uploaded object.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            Listener* listener = nullptr,
                            Controller* controller_out = nullptr);

  /// @brief Asynchronously uploads data to the currently specified
  /// StorageReference, with custom metadata.
  ///
  /// @param[in] buffer A byte buffer to write data from. This buffer must
  /// remain valid for the duration of the transfer.
  /// @param[in] buffer_size The number of bytes to write.
  /// @param[in] metadata Metadata containing additional information (MIME type,
  /// etc.) about the object being uploaded.
  /// @param[in] listener Optional listener that will respond to events on this
  /// upload operation. The caller is responsible for allocating and
  /// deallocating the listener.
  /// @param[out] controller_out Optional Controller output that provides the
  /// ability to pause, resume, or cancel an ongoing upload operation.
  ///
  /// @returns A future that returns the Metadata of the uploaded object.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            const Metadata& metadata,
                            Listener* listener = nullptr,
                            Controller* controller_out = nullptr);

  /// @brief Returns the result of the most recent call to PutBytes().
  ///
  /// @returns The result of the most recent call to PutBytes().
  Future<Metadata> PutBytesLastResult() const;

  /// @brief Asynchronously uploads data from a local file to the currently
  /// specified StorageReference, without additional metadata.
  ///
  /// @param[in] path Path to local file on device to upload to Firebase
  /// Storage.
  /// @param[in] listener Optional listener that will respond to events on this
  /// upload operation. The caller is responsible for allocating and
  /// deallocating the listener.
  /// @param[out] controller_out Optional Controller output that provides the
  /// ability to pause, resume, or cancel an ongoing upload operation.
  ///
  /// @returns A future that returns the Metadata of the uploaded object.
  Future<Metadata> PutFile(const char* path, Listener* listener = nullptr,
                           Controller* controller_out = nullptr);

#ifndef SWIG
  /// @brief Asynchronously uploads data from a local file to the currently
  /// specified StorageReference, without additional metadata.
  ///
  /// @param[in] path Path to local file on device to upload to Firebase
  /// Storage.
  /// @param[in] listener Optional listener that will respond to events on this
  /// upload operation.
  /// @param[out] controller_out Optional Controller output to control the
  /// upload operation.
  ///
  /// @returns A future that returns the Metadata of the uploaded object.
  Future<Metadata> PutFile(const std::string& path,
                           Listener* listener = nullptr,
                           Controller* controller_out = nullptr) {
    return PutFile(path.c_str(), listener, controller_out);
  }
#endif  // SWIG

  /// @brief Asynchronously uploads data from a local file to the currently
  /// specified StorageReference, with custom metadata.
  ///
  /// @param[in] path Path to local file on device to upload to Firebase
  /// Storage.
  /// @param[in] metadata Metadata containing additional information (MIME type,
  /// etc.) about the object being uploaded.
  /// @param[in] listener Optional listener that will respond to events on this
  /// upload operation. The caller is responsible for allocating and
  /// deallocating the listener.
  /// @param[out] controller_out Optional Controller output that provides the
  /// ability to pause, resume, or cancel an ongoing upload operation.
  ///
  /// @returns A future that returns the Metadata of the uploaded object.
  Future<Metadata> PutFile(const char* path, const Metadata& metadata,
                           Listener* listener = nullptr,
                           Controller* controller_out = nullptr);

#ifndef SWIG
  /// @brief Asynchronously uploads data from a local file to the currently
  /// specified StorageReference, with custom metadata.
  ///
  /// @param[in] path Path to local file on device to upload to Firebase
  /// Storage.
  /// @param[in] metadata Metadata containing additional information (MIME type,
  /// etc.) about the object being uploaded.
  /// @param[in] listener Optional listener that will respond to events on this
  /// upload operation.
  /// @param[out] controller_out Optional Controller output to control the
  /// upload operation.
  ///
  /// @returns A future that returns the Metadata of the uploaded object.
  Future<Metadata> PutFile(const std::string& path, const Metadata& metadata,
                           Listener* listener = nullptr,
                           Controller* controller_out = nullptr) {
    return PutFile(path.c_str(), metadata, listener, controller_out);
  }
#endif  // SWIG

  /// @brief Returns the result of the most recent call to PutFile().
  ///
  /// @returns The result of the most recent call to PutFile().
  Future<Metadata> PutFileLastResult() const;

  /// @brief Returns true if this StorageReference is valid, false if it is not
  /// valid. An invalid StorageReference indicates that the reference is
  /// uninitialized (created with the default constructor) or that there was an
  /// error retrieving the reference.
  ///
  /// @returns true if this StorageReference is valid, false if this
  /// StorageReference is invalid.
  bool is_valid() const;

#ifndef SWIG
  /// @brief Returns the `gs://<bucket>/<full_path>` URI of this reference, or
  /// an empty string if invalid.
  std::string ToString() const;

  /// @brief Equality operator.
  bool operator==(const StorageReference& other) const;

  /// @brief Inequality operator.
  bool operator!=(const StorageReference& other) const {
    return !(*this == other);
  }
#endif  // SWIG

#if defined(FIREBASE_STORAGE_BUILDING_SDK) && !defined(SWIG) && \
    !defined(DOXYGEN)
  /// @cond FIREBASE_APP_INTERNAL
  // ABI compatibility: non-const overloads matching the pre-C++17 SDK's
  // exported symbols, so binaries built against the older headers still link.
  // Only visible while compiling the SDK (FIREBASE_STORAGE_BUILDING_SDK is a
  // PRIVATE define of the firebase_storage target), so newly compiled client
  // code always binds to the const overloads. These are non-virtual and do
  // not affect object layout.
  // Defined out-of-line (so the symbols are exported); each forwards to the
  // const overload above.
  Storage* storage();
  Future<void> DeleteLastResult();
  std::string bucket();
  std::string full_path();
  Future<size_t> GetFileLastResult();
  Future<size_t> GetBytesLastResult();
  Future<std::string> GetDownloadUrlLastResult();
  Future<Metadata> GetMetadataLastResult();
  Future<Metadata> UpdateMetadataLastResult();
  Future<StorageListResult> ListLastResult();
  std::string name();
  StorageReference GetParent();
  Future<Metadata> PutBytesLastResult();
  Future<Metadata> PutFileLastResult();
  /// @endcond
#endif  // defined(FIREBASE_STORAGE_BUILDING_SDK) && !defined(SWIG) &&
        // !defined(DOXYGEN)

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class Controller;
  friend class internal::ControllerInternal;
  friend class Metadata;
  friend class internal::MetadataInternal;
  friend class Storage;
  friend class internal::StorageReferenceInternal;
  friend class internal::StorageReferenceInternalCommon;

  // Intentionally not explicit, to preserve source compatibility with the
  // previous SDK (internal code converts StorageReferenceInternal* implicitly).
  StorageReference(internal::StorageReferenceInternal* internal);  // NOLINT

  internal::StorageReferenceInternal* internal_;
  /// @endcond
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_STORAGE_REFERENCE_H_
