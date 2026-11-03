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

#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_H_

#include <cstddef>
#include <string>

#include "firebase/app.h"
#include "firebase/internal/common.h"
#include "firebase/log.h"
#include "firebase/storage/common.h"
#include "firebase/storage/controller.h"
#include "firebase/storage/list_result.h"  // Umbrella: StorageListResult.
#include "firebase/storage/listener.h"
#include "firebase/storage/metadata.h"
#include "firebase/storage/storage_reference.h"

#if !defined(DOXYGEN)
#ifndef SWIG
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(storage)
#endif  // SWIG
#endif  // !defined(DOXYGEN)

namespace firebase {

/// Namespace for the Firebase C++ SDK for Cloud Storage.
namespace storage {

namespace internal {
class StorageInternal;
class MetadataInternal;
}  // namespace internal

class StorageReference;

#ifndef SWIG
/// @brief Entry point for the Firebase C++ SDK for Cloud Storage.
///
/// To use the SDK, call firebase::storage::Storage::GetInstance() to
/// obtain an instance of Storage, then use GetReference() to obtain references
/// to child blobs. From there you can upload data with
/// StorageReference::PutBytes() or StorageReference::PutFile(), and download
/// data via StorageReference::GetBytes() or StorageReference::GetFile().
#endif  // SWIG
class Storage {
 public:
  /// @brief Destructor. You may delete an instance of Storage when
  /// you are finished using it, to shut down the Storage library.
  ~Storage();

  /// @brief Get an instance of Storage corresponding to the default App.
  ///
  /// Cloud Storage uses firebase::App to communicate with Firebase
  /// Authentication to authenticate users to the server backend.
  ///
  /// @note To receive an InitResult, call
  /// `GetInstance(::firebase::App::GetInstance(), &init_result)` instead. (A
  /// default-App overload taking only `InitResult*` is intentionally not
  /// provided, so that `GetInstance(nullptr)`, `GetInstance(NULL)`, and
  /// `GetInstance(0)` keep resolving unambiguously to the `App*` overload.)
  ///
  /// @returns An instance of Storage corresponding to the default App, or
  /// nullptr if the default App does not exist or no default Storage bucket is
  /// configured.
  static Storage* GetInstance();

  /// @brief Get an instance of Storage corresponding to the default App,
  /// with the given Cloud Storage URL.
  ///
  /// @param[in] url Cloud Storage bucket URL (for example,
  /// "gs://my-custom-bucket").
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns An instance of Storage corresponding to the default App and URL,
  /// or nullptr if the default App does not exist or the bucket URL is invalid.
  static Storage* GetInstance(const std::string& url,
                              InitResult* init_result_out = nullptr);

  /// @brief Get an instance of Storage corresponding to the given App.
  ///
  /// Cloud Storage uses firebase::App to communicate with Firebase
  /// Authentication to authenticate users to the server backend.
  ///
  /// @param[in] app An instance of firebase::App. Cloud Storage will use
  /// this to communicate with Firebase Authentication.
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns An instance of Storage corresponding to the given App, or nullptr
  /// if `app` is nullptr or no default Storage bucket is configured.
  static Storage* GetInstance(::firebase::App* app,
                              InitResult* init_result_out = nullptr);

  /// @brief Get an instance of Storage corresponding to the given App,
  /// with the given Cloud Storage URL.
  ///
  /// Cloud Storage uses firebase::App to communicate with Firebase
  /// Authentication to authenticate users to the server backend.
  ///
  /// @param[in] app An instance of firebase::App. Cloud Storage will use
  /// this to communicate with Firebase Authentication.
  /// @param[in] url Cloud Storage bucket URL (for example,
  /// "gs://my-custom-bucket"). Must refer to a root bucket without an object
  /// path.
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns An instance of Storage corresponding to the given App, or nullptr
  /// if `app` is nullptr or `url` is invalid or contains an object path.
  static Storage* GetInstance(::firebase::App* app, const char* url,
                              InitResult* init_result_out = nullptr);

  /// @brief Get an instance of Storage corresponding to the given App,
  /// with the given Cloud Storage URL.
  ///
  /// @param[in] app An instance of firebase::App.
  /// @param[in] url Cloud Storage bucket URL (for example,
  /// "gs://my-custom-bucket").
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here.
  ///
  /// @returns An instance of Storage corresponding to the given App, or nullptr
  /// if `app` is nullptr or `url` is invalid.
  static Storage* GetInstance(::firebase::App* app, const std::string& url,
                              InitResult* init_result_out = nullptr) {
    return GetInstance(app, url.c_str(), init_result_out);
  }

  /// @brief Get the firebase::App that this Storage was created with.
  ///
  /// @returns The firebase::App this Storage was created with.
  ::firebase::App* app() const;

  /// @brief Get the URL that this Storage was created with.
  ///
  /// @returns The URL this Storage was created with, or an empty
  /// string if this Storage was created with default parameters.
  std::string url() const;

