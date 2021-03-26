// Copyright 2017 Google LLC
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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_STORAGE_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_STORAGE_DESKTOP_H_

#include <string>
#include <vector>

#include "app/src/future_manager.h"
#include "app/src/mutex.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage/common.h"

namespace firebase {
namespace storage {
namespace internal {

class RestOperation;

class StorageInternal {
 public:
  // Build a Storage. A nullptr or empty url uses the default getInstance.
  StorageInternal(App* app, const char* url);
  ~StorageInternal();

  // Get the firease::App that this Storage was created with.
  ::firebase::App* app() { return app_; }

  // Return the URL we were created with, if we were created with it explicitly.
  std::string url() { return url_; }

  // Get a StorageReference to the root of the database.
  StorageReferenceInternal* GetReference() const;

  // Get a StorageReference for the specified path.
  StorageReferenceInternal* GetReference(const char* path) const;

  // Get a StorageReference for the provided URL.
  StorageReferenceInternal* GetReferenceFromUrl(const char* url) const;

  // Returns the maximum time (in seconds) to retry a download if a failure
  // occurs.
  double max_download_retry_time() { return max_download_retry_time_; }

  // Sets the maximum time (in seconds) to retry a download if a failure occurs.
  void set_max_download_retry_time(double max_download_retry_time) {
    max_download_retry_time_ = max_download_retry_time;
  }

  // Returns the maximum time (in seconds) to retry an upload if a failure
  // occurs.
  double max_upload_retry_time() { return max_upload_retry_time_; }

  // Sets the maximum time (in seconds) to retry an upload if a failure occurs.
  void set_max_upload_retry_time(double max_upload_retry_time) {
    max_upload_retry_time_ = max_upload_retry_time;
  }

  // Returns the maximum time (in seconds) to retry operations other than upload
  // and download if a failure occurs.
  double max_operation_retry_time() { return max_operation_retry_time_; }

  // Sets the maximum time (in seconds) to retry operations other than upload
  // and download if a failure occurs.
  void set_max_operation_retry_time(double max_operation_retry_time) {
    max_operation_retry_time_ = max_operation_retry_time;
  }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // When this is deleted, it will clean up all StorageReferences and other
  // objects.
  FutureManager& future_manager() { return future_manager_; }
  CleanupNotifier& cleanup() { return cleanup_; }

  // Fetches the auth token (if available) from app via the function callback
  // registry.  If not available, it returns an empty string.
  std::string GetAuthToken();

  // Get the user agent to send with storage requests.
  const std::string& user_agent() const { return user_agent_; }

  // Add an operation to the list of outstanding operations.
  void AddOperation(RestOperation* operation);
  // Remove an operation from the list of outstanding operations.
  void RemoveOperation(RestOperation* operation);

 private:
  // Clean up completed operations.
  void CleanupCompletedOperations();

 private:
  App* app_;

  FutureManager future_manager_;

  std::string url_;

  double max_download_retry_time_;
  double max_operation_retry_time_;
  double max_upload_retry_time_;
  StoragePath root_;

  CleanupNotifier cleanup_;
  std::string user_agent_;
  Mutex operations_mutex_;
  std::vector<RestOperation*> operations_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_STORAGE_DESKTOP_H_
