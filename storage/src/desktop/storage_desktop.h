// Copyright 2017 Google LLC

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
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "storage/src/desktop/callback_dispatcher.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/include/firebase/storage/common.h"

namespace firebase::storage::internal {

class RestOperation;
class StorageReferenceInternal;

class StorageInternal {
 public:
  struct LifetimeToken {
    std::mutex mutex;
    std::condition_variable cv;
    StorageInternal* storage = nullptr;
    std::vector<std::thread::id> retry_threads;
    std::vector<std::thread::id> active_users;
    bool shutting_down = false;
  };

  StorageInternal(App* app, const char* url);
  ~StorageInternal();

  StorageInternal(const StorageInternal&) = delete;
  StorageInternal& operator=(const StorageInternal&) = delete;
  StorageInternal(StorageInternal&&) = delete;
  StorageInternal& operator=(StorageInternal&&) = delete;

  ::firebase::App* app() const { return app_; }
  const std::string& url() const { return url_; }

  StorageReferenceInternal* GetReference();
  StorageReferenceInternal* GetReference(const char* path);
  StorageReferenceInternal* GetReferenceFromUrl(const char* url);

  double max_download_retry_time() const {
    return max_download_retry_time_.load();
  }
  void set_max_download_retry_time(double v) {
    max_download_retry_time_.store(v);
  }
  double max_upload_retry_time() const { return max_upload_retry_time_.load(); }
  void set_max_upload_retry_time(double v) { max_upload_retry_time_.store(v); }
  double max_operation_retry_time() const {
    return max_operation_retry_time_.load();
  }
  void set_max_operation_retry_time(double v) {
    max_operation_retry_time_.store(v);
  }

  bool initialized() const { return app_ != nullptr; }
  CleanupNotifier& cleanup() { return cleanup_; }
  const std::shared_ptr<LifetimeToken>& lifetime_token() const {
    return lifetime_token_;
  }

  std::string GetAuthToken();
  const std::string& user_agent() const { return user_agent_; }

  uint64_t AddOperation(RestOperation* operation);
  void RemoveOperation(RestOperation* operation);
  void DeleteOperation(RestOperation* operation, uint64_t operation_id);
  void UseEmulator(const char* host, int port);

  std::string get_host() const {
    MutexLock lock(host_mutex_);
    return host_;
  }
  int get_port() const {
    MutexLock lock(host_mutex_);
    return port_;
  }
  std::string get_scheme() const {
    MutexLock lock(host_mutex_);
    return scheme_;
  }
  CallbackDispatcher& listener_dispatcher() { return listener_dispatcher_; }

 private:
  void CleanupCompletedOperations();
  void CleanupAllOperations();

  std::shared_ptr<LifetimeToken> lifetime_token_ =
      std::make_shared<LifetimeToken>();
  App* app_ = nullptr;
  std::string url_;
  std::atomic<double> max_download_retry_time_{600.0};
  std::atomic<double> max_operation_retry_time_{120.0};
  std::atomic<double> max_upload_retry_time_{600.0};
  StoragePath root_;
  CleanupNotifier cleanup_;
  std::string user_agent_;
  std::mutex operations_mutex_;
  std::condition_variable operations_cv_;
  struct TrackedOperation {
    RestOperation* operation;
    uint64_t id;
  };
  std::vector<TrackedOperation> operations_;
  std::unordered_set<uint64_t> deleting_operations_;
  uint64_t next_operation_id_ = 1;
  mutable Mutex host_mutex_;
  std::string host_ = "firebasestorage.googleapis.com";
  std::string scheme_ = "https";
  int port_ = 443;
  bool configured_ = false;
  CallbackDispatcher listener_dispatcher_;
};

class StorageLifetimeScope {
 public:
  enum class Mode {
    kAllowShutdown,
    kRejectShutdown,
  };

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