  /// @brief Get a StorageReference to the root of the Cloud Storage bucket.
  ///
  /// @returns A StorageReference pointing to the root of the bucket.
  StorageReference GetReference() const;

  /// @brief Get a StorageReference for the specified path.
  ///
  /// @param[in] path Path relative to the root of the Cloud Storage bucket.
  ///
  /// @returns A StorageReference for the given path, or an invalid
  /// StorageReference if `path` is nullptr.
  StorageReference GetReference(const char* path) const;
  /// @brief Get a StorageReference for the specified path.
  ///
  /// @param[in] path Path relative to the root of the Cloud Storage bucket.
  ///
  /// @returns A StorageReference for the given path.
  StorageReference GetReference(const std::string& path) const {
    return GetReference(path.c_str());
  }

  /// @brief Get a StorageReference for the provided URL.
  ///
  /// @param[in] url A `gs://` or `http[s]://firebasestorage.googleapis.com/...`
  /// URL pointing to a Cloud Storage bucket or object.
  ///
  /// @returns A StorageReference for the provided URL, or an invalid
  /// StorageReference if the URL is nullptr, malformed, or refers to a
  /// different bucket than this Storage instance.
  StorageReference GetReferenceFromUrl(const char* url) const;
  /// @brief Get a StorageReference for the provided URL.
  ///
  /// @param[in] url A `gs://` or `http[s]://firebasestorage.googleapis.com/...`
  /// URL pointing to a Cloud Storage bucket or object.
  ///
  /// @returns A StorageReference for the provided URL, or an invalid
  /// StorageReference if the URL is malformed or refers to a different bucket.
  StorageReference GetReferenceFromUrl(const std::string& url) const {
    return GetReferenceFromUrl(url.c_str());
  }

  /// @brief Returns the maximum time in seconds to retry a download if a
  /// failure occurs.
  double max_download_retry_time() const;
  /// @brief Sets the maximum time in seconds to retry a download if a failure
  /// occurs. Defaults to 600 seconds (10 minutes).
  void set_max_download_retry_time(double max_transfer_retry_seconds);

  /// @brief Returns the maximum time in seconds to retry an upload if a failure
  /// occurs.
  double max_upload_retry_time() const;
  /// @brief Sets the maximum time in seconds to retry an upload if a failure
  /// occurs. Defaults to 600 seconds (10 minutes).
  void set_max_upload_retry_time(double max_transfer_retry_seconds);

  /// @brief Returns the maximum time in seconds to retry operations other than
  /// upload and download if a failure occurs.
  double max_operation_retry_time() const;
  /// @brief Sets the maximum time in seconds to retry operations other than
  /// upload and download if a failure occurs. Defaults to 120 seconds
  /// (2 minutes).
  void set_max_operation_retry_time(double max_operation_retry_seconds);

  /// @brief Configures the Storage SDK to use an emulated backend instead of
  /// the default remote backend. This method should be called before invoking
  /// any other methods on a new instance of Storage.
  ///
  /// @param[in] host The emulator host (without scheme, e.g. "localhost").
  /// @param[in] port The emulator port (1..65535).
  void UseEmulator(const std::string& host, int port) {
    UseEmulator(host.c_str(), port);
  }
  /// @brief Configures the Storage SDK to use an emulated backend instead of
  /// the default remote backend. This method should be called before invoking
  /// any other methods on a new instance of Storage.
  ///
  /// @param[in] host The emulator host (without scheme, e.g. "localhost").
  /// @param[in] port The emulator port (1..65535).
  void UseEmulator(const char* host, int port);

  /// @brief Sets the log verbosity of this Storage instance.
  ///
  /// @param[in] log_level The desired LogLevel.
  void set_log_level(LogLevel log_level);

  /// @brief Gets the log verbosity of this Storage instance.
  ///
  /// @returns The current LogLevel.
  LogLevel log_level() const;

#if !defined(SWIG) && !defined(DOXYGEN)
  /// @cond FIREBASE_APP_INTERNAL
  // ABI compatibility: non-const overloads matching the pre-C++17 SDK's
  // exported symbols, so binaries built against the older headers still link.
  // New code resolves to the const overloads above.
  // Defined out-of-line so the symbols are exported from the library.
  ::firebase::App* app();
  std::string url();
  double max_download_retry_time();
  double max_upload_retry_time();
  double max_operation_retry_time();
  /// @endcond
#endif  // !defined(SWIG) && !defined(DOXYGEN)

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class Metadata;
  friend class internal::MetadataInternal;

  Storage(::firebase::App* app, const char* url);
  Storage(const Storage& src) = delete;
  Storage& operator=(const Storage& src) = delete;

  // Destroy the internal_ object.
  void DeleteInternal();

  internal::StorageInternal* internal_ = nullptr;
  /// @endcond
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_H_
