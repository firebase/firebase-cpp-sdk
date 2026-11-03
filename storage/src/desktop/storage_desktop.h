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

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_DESKTOP_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/include/firebase/storage/common.h"

namespace firebase::storage::internal {

class RestOperation;
class StorageReferenceInternal;

class StorageInternal {
 public:
  // Shared lifetime and shutdown token used by async retry threads and REST
  // responses to coordinate safe access to StorageInternal.
  struct LifetimeToken {
    std::mutex mutex;
    std::condition_variable cv;
    StorageInternal* storage = nullptr;
    std::vector<std::thread::id> retry_threads;
    // Threads currently dereferencing `storage` outside of `mutex` (see
    // StorageLifetimeScope below). ~StorageInternal()
    // waits for this to drain (ignoring its own thread) before invalidating
    // `storage`.
    std::vector<std::thread::id> active_users;
    bool shutting_down = false;
  };

  // Build a Storage. A nullptr or empty url uses the default getInstance.
  StorageInternal(App* app, const char* url);
  ~StorageInternal();

  StorageInternal(const StorageInternal&) = delete;
  StorageInternal& operator=(const StorageInternal&) = delete;
  StorageInternal(StorageInternal&&) = delete;
  StorageInternal& operator=(StorageInternal&&) = delete;

  // Get the firebase::App that this Storage was created with.
  ::firebase::App* app() const { return app_; }

  // Return the URL we were created with, if we were created with it explicitly.
  const std::string& url() const { return url_; }

  // Get a StorageReference to the root of the database.
  StorageReferenceInternal* GetReference();

  // Get a StorageReference for the specified path.
  StorageReferenceInternal* GetReference(const char* path);

  // Get a StorageReference for the provided URL.
  StorageReferenceInternal* GetReferenceFromUrl(const char* url);

  // Returns the maximum time (in seconds) to retry a download if a failure
  // occurs.
  double max_download_retry_time() const {
    return max_download_retry_time_.load();
  }

  // Sets the maximum time (in seconds) to retry a download if a failure occurs.
  void set_max_download_retry_time(double max_download_retry_time) {
    max_download_retry_time_.store(max_download_retry_time);
  }

  // Returns the maximum time (in seconds) to retry an upload if a failure
  // occurs.
  double max_upload_retry_time() const { return max_upload_retry_time_.load(); }

  // Sets the maximum time (in seconds) to retry an upload if a failure occurs.
  void set_max_upload_retry_time(double max_upload_retry_time) {
    max_upload_retry_time_.store(max_upload_retry_time);
  }

  // Returns the maximum time (in seconds) to retry operations other than upload
  // and download if a failure occurs.
  double max_operation_retry_time() const {
    return max_operation_retry_time_.load();
  }

  // Sets the maximum time (in seconds) to retry operations other than upload
  // and download if a failure occurs.
  void set_max_operation_retry_time(double max_operation_retry_time) {
    max_operation_retry_time_.store(max_operation_retry_time);
  }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // When this is deleted, it will clean up all StorageReferences and other
  // objects.
  FutureManager& future_manager() { return future_manager_; }
  CleanupNotifier& cleanup() { return cleanup_; }

  // Shared token used to coordinate lifetime with detached retry threads and
  // async responses.
  const std::shared_ptr<LifetimeToken>& lifetime_token() const {
    return lifetime_token_;
  }

  // Fetches the auth token (if available) from app via the function callback
  // registry.  If not available, it returns an empty string.
  std::string GetAuthToken();

  // Get the user agent to send with storage requests.
  const std::string& user_agent() const { return user_agent_; }

