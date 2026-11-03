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

#include "storage/src/desktop/storage_desktop.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_reference_desktop.h"

namespace firebase::storage::internal {

StorageInternal::StorageInternal(App* app, const char* url) : app_(app) {
  if (!app_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->storage = this;
  }

  if (url != nullptr && url[0] != '\0') {
    url_ = url;
    root_ = StoragePath(this, url_);
  } else {
    const char* bucket = app_->options().storage_bucket();
    url_ = (bucket != nullptr && bucket[0] != '\0')
               ? std::string(kGsScheme) + bucket
               : std::string();
    root_ = StoragePath(this, url_);
  }

  firebase::rest::util::Initialize();
  firebase::rest::InitTransportCurl();
  // Spin up the token auto-update thread in Auth.
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);

  std::string sdk;
  std::string version;
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  assert(!sdk.empty() && !version.empty());
  // Strip the common user agent prefix only when it is actually present.
  constexpr size_t kPrefixLen = sizeof(FIREBASE_USER_AGENT_PREFIX) - 1;
  const std::string sdk_name =
      sdk.compare(0, kPrefixLen, FIREBASE_USER_AGENT_PREFIX) == 0
          ? sdk.substr(kPrefixLen)
          : sdk;
  user_agent_ = sdk_name + "/" + version;
}

StorageInternal::~StorageInternal() {
  if (!app_) {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->shutting_down = true;
    lifetime_token_->storage = nullptr;
    return;
  }

  // Signal retry threads that StorageInternal is shutting down.
  {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->shutting_down = true;
    lifetime_token_->cv.notify_all();
  }

  // Cancel any active operations so in-flight requests wake up immediately.
  {
    std::vector<std::shared_ptr<RestOperation::OperationHandle>> handles;
    {
      std::lock_guard<std::mutex> lock(operations_mutex_);
      handles.reserve(operations_.size());
      for (const TrackedOperation& tracked : operations_) {
        if (tracked.operation != nullptr) {
          handles.push_back(tracked.operation->operation_handle());
        }
      }
    }
    for (const auto& handle : handles) {
      if (handle) {
        std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
        if (handle->op != nullptr) {
          handle->op->Cancel();
        }
      }
    }
  }

  // Wait for all detached retry threads to exit, and for every thread that is
  // currently using this object through the lifetime token, before
  // invalidating storage and tearing down FutureManager or curl transport.
  // The current thread is ignored so that a callback that destroys Storage
  // does not deadlock waiting for itself.
  {
    std::unique_lock<std::mutex> lock(lifetime_token_->mutex);
    const std::thread::id self_id = std::this_thread::get_id();
    const auto only_self = [self_id](const std::vector<std::thread::id>& ids) {
      return std::all_of(
          ids.begin(), ids.end(),
          [self_id](const std::thread::id& id) { return id == self_id; });
    };
    lifetime_token_->cv.wait(lock, [this, &only_self]() {
      return only_self(lifetime_token_->retry_threads) &&
             only_self(lifetime_token_->active_users);
    });
    lifetime_token_->storage = nullptr;
  }

  CleanupCompletedOperations();
  cleanup_.CleanupAll();
  CleanupAllOperations();
  firebase::rest::CleanupTransportCurl();
  firebase::rest::util::Terminate();
  // Stop the token auto-update thread in Auth.
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
  app_ = nullptr;
}

// Get a StorageReference to the root of the database.
StorageReferenceInternal* StorageInternal::GetReference() {
  if (!app_) return nullptr;
  {
    MutexLock lock(host_mutex_);
    configured_ = true;
  }
  return new StorageReferenceInternal(root_, this);
}

// Get a StorageReference for the specified path.
StorageReferenceInternal* StorageInternal::GetReference(const char* path) {
  if (!app_ || !path) return nullptr;
  {
    MutexLock lock(host_mutex_);
    configured_ = true;
  }
  return new StorageReferenceInternal(root_.GetChild(path), this);
}

// Get a StorageReference for the provided URL.
StorageReferenceInternal* StorageInternal::GetReferenceFromUrl(
    const char* url) {
  if (!app_ || !url) return nullptr;
  StoragePath parsed_path(this, url);
  if (!parsed_path.IsValid()) {
    LogError("Invalid Storage URL: '%s'", url);
    return nullptr;
  }
  {
    MutexLock lock(host_mutex_);
    configured_ = true;
  }
  return new StorageReferenceInternal(parsed_path, this);
}

// Returns the auth token for the current user, if there is a current user,
// and they have a token, and auth exists as part of the app.
// Otherwise, returns an empty string.
std::string StorageInternal::GetAuthToken() {
  if (!app_) return "";
  std::string result;
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentToken, app(), nullptr, &result);
  return result;
}

// Add an operation to the list of outstanding operations.
uint64_t StorageInternal::AddOperation(RestOperation* operation) {
  if (!operation) return 0;
  CleanupCompletedOperations();
  std::lock_guard<std::mutex> lock(operations_mutex_);
  // Every pointer in operations_ is live, so a match is this very operation.
  auto it = std::find_if(operations_.begin(), operations_.end(),
                         [operation](const TrackedOperation& tracked) {
                           return tracked.operation == operation;
                         });
  if (it != operations_.end()) {
    return it->id;
  }
  const uint64_t id = next_operation_id_++;
  operations_.push_back(TrackedOperation{operation, id});
  return id;
}