  // Add an operation to the list of outstanding operations. Returns a unique,
  // never reused id for the operation (0 if operation is null). The id, not
  // the operation's address, identifies the operation in DeleteOperation() so
  // that a new operation allocated at a recycled address is never confused
  // with a deleted one.
  uint64_t AddOperation(RestOperation* operation);
  // Remove an operation from the list of outstanding operations.
  void RemoveOperation(RestOperation* operation);
  // Delete the operation tracked under operation_id, if it is still tracked
  // (synchronized with CleanupCompletedOperations and CleanupAllOperations to
  // prevent double-free). Returns immediately if the operation was already
  // deleted or is being deleted by another thread; CleanupAllOperations()
  // waits for such in-progress deletions.
  void DeleteOperation(RestOperation* operation, uint64_t operation_id);

  // Configures the Storage SDK to use an emulated backend instead of call the
  // default remote backend
  void UseEmulator(const char* host, int port);

  // Returns the Host for the storage backend
  std::string get_host() const {
    MutexLock lock(host_mutex_);
    return host_;
  }

  // Returns the Port for the storage backend
  int get_port() const {
    MutexLock lock(host_mutex_);
    return port_;
  }

  // Returns the url scheme currently in use for the storage backend
  std::string get_scheme() const {
    MutexLock lock(host_mutex_);
    return scheme_;
  }

 private:
  // Clean up completed operations.
  void CleanupCompletedOperations();
  // Cancel and delete all remaining operations on shutdown.
  void CleanupAllOperations();

  std::shared_ptr<LifetimeToken> lifetime_token_ =
      std::make_shared<LifetimeToken>();
  App* app_ = nullptr;

  FutureManager future_manager_;

  std::string url_;

  std::atomic<double> max_download_retry_time_{600.0};
  std::atomic<double> max_operation_retry_time_{120.0};
  std::atomic<double> max_upload_retry_time_{600.0};
  StoragePath root_;

  CleanupNotifier cleanup_;
  std::string user_agent_;
  std::mutex operations_mutex_;
  std::condition_variable operations_cv_;
  // A live operation and the unique id assigned to it by AddOperation().
  struct TrackedOperation {
    RestOperation* operation;
    uint64_t id;
  };
  // Guarded by operations_mutex_. Operations are removed from operations_
  // (under the lock) before they are deleted, so every pointer in it is live.
  std::vector<TrackedOperation> operations_;
  // Ids of operations removed from operations_ whose deletion is in progress.
  std::unordered_set<uint64_t> deleting_operations_;
  uint64_t next_operation_id_ = 1;
  mutable Mutex host_mutex_;
  std::string host_ = "firebasestorage.googleapis.com";
  std::string scheme_ = "https";
  int port_ = 443;
  bool configured_ = false;
};

// Registers the calling thread in LifetimeToken::active_users for the
// lifetime of the scope. While engaged, ~StorageInternal() blocks (unless it
// runs on this same thread) before it invalidates the token, so storage()
// stays valid for the whole scope, without LifetimeToken::mutex being held
// (holding it would deadlock with anything in the scope that locks it again,
// e.g. ~StorageReferenceInternal()).
class StorageLifetimeScope {
 public:
  enum class Mode {
    // Engage as long as the StorageInternal has not been invalidated, even if
    // it is shutting down (used to complete futures during shutdown).
    kAllowShutdown,
    // Do not engage once shutdown has started (used to start new work).
    kRejectShutdown,
  };

  // Does not engage if `token` is null or has been invalidated (or, with
  // kRejectShutdown, is shutting down); check operator bool before use.
  StorageLifetimeScope(std::shared_ptr<StorageInternal::LifetimeToken> token,
                       Mode mode);
  ~StorageLifetimeScope();

  StorageLifetimeScope(const StorageLifetimeScope&) = delete;
  StorageLifetimeScope& operator=(const StorageLifetimeScope&) = delete;

  explicit operator bool() const { return storage_ != nullptr; }
  StorageInternal* storage() const { return storage_; }

 private:
  std::shared_ptr<StorageInternal::LifetimeToken> token_;
  std::thread::id id_;
  StorageInternal* storage_ = nullptr;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_DESKTOP_H_