// Remove an operation from the list of outstanding operations.
void StorageInternal::RemoveOperation(RestOperation* operation) {
  std::lock_guard<std::mutex> lock(operations_mutex_);
  auto it = std::find_if(operations_.begin(), operations_.end(),
                         [operation](const TrackedOperation& tracked) {
                           return tracked.operation == operation;
                         });
  if (it != operations_.end()) {
    operations_.erase(it);
  }
}

void StorageInternal::DeleteOperation(RestOperation* operation,
                                      uint64_t operation_id) {
  if (!operation || operation_id == 0) return;
  {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    auto it = std::find_if(operations_.begin(), operations_.end(),
                           [operation_id](const TrackedOperation& tracked) {
                             return tracked.id == operation_id;
                           });
    if (it == operations_.end()) {
      // Already deleted, or being deleted by another thread. Do not wait for
      // that deletion here: this runs as the operation's cleanup callback,
      // and the deleting thread's ~RestOperation() blocks in
      // cleanup().UnregisterObject() until this callback returns, so waiting
      // would deadlock. CleanupAllOperations() waits for all in-progress
      // deletions before shutdown proceeds. Ids are never reused, so this can
      // never match a different operation.
      return;
    }
    // Use the tracked pointer: it is guaranteed to be the live operation that
    // was registered under operation_id.
    operation = it->operation;
    operations_.erase(it);
    deleting_operations_.insert(operation_id);
  }
  delete operation;
  std::lock_guard<std::mutex> lock(operations_mutex_);
  deleting_operations_.erase(operation_id);
  operations_cv_.notify_all();
}

// Clean up completed operations.
void StorageInternal::CleanupCompletedOperations() {
  std::vector<TrackedOperation> operations_to_delete;
  {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    for (auto it = operations_.begin(); it != operations_.end();) {
      if (it->operation->is_complete()) {
        operations_to_delete.push_back(*it);
        deleting_operations_.insert(it->id);
        it = operations_.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (const TrackedOperation& tracked : operations_to_delete) {
    delete tracked.operation;
    std::lock_guard<std::mutex> lock(operations_mutex_);
    deleting_operations_.erase(tracked.id);
    operations_cv_.notify_all();
  }
}

void StorageInternal::CleanupAllOperations() {
  while (true) {
    std::vector<TrackedOperation> operations_to_delete;
    {
      std::lock_guard<std::mutex> lock(operations_mutex_);
      for (const TrackedOperation& tracked : operations_) {
        operations_to_delete.push_back(tracked);
        deleting_operations_.insert(tracked.id);
      }
      operations_.clear();
    }
    for (const TrackedOperation& tracked : operations_to_delete) {
      tracked.operation->Cancel();
      delete tracked.operation;
      std::lock_guard<std::mutex> lock(operations_mutex_);
      deleting_operations_.erase(tracked.id);
      operations_cv_.notify_all();
    }
    std::unique_lock<std::mutex> lock(operations_mutex_);
    if (operations_.empty() && deleting_operations_.empty()) {
      break;
    }
    if (operations_.empty()) {
      operations_cv_.wait(lock, [this]() {
        return deleting_operations_.empty() || !operations_.empty();
      });
    }
  }
}

void StorageInternal::UseEmulator(const char* host, int port) {
  if (host == nullptr || host[0] == '\0') {
    LogError("Emulator host cannot be null or empty.");
    return;
  }

  std::string host_str(host);
  const size_t colon_count =
      static_cast<size_t>(std::count(host_str.begin(), host_str.end(), ':'));
  const bool is_bracketed_ipv6 =
      host_str.size() >= 2 && host_str.front() == '[' &&
      host_str.back() == ']' && host_str.find(']') == host_str.size() - 1 &&
      colon_count >= 1;
  const bool is_unbracketed_ipv6 =
      host_str.find('[') == std::string::npos &&
      host_str.find(']') == std::string::npos && colon_count >= 2;
  const bool has_invalid_colon =
      colon_count > 0 && !is_bracketed_ipv6 && !is_unbracketed_ipv6;
  if (host_str.find('/') != std::string::npos ||
      host_str.find(' ') != std::string::npos || has_invalid_colon) {
    LogError("Emulator host '%s' must be a bare hostname or IP address.", host);
    return;
  }

  if (port <= 0 || port > 65535) {
    LogError("Emulator port must be between 1 and 65535.");
    return;
  }

  MutexLock lock(host_mutex_);
  if (configured_) {
    LogError(
        "Cannot connect to emulator after Storage SDK initialization. "
        "Call UseEmulator(host, port) before creating a Storage "
        "reference or trying to load data.");
    return;
  }

  scheme_ = "http";
  port_ = port;
  host_ = is_unbracketed_ipv6 ? ("[" + host_str + "]") : host_str;
}

}  // namespace firebase::storage::internal
